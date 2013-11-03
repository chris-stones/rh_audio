
#pragma once

#include "asmp.h"

#include <pthread.h>

int aud_init_interface_libsndfile(aud_sample_handle p); // dieing... libsndfile SEEMED better (simpler) than ffmpeg... but it isnt.
int aud_init_interface_ffmpeg(aud_sample_handle p); // use this!
int aud_init_interface_raw(aud_sample_handle p);
int aud_init_interface_s5prom(aud_sample_handle p);

typedef int(*aud_sample_opener)(aud_sample_handle p, const char * const fn);
typedef int(*aud_sample_reader)(aud_sample_handle p, int frames, void * dst, size_t dst_size);
typedef int(*aud_sample_resetter)(aud_sample_handle p);
typedef int(*aud_sample_stater)(aud_sample_handle p);
typedef int(*aud_sample_closer)(aud_sample_handle p);

typedef enum {

  AUD_SAMPLE_SEEKABLE = (1<<0),

} aud_sample_type_flags_t;

struct aud_sample_type {

  int ref;
  pthread_mutex_t monitor;

//  size_t frames;
  int	 channels;
  int	 samplerate;
  int	 samplesize;
  int 	 flags;

  aud_sample_opener opener;
  aud_sample_reader reader;
  aud_sample_resetter reseter;
  aud_sample_stater stater;
  aud_sample_closer closer;
  void * priv;
};

struct aud_output_type {

  int ref_count;
};
