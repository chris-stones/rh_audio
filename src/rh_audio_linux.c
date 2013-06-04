
#include "rh_audio_internal.h"

#include<lib/audio/aout/aout.h>
#include<lib/audio/asmp/asmp.h>
#include<lib/util/bucket/bucket.h>

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

static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int isInitialised = 0;

int rh_audiosample_setup() {
 
  int err = -1;
  
  if(isInitialised)
    return 0;
  
  if(pthread_mutex_lock( &init_mutex ) == 0) {
  
    if(isInitialised) {
      
      err = 0;
      
    } else {
      aout_register_default_sample_readfunc( (aout_sample_reader)&asmp_read );
      aout_register_default_sample_seekfunc( (aout_sample_seeker)&asmp_seek );
      aout_register_default_sample_tellfunc( (aout_sample_teller)&asmp_tell );
      aout_register_default_sample_sizefunc( (aout_sample_sizer) &asmp_size );
    
      err = bucket_create( &channel_bucket );
      
      if(!err)
	isInitialised = 1;
    }
    
    pthread_mutex_unlock( &init_mutex );
  }
  
  return err;
}

int rh_audiosample_shutdown() {
  
  int e = bucket_free( channel_bucket );
  
  channel_bucket = NULL;
  
  return e;
}

int rh_audiosample_open	( rh_audiosample_handle * out, const char * source, int flags ) {
  
  rh_audiosample_handle h = (rh_audiosample_handle)calloc(1,sizeof(struct rh_audiosample_type) );
  
  if(!h) return -1;
  
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
      *out = h;
      rh_audiosample_add_to_internal_bucket( h );
      return 0;
    }
  } else {
    h->src = source;
    *out = h;
    rh_audiosample_add_to_internal_bucket( h );
    return 0;
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
      if((h->flags & RH_AUDIOSAMPLE_DONTCOPYSRC)==0) free((void*)(h->src));
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

static int ensure_channel_open(sample_channel_pair_ptr p) {
  
  if( p->channel != NULL )
    return 0;
  
  return aout_open( &p->channel, 2, 44100 );
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

static _cb(aout_handle p, void * samp_data, void * cb_data, aout_cb_event_enum_t ev, ...) {

  rh_audiosample_handle h = (rh_audiosample_handle)( cb_data );
  
  int e = -1;
  
  if(h->cb) {
    switch(ev) {
      case AOUT_STARTED:
	
	e = (*(h->cb))(h, h->cb_data, RH_AUDIOSAMPLE_STARTED);
	break;
	
      case AOUT_STOPPED:
	
	// wake any threads waiting for this sample to finish
	h->priv_flags = 0;
	pthread_cond_broadcast( &h->cond );
	
	e = (*(h->cb))(h, h->cb_data, RH_AUDIOSAMPLE_STOPPED);
	break;
	
      case AOUT_ERROR:
	
	// wake any threads waiting for this sample to finish
	h->priv_flags = 0;
	pthread_cond_broadcast( &h->cond );
	
	e = (*(h->cb))(h, h->cb_data, RH_AUDIOSAMPLE_ERROR);
	break;
    }
  }
  return e;
}

int rh_audiosample_wait( rh_audiosample_handle h ) {
  
  int err = -1;
  
  if( pthread_mutex_lock(&h->monitor) == 0 ) {
    
    while( h->priv_flags & ( PRIV_FLAG_LOOPING | PRIV_FLAG_PLAYING ) )
      pthread_cond_wait( &h->cond, &h->monitor );
    
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
	
      if(p && (ensure_channel_open(p) == 0)) {
	
	p->sample = h->sample;
	if(asmp_seek(p->sample, 0, SEEK_SET) == 0) {
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
  
  int err = -1;
  
  if( pthread_mutex_lock(&h->monitor) == 0 ) {
    
    sample_channel_pair_ptr p = find_sample_from_bucket( h->sample );
    
    if(p) {
      err = 0;
      if( aout_running( p->channel ) == 1 )
	err = 1;
    }
    pthread_mutex_unlock( &h->monitor );
  }
  
  return err;
}

int rh_audiosample_seek ( rh_audiosample_handle h, int offset, int whence ) {
 
  int err = -1;
  
  if( pthread_mutex_lock( &h->monitor ) == 0 ) {
  
    err = asmp_seek( h->sample, offset, whence );
  
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

