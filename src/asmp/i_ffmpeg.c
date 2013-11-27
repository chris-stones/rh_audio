
/***
 *
 * Using FFMPEG to decode audio data.
 *
 *      This code supports reading audio from ...
 *        * files on the filesystem.
 *        * files in an android APK.
 *        * files in a rawpak container ( either on filesystem, or android APK )
 *
 * example,
 *
 *     rh_asmp_itf itf;
 *     rh_asmp_create_ffmpeg( &itf, ... );
 *     (*itf)->open ( itf, "sounds/sound0.ogg" );                   // load audio from file sounds/sound0.ogg
 *     (*itf)->openf( itf, "rh_rawpak_ctx://%p", rh_rawpak_ctx  );  // load audio from a rawpak context.
 *     (*itf)->open ( itf, "apk://sounds/sound0.ogg");              // load audio from android assest sounds/sound0.ogg
 */

#include<rh_raw_loader.h>

/*** INCLUDE FFMPEG ***************/
#ifndef UINT64_C
#define UINT64_C(c) (c ## ULL)
#endif
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
} // extern "C" {
#endif
/**********************************/

#ifdef __ANDROID__
	#include<android/asset_manager.h>
	static int apk_open_avformatctx(rh_asmp_itf self, size_t internalBufferSize, AVFormatContext ** format_ctx );
#endif

#include<stdlib.h>
#include<stdio.h>
#include<pthread.h>
#include<stdarg.h>
#include<linux/limits.h>

#include "asmp.h"

struct asmp_instance {

	// interface ptr must be the first item in the instance.
	struct rh_asmp * interface;

	// private data
	asmp_cb_func_t      cb_func;
	void            *   cb_data;
	AVFormatContext * 	pFormatCtx;
	AVCodecContext  *	pCodecCtx;
	AVCodec			*	pCodec;
	AVFrame			*	pFrame;
	int                 processedSamples;
	int firstAudioStream;
	int ate;
	int channels;
	int samplerate;
	int samplesize;
	pthread_mutex_t monitor;
	int ref;

#ifdef __ANDROID__
	struct {
		void  *avformat_buffer;
		size_t avformat_buffer_size;
		AAsset* asset;
		AAssetManager * assetManager;
	} android ;
#endif

};

static int _impl_on_output_event(rh_asmp_itf self, rh_output_event_enum_t ev) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	if( instance->cb_func )
		(*instance->cb_func)(instance->cb_data, ev);
}

static int _impl_open(rh_asmp_itf self, const char * const fn) {

  struct asmp_instance * instance = (struct asmp_instance *)self;

  {
    av_register_all();

	if((instance->pFrame = avcodec_alloc_frame())==NULL) {
		return -1;
	}

	{
		if(strncmp("rh_rawpak_ctx://",fn,16)==0) {

			/* READ audio from a rawpak container, either on disk, or android APK */

			void * p = NULL;

			if(sscanf(fn+16,"%p",&p) != 1) {
				av_freep(&instance->pFrame);
				return -1;
			}

			if(rh_rawpak_open_avformatctx( (rh_rawpak_ctx)p, 0, (void**)&instance->pFormatCtx )!=0) {
				av_freep(&instance->pFrame);
				return -1;
			}
		}
#ifdef __ANDROID__
		else if(strncmp("apk://",fn,6) == 0) {

			/* READ audio from a file in the android APK */

			/*** MEGGA HACK! ***/
			extern AAssetManager * __rh_hack_get_android_asset_manager();

			instance->android.assetManager = __rh_hack_get_android_asset_manager();
			instance->android.asset = AAssetManager_open( instance->android.assetManager, fn+6, AASSET_MODE_RANDOM);

			if(instance->android.asset && apk_open_avformatctx(self, 0, &(instance->pFormatCtx) ) != 0) {

				AAsset_close(instance->android.asset);
				instance->android.asset = NULL;
				instance->android.assetManager = NULL;
				return -1;
			}
		}
#endif /*** __ANDROID__ ***/
		else {

			/* READ audio from a file on the disk */

			if(avformat_open_input(&instance->pFormatCtx, fn, NULL, NULL)!=0) {
				av_freep(&instance->pFrame);
				return -1;
			}
		}
	}

    if(avformat_find_stream_info(instance->pFormatCtx, NULL)<0) {
    	av_freep(&instance->pFrame);
		avformat_close_input(&instance->pFormatCtx);
		return -1;
    }

	{
	int i;
	instance->firstAudioStream = -1;
    for(i=0; i<instance->pFormatCtx->nb_streams; i++)
      if(instance->pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO) {
		 instance->firstAudioStream = i;
		 break;
	  }
	}

	if(instance->firstAudioStream == -1) {
		av_freep(&instance->pFrame);
		avformat_close_input(&instance->pFormatCtx);
		return -1;
	}

	instance->pCodecCtx=instance->pFormatCtx->streams[instance->firstAudioStream]->codec;

	if((instance->pCodec=avcodec_find_decoder(instance->pCodecCtx->codec_id)) == NULL) {

		avformat_close_input(&instance->pFormatCtx);
		av_freep(&instance->pFrame);
		return -1;
	}

	if(avcodec_open2(instance->pCodecCtx, instance->pCodec, NULL)<0) {

		avformat_close_input(&instance->pFormatCtx);
		av_freep(&instance->pFrame);
		return -1;
	}

    instance->channels 		= instance->pCodecCtx->channels;
    instance->samplerate 	= instance->pCodecCtx->sample_rate;
    instance->samplesize	= 2;

    switch(instance->pCodecCtx->sample_fmt) {
    case AV_SAMPLE_FMT_U8:          ///< unsigned 8 bits
    case AV_SAMPLE_FMT_U8P:         ///< unsigned 8 bits, planar
    	instance->samplesize	= 1;
    	break;
    case AV_SAMPLE_FMT_S16:         ///< signed 16 bits
    case AV_SAMPLE_FMT_S16P:        ///< signed 16 bits, planar
    	instance->samplesize	= 2;
    	break;
    case AV_SAMPLE_FMT_S32:         ///< signed 32 bits
    case AV_SAMPLE_FMT_S32P:        ///< signed 32 bits, planar
    	instance->samplesize	= 4;
    	break;
    case AV_SAMPLE_FMT_FLT:         ///< float
    case AV_SAMPLE_FMT_FLTP:        ///< float, planar
    	instance->samplesize	= 4;
    	break;
    case AV_SAMPLE_FMT_DBL:         ///< double
    case AV_SAMPLE_FMT_DBLP:        ///< double, planar
    	instance->samplesize	= 8;
    	break;
    }

	if( instance->pCodecCtx->sample_fmt != AV_SAMPLE_FMT_S16 ) {

		printf("ERROR: audio native format is not S16, we will have to re-sample (TODO)\n");
		avformat_close_input(&instance->pFormatCtx);
		av_freep(&instance->pFrame);

		return -1;
	}

    return 0;
  }

  return -1;
}

