
#include "asmp_internal.h"

#include<stdlib.h>
#include<pthread.h>

int asmp_open(aud_sample_handle * h, const char * const fn) {
  
  aud_sample_handle p = calloc(1, sizeof( struct aud_sample_type ) );
  
  if(!p)
   goto err0;
  
  if( aud_init_interface_libsndfile( p ) != 0 )
     goto err1;
  
  if( pthread_mutex_init( &p->monitor, NULL ) != 0)
    goto err1;
  
  if(p->opener(p, fn) != 0)
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

aud_sample_handle asmp_addref(aud_sample_handle p) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->ref++;
    
    pthread_mutex_unlock(&p->monitor);
    
    return p;
  }
  
  return NULL;
}

int asmp_close(aud_sample_handle p) {

  int e = -1;
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    p->ref--;
      
    if(p->ref==0)
      e = p->closer(p);
    else
      e = 0;
   
    pthread_mutex_unlock(&p->monitor);
    
    if(p->ref == 0) {
     
      pthread_mutex_destroy(&p->monitor);
      free(p);
    }
  }
  
  return e;
}

int asmp_seek(aud_sample_handle p, int frames, int whence) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    int e = p->seeker(p, frames, whence);
    
    pthread_mutex_unlock(&p->monitor);
    
    return e;
  }
  
  return -1;
}

int asmp_read(aud_sample_handle p, int frames, void * dst, size_t dst_size) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    int e = p->reader(p, frames, dst, dst_size);
    
    pthread_mutex_unlock(&p->monitor);
    
    return e;
  }
  
  return -1;
}

int asmp_get_channels(aud_sample_handle p) {
  
  return p->channels;
}

size_t asmp_size(aud_sample_handle p) {
  
  return p->frames;
}

int asmp_tell(aud_sample_handle p) {
  
  if( pthread_mutex_lock( &p->monitor ) == 0)  {
    
    int e = p->teller(p);
    
    pthread_mutex_unlock(&p->monitor);
    
    return e;
  }
  
  return -1;
}

int asmp_get_samplerate(aud_sample_handle p) {
  
  return p->samplerate;
}

int asmp_get_seekable(aud_sample_handle p) {
  
  return (p->flags & AUD_SAMPLE_SEEKABLE) ? 1 : 0;
}

