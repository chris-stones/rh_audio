
#pragma once

#include<stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif /** __cplusplus **/

struct aout_type;
typedef struct aout_type * aout_handle;

typedef enum {

  AOUT_STOPPED = (1<<0),
  AOUT_STARTED = (1<<1),
  AOUT_ERROR   = (1<<2)

} aout_cb_event_enum_t;

typedef int(*aout_sample_reader)	(void * samp_data, int frames, void * p, size_t size);
typedef int(*aout_sample_stater)	(void * samp_data);
typedef int(*aout_sample_resetter)	(void * samp_data);
typedef int(*aout_cb)(aout_handle p, void * samp_data, void * cb_data, aout_cb_event_enum_t ev);

int aout_open(aout_handle * h, unsigned int chanels, unsigned int rate);
int aout_open_r(aout_handle * h, unsigned int chanels, unsigned int rate);
aout_handle aout_addref(aout_handle p);
int aout_close(aout_handle p);
int aout_rewind(aout_handle p);
int aout_update(aout_handle p);
int aout_start(aout_handle p);
int aout_loop(aout_handle p);
int aout_stop(aout_handle p);
int aout_running(aout_handle p);

int aout_update_all();
int aout_update_next();

void * aout_get_sample_data(aout_handle h);
void * aout_get_cb_data(aout_handle h);

int aout_register_sample_data(aout_handle p, void * data);
int aout_register_cb(aout_handle h, aout_cb c, void * cb_data);
int aout_register_sample_readfunc(aout_handle p, aout_sample_reader r);
int aout_register_sample_resetfunc(aout_handle p, aout_sample_resetter s);
int aout_register_sample_statfunc(aout_handle p, aout_sample_stater t);

int aout_register_default_sample_readfunc(aout_sample_reader r);
int aout_register_default_sample_resetfunc(aout_sample_resetter s);
int aout_register_default_sample_statfunc(aout_sample_stater t);

#ifdef __cplusplus
} /* extern "C" { */
#endif /** __cplusplus **/

