
#include "asmp_internal.h"

#include<sndfile.h>
#include<stdlib.h>
#include<stdio.h>

struct priv_internal {
  
  SNDFILE *context;
  SF_INFO sfinfo;

//  void * file_in_mem;
//  void * file_in_mem_position;
//  size_t file_in_mem_len;
};

static inline struct priv_internal * get_priv(aud_sample_handle p) {
 
  return (struct priv_internal *)p->priv;
}

static inline SNDFILE * get_ctx(aud_sample_handle p) {
 
  return get_priv(p)->context;
}

/*
static inline SF_INFO * get_inf(aud_sample_handle p) {
 
  return &get_priv(p)->sfinfo;
}
*/

static int _aud_sample_opener(aud_sample_handle p, const char * const fn) {
  
  struct priv_internal *priv = calloc(1, sizeof(struct priv_internal));
  
  if(priv) {
   
    priv->context = sf_open(fn, SFM_READ, &priv->sfinfo);
    
    sf_seek(priv->context, 0, SEEK_SET);
    
    if(priv->context) {
      
      p->priv = (void*)priv;
      
      if(priv->sfinfo.seekable) 
	p->flags |= AUD_SAMPLE_SEEKABLE;
      
      p->channels 	= priv->sfinfo.channels;
      p->frames	  	= priv->sfinfo.frames;
      p->samplerate 	= priv->sfinfo.samplerate;
      
      return 0;
    }
    else
      free(priv);
  }
  
  return -1;
}

static int _aud_sample_reader(aud_sample_handle p, int frames, void * dst, size_t dst_size) {
  
  return sf_readf_short(get_ctx(p), dst, frames);
}

static int _aud_sample_seeker(aud_sample_handle p, int frames, int whence) {
  
  return sf_seek(get_ctx(p), frames, whence);
}

static int _aud_sample_teller(aud_sample_handle p) {

  return sf_seek(get_ctx(p), 0, SEEK_CUR);
}

static int _aud_sample_closer(aud_sample_handle p) {

  sf_close(get_ctx(p));
  
  free(p->priv);
  p->priv = NULL;
  
  return 0;
}

int aud_init_interface_libsndfile(aud_sample_handle p) {

  p->opener = &_aud_sample_opener;
  p->reader = &_aud_sample_reader;
  p->seeker = &_aud_sample_seeker;
  p->closer = &_aud_sample_closer;
  p->teller = &_aud_sample_teller;
  
  return 0;
}

