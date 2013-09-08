
/***
 *
 * Using ffmpeg as a backend.
 *
 */

#include "asmp_internal.h"

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

struct priv_internal {

	AVFormatContext * 	pFormatCtx;
	AVCodecContext  *	pCodecCtx;
	AVCodec			*	pCodec;
	AVFrame			*	pFrame;
	int                 processedSamples;
	int firstAudioStream;

	int stat;
};

static inline struct priv_internal * get_priv(aud_sample_handle p) {

  return (struct priv_internal *)p->priv;
}

static inline AVFormatContext * get_fmt_ctx(aud_sample_handle p) {

  return get_priv(p)->pFormatCtx;
}

static inline AVCodecContext * get_codec_ctx(aud_sample_handle p) {

  return get_priv(p)->pCodecCtx;
}

static int _aud_sample_opener(aud_sample_handle p, const char * const fn) {

  struct priv_internal *priv = calloc(1, sizeof(struct priv_internal));

  if(priv) {

    av_register_all();

	if((priv->pFrame = avcodec_alloc_frame())==NULL) {
		free(priv);
		return -1;
	}

	{
		if(strncmp("rh_rawpak_ctx://",fn,16)==0) {

			void * p = NULL;

			if(sscanf(fn+16,"%p",&p) != 1) {
				av_freep(&priv->pFrame);
				free(priv);
				return -1;
			}

			if(rh_rawpak_open_avformatctx( (rh_rawpak_ctx)p, 0, (void**)&priv->pFormatCtx )!=0) {
				av_freep(&priv->pFrame);
				free(priv);
				return -1;
			}
		}
		else {

			if(avformat_open_input(&priv->pFormatCtx, fn, NULL, NULL)!=0) {
				av_freep(&priv->pFrame);
				free(priv);
				return -1;
			}
		}
	}

    if(avformat_find_stream_info(priv->pFormatCtx, NULL)<0) {
    	av_freep(&priv->pFrame);
		avformat_close_input(&priv->pFormatCtx);
		free(priv);
		return -1;
    }

	{
	int i;
	priv->firstAudioStream = -1;
    for(i=0; i<priv->pFormatCtx->nb_streams; i++)
      if(priv->pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO) {
		 priv->firstAudioStream = i;
		 break;
	  }
	}

	if(priv->firstAudioStream == -1) {
		av_freep(&priv->pFrame);
		avformat_close_input(&priv->pFormatCtx);
		free(priv);
		return -1;
	}

	priv->pCodecCtx=priv->pFormatCtx->streams[priv->firstAudioStream]->codec;

	if((priv->pCodec=avcodec_find_decoder(priv->pCodecCtx->codec_id)) == NULL) {

		avformat_close_input(&priv->pFormatCtx);
		av_freep(&priv->pFrame);
		free(priv);
		return -1;
	}

	if(avcodec_open2(priv->pCodecCtx, priv->pCodec, NULL)<0) {

		avformat_close_input(&priv->pFormatCtx);
		av_freep(&priv->pFrame);
		free(priv);
		return -1;
	}

    p->priv = (void*)priv;

    p->channels 	= priv->pCodecCtx->channels;
//	p->frames		= priv->pCodecCtx->;
    p->samplerate 	= priv->pCodecCtx->sample_rate;

	if( priv->pCodecCtx->sample_fmt != AV_SAMPLE_FMT_S16 )
		printf("ERROR: audio native format is not S16, we will have to re-sample (TODO)\n");

    return 0;
  }

  return -1;
}

static int _ff_read_packet(struct priv_internal * priv) {

	AVPacket packet;

	int err = 0;
	int frameFinished = 0;

	while(av_read_frame(priv->pFormatCtx, &packet) >= 0) {

		if(packet.stream_index == priv->firstAudioStream) {

			if( avcodec_decode_audio4(priv->pCodecCtx, priv->pFrame, &frameFinished, &packet) < 0 )
				err = -1; //
		}

		av_free_packet(&packet);

		if(err || frameFinished)
			return err;
	}

	priv->stat = 1; // SET END OF STREAM

	return -1;
}

static int _aud_sample_reader(aud_sample_handle p, int samples, void * dst, size_t dst_size) {

	int ret = 0;
	struct priv_internal * priv = get_priv(p);

	if(priv->processedSamples >= priv->pFrame->nb_samples) {

		priv->processedSamples = 0;
		_ff_read_packet(priv);
	}

	{
		int samplesRemainingInFrame = priv->pFrame->nb_samples - priv->processedSamples;
		if(samples > samplesRemainingInFrame)
			samples = samplesRemainingInFrame;
	}

	// FIXME - ASSUMING S16
	memcpy( dst,
			((char *)priv->pFrame->data[0]) + (priv->pCodecCtx->channels * 2 * priv->processedSamples),
			samples * priv->pCodecCtx->channels * 2);

	priv->processedSamples += samples;

    return samples;
}

static int _aud_sample_stater(aud_sample_handle p) {

	return get_priv(p)->stat;
}

static int _aud_sample_resetter(aud_sample_handle p) {

	av_seek_frame( get_fmt_ctx(p),get_priv(p)->firstAudioStream,0,0 );

	get_priv(p)->stat = 0;

	return 0;
}

static int _aud_sample_closer(aud_sample_handle p) {

  struct priv_internal * priv = get_priv(p);

  avcodec_close( get_codec_ctx(p) );
  avformat_close_input( &priv->pFormatCtx );
  av_freep(&priv->pFrame);
  free(p->priv);
  p->priv = NULL;

  return 0;
}

int aud_init_interface_ffmpeg(aud_sample_handle p) {

  p->opener = &_aud_sample_opener;
  p->reader = &_aud_sample_reader;
  p->reseter = &_aud_sample_resetter;
  p->closer = &_aud_sample_closer;
  p->stater = &_aud_sample_stater;

  return 0;
}

