
#include "aout_internal.h"

#include<stdlib.h>
#include<pthread.h>
#include<assert.h>

static int _dummy_sample_reader_imp		(void *samp_data, int frames, void * p, size_t size) 	{assert(!__FUNCTION__); return -1;}
static int _dummy_sample_resetter_imp	(void *samp_data) 										{assert(!__FUNCTION__); return -1;}
static int _dummy_sample_stater_imp		(void *samp_data) 										{assert(!__FUNCTION__); return -1;}

static aout_sample_reader 	_default_sample_reader 		= &_dummy_sample_reader_imp;
static aout_sample_resetter _default_sample_resetter 	= &_dummy_sample_resetter_imp;
static aout_sample_stater 	_default_sample_stater 		= &_dummy_sample_stater_imp;

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

int aout_register_default_sample_resetfunc(aout_sample_resetter s) {

  _default_sample_resetter = s;
  return 0;
}

int aout_register_default_sample_statfunc(aout_sample_stater t) {

  _default_sample_stater = t;
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

int aout_register_sample_resetfunc(aout_handle p, aout_sample_resetter s) {

  if( pthread_mutex_lock( &p->monitor ) == 0)  {

    p->samp_resetter = s;

    pthread_mutex_unlock(&p->monitor);

    return 0;
  }

  return -1;
}

int aout_register_sample_statfunc(aout_handle p, aout_sample_stater t) {

  if( pthread_mutex_lock( &p->monitor ) == 0)  {

    p->samp_stater = t;

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

#ifdef __ANDROID__
    assert(!robust && "Android does not support robust mutexes");
#else
    if(robust)
      pthread_mutexattr_setrobust( &attr, PTHREAD_MUTEX_ROBUST );
    else
      pthread_mutexattr_setrobust( &attr, PTHREAD_MUTEX_STALLED );
#endif

    if ( pthread_mutex_init(mutex, &attr) == 0 )
      ret = 0;

    pthread_mutexattr_destroy( &attr );
  }

  return ret;
}

static int _aout_open(aout_handle * h, unsigned int chanels, unsigned int rate, unsigned int samplesize, int recursive) {

  aout_handle p = calloc(1, sizeof( struct aout_type ) );

  if(!p)
   goto err0;

  // Configure audio output backend.
#ifdef __ANDROID__
  aout_init_interface_OpenSLES(p);
#else
  aout_init_interface_ALSA(p);
#endif

  // Set default audio input backend.
  p->samp_reader = _default_sample_reader;
  p->samp_resetter = _default_sample_resetter;
  p->samp_stater = _default_sample_stater;

  if( mutex_init( &p->monitor, recursive, 0, 0 ) != 0)
    goto err1;

  if( p->channel_open( p, chanels, rate, samplesize ) != 0)
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

static int _aout_reopen(aout_handle h, unsigned int chanels, unsigned int rate, unsigned int samplesize) {

	return h->channel_open(h, chanels, rate, samplesize );
}

int aout_open(aout_handle * h, unsigned int chanels, unsigned int rate, unsigned int samplesize) {

  return _aout_open(h,chanels,rate, samplesize, 1);
}

int aout_reopen(aout_handle h, unsigned int chanels, unsigned int rate, unsigned int samplesize) {

 return _aout_reopen(h,chanels,rate, samplesize, 1);
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

int aout_rewind(aout_handle p) {

  if( pthread_mutex_lock( &p->monitor ) == 0)  {

	int e = p->samp_resetter(p->samp_data);

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

