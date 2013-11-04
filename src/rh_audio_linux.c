
#include "rh_audio_internal.h"

#include "aout/aout.h"
#include "asmp/asmp.h"
#include "bucket.h"

#include<string.h>
#include<stdio.h>
#include<stdlib.h>

typedef enum {

  PRIV_FLAG_PLAYING = 1,
  PRIV_FLAG_LOOPING = 2,

} priv_flag_enum_t;

struct rh_audiosample_type {

  pthread_mutex_t 		monitor;
  pthread_cond_t		cond;
  rh_audiosample_cb_type 	cb;
  void*				cb_data;
  const char*			src;
  int 				flags;
  int				priv_flags;
  asmp_handle			sample;

};

static bucket_handle channel_bucket = 0;

typedef struct {
  asmp_handle 	sample;
  aout_handle	channel;
} sample_channel_pair_t;
typedef sample_channel_pair_t* sample_channel_pair_ptr;

int rh_audiosample_setup() {

	int err = -1;

	aout_register_default_sample_readfunc( (aout_sample_reader)&asmp_read );
	aout_register_default_sample_resetfunc( (aout_sample_resetter)&asmp_reset );
	aout_register_default_sample_statfunc( (aout_sample_stater)&asmp_stat );

	err = bucket_create( &channel_bucket );

	if( !err )
		err = rh_audiosample_create_internal_bucket();

#ifdef __ANDROID__
	if( !err )
		err = aout_create_OpenSLES();
#else
	if( !err )
		err = aout_create_ALSA();
#endif

	return err;
}

int rh_audiosample_shutdown() {

  sample_channel_pair_ptr * array;
  int len;

  int e0,e1,e2;

  if( bucket_lock( channel_bucket, (void***)&array, &len ) == 0 ) {

	 int i;
	 for(i=0;i<len;i++)
		if(array[i]->channel) {
			aout_close(array[i]->channel);
//			asmp_close(array[i]->sample); // TODO: After ref-counting is implemented!
			free(array[i]);
		}

	  bucket_unlock( channel_bucket );
  }

  e0 = bucket_free( channel_bucket );
  e1 = rh_audiosample_destroy_internal_bucket();

#ifdef __ANDROID__
  e2 = aout_destroy_OpenSLES();
#else
  e2 = aout_destroy_ALSA();
#endif

  channel_bucket = NULL;

  return (e0 | e1 | e2);
}

int rh_audiosample_open	( rh_audiosample_handle * out, const char * source, int flags ) {

  rh_audiosample_handle h = (rh_audiosample_handle)calloc(1,sizeof(struct rh_audiosample_type) );

  if(!h)
	return -1;

  h->flags = flags;

  if(pthread_mutex_init( &h->monitor, NULL ) != 0) {

    free(h);
    return -1;
  }

  if( pthread_cond_init( &h->cond, NULL) != 0 ) {

    pthread_mutex_destroy( &h->monitor );
    free(h);
    return -1;
  }

  if((flags & RH_AUDIOSAMPLE_DONTCOPYSRC)==0) {
    char * _s;
    if((_s = (char*)calloc(1, strlen(source)+1 ))) {
      strcpy(_s, source);
      h->src = _s;

      rh_audiosample_add_to_internal_bucket( h );

	  if( asmp_open( &h->sample, h->src ) == 0 ) {
		*out = h;
		return 0;
	  }
	  free(_s);
    }
  } else {
    h->src = source;
    rh_audiosample_add_to_internal_bucket( h );

	if( asmp_open( &h->sample, h->src ) == 0 ) {
		*out = h;
		return 0;
	}
  }

  pthread_mutex_destroy(&h->monitor);
  pthread_cond_destroy(&h->cond);

  free(h);

  return -1;
}

int rh_audiosample_close( rh_audiosample_handle h ) {

    if(h) {

    	rh_audiosample_remove_from_internal_bucket( h );

    	if(pthread_mutex_lock(&h->monitor)==0)
    		pthread_mutex_unlock(&h->monitor);

    	pthread_mutex_destroy( &h->monitor );

    	if((h->flags & RH_AUDIOSAMPLE_DONTCOPYSRC)==0)
    		free((void*)(h->src));

		asmp_close(h->sample);

    	free(h);
    }
    return 0;
}

static int ensure_sample_open(rh_audiosample_handle h) {

  if(h->sample != NULL)
    return 0;

  if( asmp_open( &h->sample, h->src ) == 0)
    return 0;

  return -1;
}

static int ensure_channel_open(sample_channel_pair_ptr p, asmp_handle sample) {

  int c = asmp_get_channels		( sample );
  int r = asmp_get_samplerate	( sample );
  int s = asmp_get_samplesize	( sample );

  if( p->channel )
	  return aout_reopen( p->channel, c, r, s );
  else
	  return aout_open( &p->channel, c, r, s );
}

static sample_channel_pair_ptr find_sample_from_array( asmp_handle sample, sample_channel_pair_ptr * array, int len ) {

  int i;
  for(i=0;i<len;i++)
    if(array[i]->sample == sample)
      return array[i];
    return NULL;
}