static int _impl_openf(rh_asmp_itf self, const char * const format, ...) {

	int err = 0;
	char *path = NULL;
	va_list va;
	va_start(va, format);
    if(!((path = malloc(sizeof (char) * PATH_MAX))))
       err = -1;
    else if(vsnprintf(path,PATH_MAX,format,va)>=PATH_MAX)
        err = -1; /* truncated */
    va_end(va);

	if(!err)
		err = _impl_open(self, path);

	free(path);

	return err;
}

static int _ff_read_packet(rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	AVPacket packet;

	int err = 0;
	int frameFinished = 0;

	while(av_read_frame(instance->pFormatCtx, &packet) >= 0) {

		if(packet.stream_index == instance->firstAudioStream) {

			if( avcodec_decode_audio4(instance->pCodecCtx, instance->pFrame, &frameFinished, &packet) < 0 )
				err = -1; //
		}

		av_free_packet(&packet);

		if(err || frameFinished)
			return err;
	}

	instance->ate= 1; // SET END OF STREAM

	return -1;
}

static int _impl_read(rh_asmp_itf self, int samples, void * dst) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	int ret = 0;

	if(instance->processedSamples >= instance->pFrame->nb_samples) {

		instance->processedSamples = 0;
		_ff_read_packet(self);
	}

	{
		int samplesRemainingInFrame = instance->pFrame->nb_samples - instance->processedSamples;
		if(samples > samplesRemainingInFrame)
			samples = samplesRemainingInFrame;
	}

	memcpy( dst,
			((char *)instance->pFrame->data[0]) + (instance->channels * instance->samplesize * instance->processedSamples),
			samples * instance->channels * instance->samplesize);

	instance->processedSamples += samples;

    return samples;
}

static int _impl_atend(rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	return instance->ate;
}

static int _impl_reset(rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	av_seek_frame( instance->pFormatCtx ,instance->firstAudioStream,0,0 );

	instance->ate = 0;

	return 0;
}

static int _impl_close(rh_asmp_itf *pself) {

  struct asmp_instance * instance = (struct asmp_instance *)(*pself);

  if(instance) {

	  int ref;

	  if( pthread_mutex_lock(&instance->monitor) != 0 )
		  return -1;
	  ref = --(instance->ref);
	  pthread_mutex_unlock(&instance->monitor);

	  if(ref == 0) {

		if(instance->pCodecCtx) 	avcodec_close( instance->pCodecCtx );
		if(instance->pFormatCtx) 	avformat_close_input( &instance->pFormatCtx );
		if(instance->pFrame)		av_freep(&instance->pFrame);

#ifdef __ANDROID__
		if(instance->android.asset)
			AAsset_close(instance->android.asset);
#endif

		free( instance->interface    );
		free( instance );
	  }

	  *pself = NULL;
  }
  return 0;
}

static rh_asmp_itf _impl_addref(rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	if( pthread_mutex_lock(&instance->monitor) != 0 )
		return NULL;

	instance->ref++;

	pthread_mutex_unlock(&instance->monitor);

	return self;
}

int _impl_samplerate( rh_asmp_itf self ) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	return instance->samplerate;
}

