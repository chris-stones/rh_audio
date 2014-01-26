
#pragma once

#include "alsa.h"

int rh_aout_create_alsa( rh_aout_itf * itf );

int aout_alsa_open(rh_aout_itf self, uint32_t channels, uint32_t samplerate, uint32_t samplesize, uint32_t bigendian);
int aout_alsa_update(rh_aout_itf self);
int aout_alsa_close(rh_aout_itf * self);
int aout_alsa_hw_settings(rh_aout_itf self, snd_pcm_format_t format, unsigned int channels, unsigned int rate);
int aout_alsa_sw_settings(rh_aout_itf self);
int aout_alsa_play( rh_aout_itf self );
int aout_alsa_loop( rh_aout_itf self );
int aout_alsa_stop( rh_aout_itf self );

int aout_alsa_set_sample(rh_aout_itf self, rh_asmp_itf  sample);
int aout_alsa_get_sample(rh_aout_itf self, rh_asmp_itf *sample);
int aout_alsa_read_sample(rh_aout_itf self, int frames, void * buffer);
int aout_alsa_atend_sample(rh_aout_itf self);
int aout_alsa_reset_sample(rh_aout_itf self);

int aout_alsa_close_api_nolock(rh_aout_itf * self);

typedef enum {

	RH_AOUT_STATUS_STOPPED = 1<<0,
	RH_AOUT_STATUS_PLAYING = 1<<1,
	RH_AOUT_STATUS_LOOPING = 1<<2,

} rh_aout_alsa_status_flags;

struct aout_instance {

  // interface ptr must be the first item in the instance.
  struct rh_aout * interface;

  // private data //

  // audio-sample ( may be NULL if channel is free )
  rh_asmp_itf audio_sample;

  // handle
  snd_pcm_t *handle;

  int samplerate;
  int samplesize;
  int channels;
  int bigendian;

  // hardware setings
  snd_pcm_hw_params_t *hwparams;
  unsigned int buffertime;
  snd_pcm_uframes_t buffer_size;
  unsigned int period_time;
  int dir;
  snd_pcm_sframes_t period_size;

  //software settings
  snd_pcm_sw_params_t *swparams;

  // finished sending audio-data to ALSA, re-awaken after 'sleep' time and send finished playing event.
  unsigned int sleep;

  // audio upload mode...
  #define IMP_FLAG_MMAP 	1 // ALSA support direct memory access via mmap
  #define IMP_FLAG_RW		2 // ALSA doesnt support direct memory access, we need a buffer :(
  int 	 imp_flags;
  void * imp_buffer;

  int status_flags;
};

