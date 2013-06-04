
#pragma once

#include "aout.h"

#include <pthread.h>
#include <string.h>
#include <stdlib.h>

int aout_init_interface_alsa(aout_handle p);

typedef int(*aout_channel_open)(aout_handle p, unsigned int channels, unsigned int rate);
typedef int(*aout_channel_close)(aout_handle p);
typedef int(*aout_channel_start)(aout_handle p);
typedef int(*aout_channel_stop)(aout_handle p);
typedef int(*aout_channel_update)(aout_handle p);

int aout_error(aout_handle h);
int aout_stopped(aout_handle h);
int aout_started(aout_handle h);
int aout_call_callback(aout_handle h, aout_cb_event_enum_t ev);
int aout_handle_events(aout_handle h);

typedef enum {
  
  AOUT_STATUS_ERROR   		=  (1<<0),
  AOUT_STATUS_PLAYING 		=  (1<<1),
  AOUT_STATUS_LOOPING		=  (1<<2)
  
} aout_status_flags_enum_t;

struct aout_type {
  
  int ref;  
  pthread_mutex_t monitor;
  
  void * samp_data;
  aout_sample_reader samp_reader;
  aout_sample_seeker samp_seeker;
  aout_sample_teller samp_teller;
  aout_sample_sizer  samp_sizer;
  aout_cb cb; void * cb_data;
  int cb_depth;
  
  aout_channel_start channel_start;
  aout_channel_stop channel_stop;
  aout_channel_update channel_update;
  aout_channel_open channel_open;
  aout_channel_close channel_close;
  
  int status;
  
  int events;
  
  void * priv;
};

