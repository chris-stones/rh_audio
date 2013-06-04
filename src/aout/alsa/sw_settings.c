
#include "alsa.h"

int aout_alsa_sw_settings(aout_handle h) {
  
  struct priv_internal *priv = get_priv(h);
  
  if(snd_pcm_sw_params_malloc(&priv->swparams) != 0)
    goto err;
  
  if(snd_pcm_sw_params_current(priv->handle,priv->swparams)<0)
    goto err;

  if(snd_pcm_sw_params_set_start_threshold(priv->handle,priv->swparams,priv->buffer_size-priv->period_size)<0)
    goto err;

  if(snd_pcm_sw_params_set_avail_min(priv->handle, priv->swparams, priv->period_size)<0)
    goto err;

 //if(periodEvent && snd_pcm_sw_params_set_period_event(priv->handle,priv->swparams,1)<0)
 //  okay = false;

//  snd_pcm_uframes_t boundary;
//  if(snd_pcm_sw_params_get_boundary(priv->swparams, &boundary) != 0)
//    okay = false;

// if(1 /* disable_stop_on_under-run ? - keep them interrupts comming! */ ) {
//    if( snd_pcm_sw_params_set_stop_threshold(priv->handle, priv->swparams, boundary) != 0)
//      okay = false;
//  }

//  if( 0 /* fill hardware buffer with silence on under-run */) {
//    if(snd_pcm_sw_params_set_silence_threshold(handle, swparams, 0) != 0)
//      okay = false;
//    if(snd_pcm_sw_params_set_silence_size(handle, swparams, boundary) != 0)
//      okay = false;
//  }

  if(snd_pcm_sw_params(priv->handle, priv->swparams)<0)
    goto err;

  return 0;
err:
  return -1;
}

