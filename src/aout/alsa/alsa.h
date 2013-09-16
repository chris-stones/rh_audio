
#pragma once

#include <alsa/asoundlib.h>

#include "../aout_internal.h"

struct priv_internal {

  // handle
  snd_pcm_t *handle;

  int samplerate;
  int samplesize;
  int channels;

  // hardware setings
  snd_pcm_hw_params_t *hwparams;
  unsigned int buffertime;
  snd_pcm_uframes_t buffer_size;
  unsigned int period_time;
  int dir;
  snd_pcm_sframes_t period_size;

  //software settings
  snd_pcm_sw_params_t *swparams;

  unsigned int sleep;

  // my stuff!
  #define IMP_FLAG_MMAP 	1
  #define IMP_FLAG_RW		2
  int 	 imp_flags;
  void * imp_buffer;

};

static inline struct priv_internal * get_priv(aout_handle p) {

  return (struct priv_internal *)p->priv;
}

int aout_alsa_open(aout_handle h, unsigned int channels, unsigned int samplerate, unsigned int samplesize);
int aout_alsa_close(aout_handle h);
int aout_alsa_update(aout_handle h);
int aout_alsa_start( aout_handle h);
int aout_alsa_stop( aout_handle h);

int aout_alsa_io_setup();
int aout_alsa_io_add(aout_handle h);
int aout_alsa_io_rem(aout_handle h);
int aout_alsa_io_teardown();

