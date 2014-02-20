/**
this is the wrapper of the native functions 
**/
/*android specific headers*/
#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>
/*standard library*/
#include <time.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
/*ffmpeg headers*/
#include <libavutil/avstring.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>

// #include <libavformat/utils.c>
#include <libavformat/avformat.h>

#include <libswscale/swscale.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/opt.h>
#include <libavcodec/avfft.h>

/*for android logs*/
#define LOG_TAG "FFmpegTest"
#define LOG_LEVEL 10
#define LOGI(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
#define LOGE(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__);}

/*
 * included libs on SO for video compression
#include <jni.h>
#include <android/log.h>
#include <string.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
*/

/**/
char *gFileName;	  //the file name of the video

AVFormatContext *gFormatCtx;
int gVideoStreamIndex;    //video stream index

AVCodecContext *gVideoCodecCtx;

static void get_video_info(char *prFilename);

/*parsing the video file, done by parse thread*/
static void get_video_info(char *prFilename) {
    AVCodec *lVideoCodec;
    int lError;
    /*some global variables initialization*/
    LOGI(10, "get video info starts!");
    /*register the codec*/
    extern AVCodec ff_h263_decoder;
    avcodec_register(&ff_h263_decoder);
    extern AVCodec ff_h264_decoder;
    avcodec_register(&ff_h264_decoder);
    extern AVCodec ff_mpeg4_decoder;
    avcodec_register(&ff_mpeg4_decoder);
    extern AVCodec ff_mjpeg_decoder;
    avcodec_register(&ff_mjpeg_decoder);
    /*register parsers*/
    //extern AVCodecParser ff_h264_parser;
    //av_register_codec_parser(&ff_h264_parser);
    //extern AVCodecParser ff_mpeg4video_parser;
    //av_register_codec_parser(&ff_mpeg4video_parser);
    /*register demux*/
    extern AVInputFormat ff_mov_demuxer;
    av_register_input_format(&ff_mov_demuxer);
    //extern AVInputFormat ff_h264_demuxer;
    //av_register_input_format(&ff_h264_demuxer);
    /*register the protocol*/
    extern URLProtocol ff_file_protocol;
    av_register_protocol2(&ff_file_protocol, sizeof(ff_file_protocol));
    /*open the video file*/
    if ((lError = av_open_input_file(&gFormatCtx, gFileName, NULL, 0, NULL)) !=0 ) {
        LOGE(1, "Error open video file: %d", lError);
        return;	//open file failed
    }
    /*retrieve stream information*/
    if ((lError = av_find_stream_info(gFormatCtx)) < 0) {
        LOGE(1, "Error find stream information: %d", lError);
        return;
    } 
    /*find the video stream and its decoder*/
    gVideoStreamIndex = av_find_best_stream(gFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &lVideoCodec, 0);
    if (gVideoStreamIndex == AVERROR_STREAM_NOT_FOUND) {
        LOGE(1, "Error: cannot find a video stream");
        return;
    } else {
	LOGI(10, "video codec: %s", lVideoCodec->name);
    }
    if (gVideoStreamIndex == AVERROR_DECODER_NOT_FOUND) {
        LOGE(1, "Error: video stream found, but no decoder is found!");
        return;
    }   
    /*open the codec*/
    gVideoCodecCtx = gFormatCtx->streams[gVideoStreamIndex]->codec;
    LOGI(10, "open codec: (%d, %d)", gVideoCodecCtx->height, gVideoCodecCtx->width);
#ifdef SELECTIVE_DECODING
    gVideoCodecCtx->allow_selective_decoding = 1;
#endif
    if (avcodec_open(gVideoCodecCtx, lVideoCodec) < 0) {
	LOGE(1, "Error: cannot open the video codec!");
        return;
    }
    LOGI(10, "get video info ends");
}

JNIEXPORT void JNICALL Java_roman10_ffmpegTest_VideoBrowser_naClose(JNIEnv *pEnv, jobject pObj) {
    int l_mbH = (gVideoCodecCtx->height + 15) / 16;
    /*close the video codec*/
    avcodec_close(gVideoCodecCtx);
    /*close the video file*/
    av_close_input_file(gFormatCtx);
}

JNIEXPORT void JNICALL Java_roman10_ffmpegTest_VideoBrowser_naInit(JNIEnv *pEnv, jobject pObj, jstring pFileName) {
    int l_mbH, l_mbW;
    /*get the video file name*/
    gFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);
    if (gFileName == NULL) {
        LOGE(1, "Error: cannot get the video file name!");
        return;
    } 
    LOGI(10, "video file name is %s", gFileName);
    get_video_info(gFileName);
    LOGI(10, "initialization done");
}

