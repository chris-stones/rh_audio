
#include "alsa.h"
#include <stdio.h>

int aout_alsa_hw_settings(aout_handle h, snd_pcm_format_t format, unsigned int channels, unsigned int rate);
int aout_alsa_sw_settings(aout_handle h);


static snd_pcm_format_t _determine_format(unsigned int samplesize) {

	switch(samplesize) {
		case 1: return SND_PCM_FORMAT_S8;
		case 2: return SND_PCM_FORMAT_S16_LE;
		case 4: return SND_PCM_FORMAT_S32_LE;
		default:
			return SND_PCM_FORMAT_UNKNOWN;
	}
}

static int _aout_alsa_open(aout_handle h, unsigned int channels, unsigned int samplerate, unsigned int samplesize) {

  snd_pcm_t * snd_handle;

  snd_pcm_format_t format = _determine_format(samplesize);

  struct priv_internal *priv = (struct priv_internal *)calloc(1,sizeof(struct aout_type ));

  priv->channels = channels;
  priv->samplerate = samplerate;
  priv->samplesize = samplesize;

  if(!priv)
    goto err0;

//if( snd_pcm_open(&snd_handle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0) < 0 )
  if( snd_pcm_open(&snd_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0 )
    goto err1;

  priv->handle = snd_handle;

  h->priv = priv;

  if( aout_alsa_hw_settings(h, format, channels, samplerate) < 0 )
    goto err2;

  if( aout_alsa_sw_settings(h) < 0 )
    goto err3;

  if(priv->imp_flags == IMP_FLAG_RW) {
    priv->imp_buffer = (void*)malloc( priv->buffer_size * samplesize );
    if(priv->imp_buffer == NULL)
      goto err3;
  }

  return 0;
err3:
  free(priv->swparams);
err2:
  free(priv->hwparams);
  snd_pcm_close(priv->handle);
err1:
  free(priv);
err0:
  return -1;
}

int aout_alsa_open(aout_handle h, unsigned int channels, unsigned int samplerate, unsigned int samplesize) {

	struct priv_internal *priv = get_priv(h);

	if( priv && priv->channels == channels && priv->samplerate == samplerate && priv->samplesize == samplesize ) {

		// channel already open, and the correct format
		return 0;
	}
	else if( priv ) {

		// channel open, but the wrong format.
		aout_alsa_close( h );
	}

	return _aout_alsa_open(h, channels, samplerate, samplesize);
}