static sample_channel_pair_ptr find_sample_from_bucket( asmp_handle sample ) {

  sample_channel_pair_ptr ret = NULL;
  sample_channel_pair_ptr * array;
  int len;

  if( bucket_lock( channel_bucket, (void***)&array, &len ) == 0 ) {

    int i;
    for(i=0;i<len;i++)
      if(array[i]->sample == sample) {
	ret = array[i];
	break;
      }

    bucket_unlock( channel_bucket );
  }
  return ret;
}

static sample_channel_pair_ptr find_free_channel(sample_channel_pair_ptr * array, int len) {

  int i;
  for(i=0;i<len;i++)
    if(array[i]->sample == NULL)
      return array[i];
  return NULL;
}

static int _cb(aout_handle p, void * samp_data, void * cb_data, aout_cb_event_enum_t ev) {

  rh_audiosample_handle h = (rh_audiosample_handle)( cb_data );

  int e = -1;

    switch(ev) {
      case AOUT_STARTED:
		if(h->cb)
			e = (*(h->cb))(h, h->cb_data, RH_AUDIOSAMPLE_STARTED);
	break;

    case AOUT_STOPPED:
		// wake any threads waiting for this sample to finish
		h->priv_flags = 0;
		pthread_cond_broadcast( &h->cond );
		if(h->cb)
			e = (*(h->cb))(h, h->cb_data, RH_AUDIOSAMPLE_STOPPED);
	break;

	case AOUT_ERROR:
		// wake any threads waiting for this sample to finish
		h->priv_flags = 0;
		pthread_cond_broadcast( &h->cond );
		if(h->cb)
			e = (*(h->cb))(h, h->cb_data, RH_AUDIOSAMPLE_ERROR);
	break;
    }

  return e;
}

int rh_audiosample_wait( rh_audiosample_handle h ) {

  int err = -1;

  if( pthread_mutex_lock(&h->monitor) == 0 ) {

    while( h->priv_flags & ( PRIV_FLAG_LOOPING | PRIV_FLAG_PLAYING ) ) {
      cond_wait_and_unlock_if_cancelled( &h->cond, &h->monitor );
	}

    err = 0;

    pthread_mutex_unlock( &h->monitor );
  }

  return err;
}

static int _rh_audiosample_play( rh_audiosample_handle h, int loop ) {

  int err = -1;

  if( pthread_mutex_lock(&h->monitor) == 0 ) {

    h->priv_flags = 0;

    if( ensure_sample_open(h) == 0) {

      int len;
      sample_channel_pair_ptr * array;

      sample_channel_pair_ptr p = NULL;

      while(!p) {
    	  if( bucket_lock( channel_bucket,(void***)&array,&len ) == 0) {

    		  if( ( p = find_sample_from_array(h->sample, array, len) ) == NULL )
    			  p = find_free_channel(array, len);

    		  bucket_unlock(channel_bucket);

    		  if(!p && bucket_add(channel_bucket, calloc(1, sizeof(sample_channel_pair_t))) != 0)
    			  break;
    	  }
      }

      if(p && (ensure_channel_open(p, h->sample) == 0)) {

    	  p->sample = h->sample;
    	  if(asmp_reset(p->sample) == 0) {
    		  if(aout_register_sample_data(p->channel, p->sample) == 0) {

    			  aout_register_cb(p->channel, &_cb, h);

    			  h->priv_flags |= PRIV_FLAG_PLAYING;
    			  if(loop)
    				  h->priv_flags |= PRIV_FLAG_LOOPING;

    			  if(loop)
    				  err = aout_loop(p->channel);
    			  else
    				  err = aout_start(p->channel);

    			  if( err )
    				  h->priv_flags = 0;
    		  }
    	  }
      }
    }
    pthread_mutex_unlock(&h->monitor);
  }
  return err;
}



int rh_audiosample_play( rh_audiosample_handle h ) {

  return _rh_audiosample_play( h, 0 );
}

int rh_audiosample_loop( rh_audiosample_handle h ) {

  return _rh_audiosample_play( h, 1 );
}

int rh_audiosample_stop( rh_audiosample_handle h ) {

  int err = -1;

  if( pthread_mutex_lock(&h->monitor) == 0 ) {

    sample_channel_pair_ptr p = find_sample_from_bucket( h->sample );

    if(p) {

      err = aout_stop( p->channel );
      aout_register_sample_data( p->channel, NULL );

      p->sample = NULL;
    }

    pthread_mutex_unlock( &h->monitor );
  }

  return err;
}

int rh_audiosample_isplaying ( rh_audiosample_handle h ) {

  int err = -2;

  if( pthread_mutex_lock(&h->monitor) == 0 ) {

	sample_channel_pair_ptr p = find_sample_from_bucket( h->sample );

	err = -1;

    if(p) {
      err = 0;
      if( aout_running( p->channel ) == 1 )
    	  err = 1;
    }
    pthread_mutex_unlock( &h->monitor );
  }

  return err;
}

int rh_audiosample_register_cb	( rh_audiosample_handle h, rh_audiosample_cb_type cb, void * cb_data ) {

  int err = -1;

  if( pthread_mutex_lock( &h->monitor ) == 0 ) {

    h->cb = cb;
    h->cb_data = cb_data;

    pthread_mutex_unlock( &h->monitor );
  }
  return err;
}

