

#include <stdio.h>

#include "alsa_private.h"

static snd_pcm_format_t _determine_format(unsigned int samplesize) {

	switch(samplesize) {
		case 1: return SND_PCM_FORMAT_S8;
		case 2: return SND_PCM_FORMAT_S16_LE;
		case 4: return SND_PCM_FORMAT_S32_LE;
		default:
			return SND_PCM_FORMAT_UNKNOWN;
	}
}

static int _aout_alsa_open(rh_aout_itf self, unsigned int channels, unsigned int samplerate, unsigned int samplesize) {

  struct aout_instance * instance = (struct aout_instance *)self;

  snd_pcm_t * snd_handle;

  snd_pcm_format_t format = _determine_format(samplesize);

  printf("_aout_alsa_open %p %d %d %d\n", self, channels, samplerate, samplesize);

  instance->channels = channels;
  instance->samplerate = samplerate;
  instance->samplesize = samplesize;

  if( snd_pcm_open(&snd_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0 ) {
	printf("snd_pcm_open error\n");
    goto err1;
  }

  instance->handle = snd_handle;

  if( aout_alsa_hw_settings(self, format, channels, samplerate) < 0 ) {
	printf("aout_alsa_hw_settings error\n");
    goto err2;
  }

  if( aout_alsa_sw_settings(self) < 0 ) {
	printf("aout_alsa_sw_settings error\n");
    goto err3;
  }

  if(instance->imp_flags == IMP_FLAG_RW) {
    instance->imp_buffer = (void*)malloc( instance->buffer_size * samplesize );
    if(instance->imp_buffer == NULL) {
	  printf("instance->imp_buffer = malloc(%d) error\n", (int)instance->buffer_size * samplesize );
      goto err3;
	}
  }

  return 0;
err3:
  free(instance->swparams);
err2:
  free(instance->hwparams);
  snd_pcm_close(instance->handle);
err1:
err0:
  return -1;
}

int aout_alsa_open(rh_aout_itf self, uint32_t channels, uint32_t samplerate, uint32_t samplesize) {

	struct aout_instance * instance = (struct aout_instance *)self;

	if( instance && instance->channels == channels && instance->samplerate == samplerate && instance->samplesize == samplesize ) {

		// channel already open, and the correct format
		return 0;
	}
	else if( instance ) {

		// channel open, but the wrong format.
		aout_alsa_close_api_nolock(&self);
	}

	return _aout_alsa_open(self, channels, samplerate, samplesize);
}