JNIEXPORT jstring JNICALL Java_roman10_ffmpegTest_VideoBrowser_naGetVideoCodecName(JNIEnv *pEnv, jobject pObj) {
    char* lCodecName = gVideoCodecCtx->codec->name;
    return (*pEnv)->NewStringUTF(pEnv, lCodecName);
}

JNIEXPORT jstring JNICALL Java_roman10_ffmpegTest_VideoBrowser_naGetVideoFormatName(JNIEnv *pEnv, jobject pObj) {
    char* lFormatName = gFormatCtx->iformat->name;
    return (*pEnv)->NewStringUTF(pEnv, lFormatName);
}


JNIEXPORT jintArray JNICALL Java_roman10_ffmpegTest_VideoBrowser_naGetVideoResolution(JNIEnv *pEnv, jobject pObj) {
    jintArray lRes;
    lRes = (*pEnv)->NewIntArray(pEnv, 2);
    if (lRes == NULL) {
        LOGI(1, "cannot allocate memory for video size");
        return NULL;
    }
    jint lVideoRes[2];
    lVideoRes[0] = gVideoCodecCtx->width;
    lVideoRes[1] = gVideoCodecCtx->height;
    (*pEnv)->SetIntArrayRegion(pEnv, lRes, 0, 2, lVideoRes);
    return lRes;
}

// FFInputFile functions
// Using these to deduce codec parameters from test file

AVFormatContext* avFormatContextForInputPath(const char *inputPath, const char *inputFormatString){
    // You can override the detected input format
    AVFormatContext *inputFormatContext = NULL;
    AVInputFormat *inputFormat = NULL;
    //AVDictionary *inputOptions = NULL;

    if (inputFormatString) {
        inputFormat = av_find_input_format(inputFormatString);
        // LOGI("avFormatContextForInputPath got inputFormat from string");
        LOGI("avFormatContextForInputPath got inputFormat from string %s ", "hello6");
    }
    // LOGI("avFormatContextForInputPath post av_Find_input_format");
    LOGI("avFormatContextForInputPath post av_Find_input_format %s", "hello7");
    // It's possible to send more options to the parser
    // av_dict_set(&inputOptions, "video_size", "640x480", 0);
    // av_dict_set(&inputOptions, "pixel_format", "rgb24", 0);
    // av_dict_free(&inputOptions); // Don't forget to free

    LOGI("avFormatContextForInputPath pre avformat_open_input path: %s format: %s", inputPath, inputFormatString);
    int openInputResult = avformat_open_input(&inputFormatContext, inputPath, inputFormat, /*&inputOptions*/ NULL);
    // LOGI("avFormatContextForInputPath avformat_open_input result: %d", openInputResult);
    LOGI("avFormatContextForInputPath avformat_open_input result: %s", "hello8");
    if (openInputResult != 0) {
        // LOGE("avformat_open_input failed: %s", stringForAVErrorNumber(openInputResult));
    	LOGE("avformat_open_input failed: %s", "hello10");
        avformat_close_input(&inputFormatContext);
        return NULL;
    }

    int streamInfoResult = avformat_find_stream_info(inputFormatContext, NULL);
    // LOGI("avFormatContextForInputPath avformat_find_stream_info result: %d", streamInfoResult);
    LOGI("avFormatContextForInputPath avformat_find_stream_info result: %s", "hello9");
    if (streamInfoResult < 0) {
        avformat_close_input(&inputFormatContext);
        // LOGE("avformat_find_stream_info failed: %s", stringForAVErrorNumber(openInputResult));
        LOGE("avformat_find_stream_info failed: %s", "hello11");
        return NULL;
    }
    return inputFormatContext;
}

// FFOutputFile functions

AVFormatContext* avFormatContextForOutputPath(const char *path, const char *formatName){
    AVFormatContext *outputFormatContext;
    LOGI("avFormatContextForOutputPath format: %s path: %s", formatName, path);
    int openOutputValue = avformat_alloc_output_context2(&outputFormatContext, NULL, formatName, path);
    if (openOutputValue < 0) {
        avformat_free_context(outputFormatContext);
    }
    return outputFormatContext;
}

int openFileForWriting(AVFormatContext *avfc, const char *path){
    if (!(avfc->oformat->flags & AVFMT_NOFILE)) {
        return avio_open(&avfc->pb, path, AVIO_FLAG_WRITE);
    }
    return -42;
}

int writeFileHeader(AVFormatContext *avfc){
    AVDictionary *options = NULL;

    // Write header for output file
    int writeHeaderResult = avformat_write_header(avfc, &options);
    if (writeHeaderResult < 0) {
        av_dict_free(&options);
    }
    av_dict_free(&options);
    return writeHeaderResult;
}

