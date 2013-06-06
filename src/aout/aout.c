
#include "aout_internal.h"

#include<stdlib.h>
#include<pthread.h>
#include<assert.h>

static int _dummy_sample_reader_imp(void *samp_data, int frames, void * p, size_t size) {assert(!__FUNCTION__); return -1;}
static int _dummy_sample_seeker_imp(void *samp_data, int f, int w) {assert(!__FUNCTION__); return -1;}
static int _dummy_sample_teller_imp(void *samp_data) {assert(!__FUNCTION__); return -1;}
static int _dummy_sample_sizer_imp (void *samp_data) {assert(!__FUNCTION__); return -1;}

static aout_sample_reader _default_sample_reader = &_dummy_sample_reader_imp;
static aout_sample_seeker _default_sample_seeker = &_dummy_sample_seeker_imp;
static aout_sample_teller _default_sample_teller = &_dummy_sample_teller_imp;
static aout_sample_sizer  _default_sample_sizer  = &_dummy_sample_sizer_imp;

int aout_register_sample_data(aout_handle p, void * data) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->samp_data = data;
    
    pthread_mutex_unlock(&p->monitor);
    
    return 0;
  }
  
  return -1;
}

void * aout_get_sample_data(aout_handle p) {
  
  void * sd = NULL;
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    sd = p->samp_data;
    
    pthread_mutex_unlock( &p->monitor );
  }
  
  return sd;
}

int aout_register_cb(aout_handle p, aout_cb c, void * cb_data) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->cb = c;
    p->cb_data = cb_data;
    
    pthread_mutex_unlock(&p->monitor);
    
    return 0;
  }
  
  return -1;
}

void * aout_get_cb_data(aout_handle p) {
  
  void * cb_data = NULL;
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    cb_data = p->cb_data;
    
    pthread_mutex_unlock(&p->monitor);
  }
  
  return cb_data;
}

int aout_register_default_sample_readfunc(aout_sample_reader r) {
  
  _default_sample_reader = r;
  return 0;
}

int aout_register_default_sample_seekfunc(aout_sample_seeker s) {
  
  _default_sample_seeker = s;
  return 0;
}

int aout_register_default_sample_tellfunc(aout_sample_teller t) {
  
  _default_sample_teller = t;
  return 0;
}

int aout_register_default_sample_sizefunc(aout_sample_sizer  s) {
  
  _default_sample_sizer = s;
  return 0;
}

int aout_register_sample_readfunc(aout_handle p, aout_sample_reader r) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->samp_reader = r;
    
    pthread_mutex_unlock(&p->monitor);
    
    return 0;
  }
  
  return -1;
}

int aout_register_sample_seekfunc(aout_handle p, aout_sample_seeker s) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->samp_seeker = s;
    
    pthread_mutex_unlock(&p->monitor);
    
    return 0;
  }
  
  return -1;
}

int aout_register_sample_tellfunc(aout_handle p, aout_sample_teller t) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->samp_teller = t;
    
    pthread_mutex_unlock(&p->monitor);
    
    return 0;
  }
  
  return -1;
}

int aout_register_sample_sizefunc(aout_handle p, aout_sample_sizer s) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->samp_sizer = s;
    
    pthread_mutex_unlock(&p->monitor);
    
    return 0;
  }
  
  return -1;
}

static int mutex_init(pthread_mutex_t *mutex, int recursize, int errorcheck, int robust) {
 
  int ret = -1;
  
  pthread_mutexattr_t attr;
  
  if ( pthread_mutexattr_init( &attr ) == 0 ) {
   
    if(recursize)
      pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE);
    else if(errorcheck)
      pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK);
    
    if(robust)
      pthread_mutexattr_setrobust( &attr, PTHREAD_MUTEX_ROBUST );
    else
      pthread_mutexattr_setrobust( &attr, PTHREAD_MUTEX_STALLED );
    
    if ( pthread_mutex_init(mutex, &attr) == 0 )
      ret = 0;
    
    pthread_mutexattr_destroy( &attr );
  }
  
  return ret;
}

static int _aout_open(aout_handle * h, unsigned int chanels, unsigned int rate, int recursive) {
  
  aout_handle p = calloc(1, sizeof( struct aout_type ) );
  
  if(!p)
   goto err0;
  
  aout_init_interface_alsa(p);
  
  p->samp_reader = _default_sample_reader;
  p->samp_seeker = _default_sample_seeker;
  p->samp_teller = _default_sample_teller;
  p->samp_sizer  = _default_sample_sizer;

  if( mutex_init( &p->monitor, recursive, 0, 0 ) != 0)
    goto err1;
  
  if( p->channel_open( p, chanels, rate ) != 0)
    goto err2;
  
  p->ref = 1;

  *h = p;
  
  return 0;
  
err2:
  pthread_mutex_destroy(&p->monitor);
err1:
  free(p);
err0:
  return -1;
}

int aout_open(aout_handle * h, unsigned int chanels, unsigned int rate) {
  
  return _aout_open(h,chanels,rate, 1);
}

aout_handle aout_addref(aout_handle p) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->ref++;
    
    pthread_mutex_unlock(&p->monitor);
    
    return p;
  }
  
  return NULL;
}

int aout_close(aout_handle p) {

  int e = -1;
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->ref--;
   
    pthread_mutex_unlock(&p->monitor);
    
    if(p->ref == 0) {
     
      p->channel_close( p );
      pthread_mutex_destroy(&p->monitor);
      free(p);
    }
  }
  
  return e;
}

int aout_seek(aout_handle p, int frames, int whence) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    int e = p->samp_seeker(p->samp_data, frames, whence);
    
    pthread_mutex_unlock(&p->monitor);
    
    return e;
  }
  
  return -1;
}

int aout_loop(aout_handle p) {
 
  int e = -1;
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->events = 0;
    p->status = 0;
    
    e = p->channel_start( p );
    
    if(p->status & AOUT_STATUS_PLAYING)
      p->status |= AOUT_STATUS_LOOPING;
    
    pthread_mutex_unlock(&p->monitor);
  }
  
  return e;
}

int aout_start(aout_handle p) {
  
  int e = -1;
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->events = 0;
    p->status = 0;
    
    e = p->channel_start( p );
    
    pthread_mutex_unlock(&p->monitor);
  }
  
  return e;
}

int aout_stop(aout_handle p) {

  int e = -1;
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->events = 0;
    
    e = p->channel_stop( p );
    
    pthread_mutex_unlock(&p->monitor);
  }
  
  return e;
}

int aout_running(aout_handle p) {
  
  int e = 0;
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    if( p->status & AOUT_STATUS_PLAYING )
      e = 1;
    
    pthread_mutex_unlock(&p->monitor);
  }
  
  return e;
}

