
#include "alsa.h"

#include<stdio.h>

int aout_alsa_hw_settings(aout_handle h, snd_pcm_format_t format, unsigned int channels, unsigned int rate) {
  
  struct priv_internal *priv = get_priv(h);
  
  if(snd_pcm_hw_params_malloc(&priv->hwparams) != 0)
    goto err;

  if(snd_pcm_hw_params_any(priv->handle, priv->hwparams) < 0)
    goto err;

  // disable re-sampling.
//  if(snd_pcm_hw_params_set_rate_resample(priv->handle, priv->hwparams, 0) < 0)
//    goto err;

  // test and set supported access types.
  {
    int mmap_interleaved = snd_pcm_hw_params_test_access(priv->handle,priv->hwparams, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    int rw_interleaved   = snd_pcm_hw_params_test_access(priv->handle,priv->hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    
    printf("aout<alsa>: SND_PCM_ACCESS_MMAP_INTERLEAVED: %s\n", mmap_interleaved == 0 ? "available" : "not available" );
    printf("aout<alsa>: SND_PCM_ACCESS_RW_INTERLEAVED:   %s\n", rw_interleaved ==   0 ? "available" : "not available" );
    
//  mmap_interleaved = -1; // DELETE ME
    
    if(mmap_interleaved == 0) {
      priv->imp_flags = IMP_FLAG_MMAP;
      if(snd_pcm_hw_params_set_access(priv->handle,priv->hwparams,SND_PCM_ACCESS_MMAP_INTERLEAVED) < 0)
	goto err;
    }
    else if( rw_interleaved == 0) {
      priv->imp_flags = IMP_FLAG_RW;
      if(snd_pcm_hw_params_set_access(priv->handle,priv->hwparams,SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
	goto err;
    }
    else 
      goto err;
  }

  // Set sample format
  if(snd_pcm_hw_params_set_format(priv->handle,priv->hwparams,format)<0)
    goto err;

  // Set number of channels
  if(snd_pcm_hw_params_set_channels(priv->handle,priv->hwparams,channels)<0)
    goto err;

  // Set Rate
  if(snd_pcm_hw_params_set_rate(priv->handle,priv->hwparams,rate, 0)<0)
    goto err;

  // Set buffer time
  priv->buffertime = 600000;
  if(snd_pcm_hw_params_set_buffer_time_near(priv->handle, priv->hwparams,&priv->buffertime,&priv->dir)<0)
    goto err;

  if(snd_pcm_hw_params_get_buffer_size(priv->hwparams, &priv->buffer_size)<0)
    goto err;

  // Set period time
  priv->period_time = 200000;
  if(snd_pcm_hw_params_set_period_time_near(priv->handle,priv->hwparams,&priv->period_time, &priv->dir)<0)
    goto err;

  if(snd_pcm_hw_params_get_period_size(priv->hwparams,(snd_pcm_uframes_t*)&priv->period_size,&priv->dir)<0)
    goto err;

  if(snd_pcm_hw_params(priv->handle,priv->hwparams)<0)
    goto err;
  
  return 0;
  err:
  return -1;
}