int writeFileTrailer(AVFormatContext *avfc){
    return av_write_trailer(avfc);
}


void Java_com_example_yourapp_yourJavaClass_compressFile(JNIEnv *env, jobject obj, jstring jInputPath, jstring jInputFormat, jstring jOutputPath, jstring jOutputFormat){
  // One-time FFmpeg initialization
  av_register_all();
  // avformat_network_init();
  avcodec_register_all();

  const char* inputPath = (*env)->GetStringUTFChars(env, jInputPath, NULL);
  const char* outputPath = (*env)->GetStringUTFChars(env, jOutputPath, NULL);
  // format names are hints. See available options on your host machine via $ ffmpeg -formats
  const char* inputFormat = (*env)->GetStringUTFChars(env, jInputFormat, NULL);
  const char* outputFormat = (*env)->GetStringUTFChars(env, jOutputFormat, NULL);

  AVFormatContext *outputFormatContext = avFormatContextForOutputPath(outputPath, outputFormat);
  AVFormatContext *inputFormatContext = avFormatContextForInputPath(inputPath, inputFormat ); //not necessary since file can be inspected

  copyAVFormatContext(&outputFormatContext, &inputFormatContext);
  // Modify outputFormatContext->codec parameters per your liking
  // See http://ffmpeg.org/doxygen/trunk/structAVCodecContext.html

  int result = openFileForWriting(outputFormatContext, outputPath);
  if(result < 0){
      // LOGE("openFileForWriting error: %d", result);
	  LOGE("openFileForWriting error: %s", "hello1");
  }

  writeFileHeader(outputFormatContext);

  // Copy input to output frame by frame
  AVPacket *inputPacket;
  inputPacket = av_malloc(sizeof(AVPacket));

  int continueRecording = 1;
  int avReadResult = 0;
  int writeFrameResult = 0;
  int frameCount = 0;
  while(continueRecording == 1){
      avReadResult = av_read_frame(inputFormatContext, inputPacket);
      frameCount++;
      if(avReadResult != 0){
        if (avReadResult != AVERROR_EOF) {
            // LOGE("av_read_frame error: %s", stringForAVErrorNumber(avReadResult));
        	LOGE("av_read_frame error: %s", "hello 2");
        }else{
            // LOGI("End of input file");
        	LOGI("End of input file %s", "hello3");
        }
        continueRecording = 0;
      }

      AVStream *outStream = outputFormatContext->streams[inputPacket->stream_index];
      writeFrameResult = av_interleaved_write_frame(outputFormatContext, inputPacket);
      if(writeFrameResult < 0){
          // LOGE("av_interleaved_write_frame error: %s", stringForAVErrorNumber(avReadResult));
    	  LOGE("av_interleaved_write_frame error: %s", "hello3");
      }
  }

  // Finalize the output file
  int writeTrailerResult = writeFileTrailer(outputFormatContext);
  if(writeTrailerResult < 0){
      // LOGE("av_write_trailer error: %s", stringForAVErrorNumber(writeTrailerResult));
	  LOGE("av_write_trailer error: %s", "hello4");
  }
  // LOGI("Wrote trailer");
  LOGI("Wrote trailer %s" , "hello5");
}

void copyAVFormatContext(AVFormatContext **dest, AVFormatContext **source){
    int numStreams = (*source)->nb_streams;
    // LOGI("copyAVFormatContext source has %d streams", numStreams);
    LOGI("copyAVFormatContext source has %s streams", "hello12");
    // LOGI("codec_id note: AAC : %d, H264 : %d", CODEC_ID_AAC, CODEC_ID_H264);
    LOGI("codec_id note: AAC : %d, H264 : %s", "hello13");
    // LOGI("sample_fmot note: S16: %d Float: %d", AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_FLT);
    LOGI("sample_fmot note: S16: %d Float: %s", "hello14");
    int i;
    for (i = 0; i < numStreams; i++) {
        // Get input stream
        AVStream *inputStream = (*source)->streams[i];
        AVCodecContext *inputCodecContext = inputStream->codec;

        // Add new stream to output with codec from input stream
        AVStream *outputStream = avformat_new_stream(*dest, inputCodecContext->codec);
        AVCodecContext *outputCodecContext = outputStream->codec;

        // Copy input stream's codecContext for output stream's codecContext
        avcodec_copy_context(outputCodecContext, inputCodecContext);

        LOGI("copyAVFormatContext Copied stream %d with codec_id %i sample_fmt %s", i, inputCodecContext->codec_id, av_get_sample_fmt_name(inputCodecContext->sample_fmt));

    }
}
