
/***
 *
 * Using libsndfile as a backend.
 *
 */

#include "asmp_internal.h"

#include<sndfile.h>
#include<stdlib.h>
#include<stdio.h>

struct priv_internal {

  SNDFILE *context;
  SF_INFO sfinfo;
  int stat;
};

static inline struct priv_internal * get_priv(aud_sample_handle p) {

  return (struct priv_internal *)p->priv;
}

static inline SNDFILE * get_ctx(aud_sample_handle p) {

  return get_priv(p)->context;
}

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
//      p->frames	  	= priv->sfinfo.frames;
      p->samplerate 	= priv->sfinfo.samplerate;

      return 0;
    }
    else
      free(priv);
  }

  return -1;
}

static int _aud_sample_reader(aud_sample_handle p, int frames, void * dst, size_t dst_size) {

  int e = sf_readf_short(get_ctx(p), dst, frames);

  if(e<=0)
	  get_priv(p)->stat = 1; // AT END OF STREAM

  return e;
}

//static int _aud_sample_seeker(aud_sample_handle p, int frames, int whence) {
//
//  return sf_seek(get_ctx(p), frames, whence);
//}

static int _aud_sample_resetter(aud_sample_handle p) {

  int e = sf_seek(get_ctx(p), 0, SEEK_SET);

  get_priv(p)->stat = 0; // NOT AT END OF STREAM

  return e;
}

//static int _aud_sample_teller(aud_sample_handle p) {
//
//  return sf_seek(get_ctx(p), 0, SEEK_CUR);
//}

static int _aud_sample_stater(aud_sample_handle p) {

	return get_priv(p)->stat;
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
  p->reseter = &_aud_sample_resetter;
  p->closer = &_aud_sample_closer;
  p->stater = &_aud_sample_stater;

  return 0;
}

