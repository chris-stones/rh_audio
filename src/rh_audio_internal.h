
#pragma once

#include <pthread.h>

#include "rh_audio.h"

#ifdef __cplusplus
extern "C" {
#endif
  
static inline int cond_wait_and_unlock_if_cancelled(pthread_cond_t * cond, pthread_mutex_t * mutex) {
 
  int err;
  typedef void (*routine)(void *);
  routine r = (routine)pthread_mutex_unlock;
  pthread_cleanup_push( r, (void*)mutex );
  err = pthread_cond_wait(cond, mutex);
  pthread_cleanup_pop(0);
  return err;
}

static inline int cond_timedwait_and_unlock_if_cancelled(pthread_cond_t * cond, pthread_mutex_t * mutex, struct timespec * abstime) {
  
  typedef void (*routine)(void *);
  routine r = (routine)pthread_mutex_unlock;
  int err;
  pthread_cleanup_push( r, (void*)mutex );
  err = pthread_cond_timedwait(cond, mutex, abstime);
  pthread_cleanup_pop(0);
  return err;
}

int rh_audiosample_create_internal_bucket();
int rh_audiosample_destroy_internal_bucket();
int rh_audiosample_add_to_internal_bucket	( rh_audiosample_handle sample );
int rh_audiosample_remove_from_internal_bucket	( rh_audiosample_handle sample );

#ifdef __cplusplus
} // extern "C" {
#endif