int _impl_samplesize( rh_asmp_itf self ) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	return instance->samplesize;
}

int _impl_channels( rh_asmp_itf self ) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	return instance->channels;
}

int rh_asmp_create_ffmpeg( rh_asmp_itf * itf, asmp_cb_func_t cb_func, void * cb_data ) {

	{
		struct asmp_instance * instance  = calloc(1, sizeof( struct asmp_instance ) );
		struct rh_asmp       * interface = calloc(1, sizeof( struct rh_asmp       ) );

		if(!instance || !interface) {
			free(instance);
			free(interface);
			return -1;
		}

		if(pthread_mutex_init(&instance->monitor, NULL)!= 0) {
			free(interface);
			free(instance);
			return -1;
		}

		instance->ref = 1;
		instance->interface = interface;
		instance->cb_func = cb_func;
		instance->cb_data = cb_data;

		interface->open  		= &_impl_open;
		interface->openf        = &_impl_openf;
		interface->addref       = &_impl_addref;
		interface->reset		= &_impl_reset;
		interface->read			= &_impl_read;
		interface->atend 		= &_impl_atend;
		interface->close 		= &_impl_close;
		interface->samplerate 	= &_impl_samplerate;
		interface->samplesize 	= &_impl_samplesize;
		interface->channels		= &_impl_channels;

		interface->on_output_event = &_impl_on_output_event;

		*itf = (rh_asmp_itf)instance;

		return 0;
	}

	return -1;
}


/*************************************** PLAY AUDIO FROM A FILE INSIDE AN APK ***************************************/

#ifdef __ANDROID__

static int apk_read_func(void* ptr, uint8_t* buf, int buf_size)
{
	rh_asmp_itf self = (rh_asmp_itf)ptr;

	struct asmp_instance * instance = (struct asmp_instance *)(self);

	int e = AAsset_read(instance->android.asset, buf, buf_size);

	return (e >= 0) ? e : -1;
}

static int64_t apk_seek_func(void* ptr, int64_t pos, int whence)
{
	rh_asmp_itf self = (rh_asmp_itf)ptr;

	struct asmp_instance * instance = (struct asmp_instance *)(self);

	if(whence == AVSEEK_SIZE)
		return AAsset_getLength(instance->android.asset);

	return AAsset_seek(instance->android.asset, pos, whence);
}

static int apk_open_avformatctx(rh_asmp_itf self, size_t internalBufferSize, AVFormatContext ** format_ctx ) {

	struct asmp_instance * instance = (struct asmp_instance *)(self);

	AVFormatContext* pCtx = NULL;
	AVIOContext* pIOCtx;
	int probedBytes = 0;

	if(!instance || !format_ctx)
		return -1;

	instance->android.avformat_buffer_size = internalBufferSize ? internalBufferSize : (32 * 1024); // 32k default.

	if((instance->android.avformat_buffer = calloc(1, instance->android.avformat_buffer_size)) == NULL)
		return -1;

	pIOCtx = avio_alloc_context(
		(unsigned char*)instance->android.avformat_buffer, instance->android.avformat_buffer_size,
		0,
		self,
		&apk_read_func,
		NULL,
		&apk_seek_func);

	if(!pIOCtx) {
		free(instance->android.avformat_buffer);
		instance->android.avformat_buffer = NULL;
		instance->android.avformat_buffer_size = 0;
		return -1;
	}

	pCtx = avformat_alloc_context();

	if(!pCtx) {
		av_free(pIOCtx);
		free(instance->android.avformat_buffer);
		instance->android.avformat_buffer = NULL;
		instance->android.avformat_buffer_size = 0;
		return -1;
	}

	// Set the IOContext:
	pCtx->pb = pIOCtx;

	// Determining the input format:
	{
		size_t filesize = AAsset_getLength(instance->android.asset);
		probedBytes = filesize > instance->android.avformat_buffer_size ? instance->android.avformat_buffer_size : filesize;
	}

	if(AAsset_read(instance->android.asset, instance->android.avformat_buffer, probedBytes) < 0) {

		av_free(pIOCtx);
		avformat_free_context(pCtx);
		free(instance->android.avformat_buffer);
		instance->android.avformat_buffer = NULL;
		instance->android.avformat_buffer_size = 0;
		return -1;
	}

	AAsset_seek(instance->android.asset, 0, SEEK_SET); // cannot fail.

	AVProbeData probeData;
	probeData.buf = (unsigned char*)instance->android.avformat_buffer;
	probeData.buf_size = probedBytes;
	probeData.filename = "";

	pCtx->iformat = av_probe_input_format(&probeData, 1);

	pCtx->flags = AVFMT_FLAG_CUSTOM_IO;

	if((avformat_open_input(&pCtx, "", 0, 0)) != 0) {
		av_free(pIOCtx);
		avformat_free_context(pCtx);
		free(instance->android.avformat_buffer);
		instance->android.avformat_buffer = NULL;
		instance->android.avformat_buffer_size = 0;
		return -1;
	}

	*format_ctx = pCtx;

	return 0;
}

#endif /*** __ANDROID__ **/

