
/***
 *
 * Stream data from ffmpeg
 *
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

#include<stdlib.h>
#include<stdio.h>
#include<pthread.h>

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
		else {

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

	if( instance->pCodecCtx->sample_fmt != AV_SAMPLE_FMT_S16 )
		printf("ERROR: audio native format is not S16, we will have to re-sample (TODO)\n");

    return 0;
  }

  return -1;
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

