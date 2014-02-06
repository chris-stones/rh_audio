
#include<stdio.h>

#include "alsa_private.h"

int aout_alsa_sw_settings(rh_aout_itf self) {

  struct aout_instance * instance = (struct aout_instance*)self;

  if(snd_pcm_sw_params_malloc(&instance->swparams) != 0)
    goto err;
  if(snd_pcm_sw_params_current(instance->handle,instance->swparams)<0)
    goto err;
  if(snd_pcm_sw_params_set_start_threshold(instance->handle,instance->swparams,instance->buffer_size-instance->period_size)<0)
    goto err;
  if(snd_pcm_sw_params_set_avail_min(instance->handle, instance->swparams, instance->period_size)<0)
    goto err;
  if(snd_pcm_sw_params(instance->handle, instance->swparams)<0)
    goto err;
  return 0;
err:
  return -1;
}

int aout_alsa_hw_settings(rh_aout_itf self, snd_pcm_format_t format, unsigned int channels, unsigned int rate) {

  struct aout_instance * instance = (struct aout_instance*)self;

  if(snd_pcm_hw_params_malloc(&instance->hwparams) != 0)
    goto err;

  if(snd_pcm_hw_params_any(instance->handle, instance->hwparams) < 0)
    goto err;

  // disable re-sampling.
//  if(snd_pcm_hw_params_set_rate_resample(priv->handle, priv->hwparams, 0) < 0)
//    goto err;

  {
    int mmap_interleaved = snd_pcm_hw_params_test_access(instance->handle,instance->hwparams, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    int rw_interleaved   = snd_pcm_hw_params_test_access(instance->handle,instance->hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);

    printf("aout<alsa>: SND_PCM_ACCESS_MMAP_INTERLEAVED: %s\n", mmap_interleaved == 0 ? "available" : "not available" );
    printf("aout<alsa>: SND_PCM_ACCESS_RW_INTERLEAVED:   %s\n", rw_interleaved ==   0 ? "available" : "not available" );

    if(mmap_interleaved == 0) {
      instance->imp_flags = IMP_FLAG_MMAP;
      if(snd_pcm_hw_params_set_access(instance->handle,instance->hwparams,SND_PCM_ACCESS_MMAP_INTERLEAVED) < 0)
	goto err;
    }
    else if( rw_interleaved == 0) {
      instance->imp_flags = IMP_FLAG_RW;
      if(snd_pcm_hw_params_set_access(instance->handle,instance->hwparams,SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
	goto err;
    }
    else
      goto err;
  }

  // Set sample format
  if(snd_pcm_hw_params_set_format(instance->handle,instance->hwparams,format)<0)
    goto err;

  // Set number of channels
  if(snd_pcm_hw_params_set_channels(instance->handle,instance->hwparams,channels)<0)
    goto err;

  // Set Rate
  if(snd_pcm_hw_params_set_rate(instance->handle,instance->hwparams,rate, 0)<0)
    goto err;

  // Set buffer time
  instance->buffertime = 100000;//300000;//600000;
  if(snd_pcm_hw_params_set_buffer_time_near(instance->handle, instance->hwparams,&instance->buffertime,&instance->dir)<0)
    goto err;

  if(snd_pcm_hw_params_get_buffer_size(instance->hwparams, &instance->buffer_size)<0)
    goto err;

  // Set period time
  instance->period_time = instance->buffertime / 3;
  if(snd_pcm_hw_params_set_period_time_near(instance->handle,instance->hwparams,&instance->period_time, &instance->dir)<0)
    goto err;

  if(snd_pcm_hw_params_get_period_size(instance->hwparams,(snd_pcm_uframes_t*)&instance->period_size,&instance->dir)<0)
    goto err;

  printf("aout<alsa>: buffer time %d, size %d\n", (int)instance->buffertime,  (int)instance->buffer_size);
  printf("aout<alsa>: period time %d, size %d\n", (int)instance->period_time, (int)instance->period_size);

  if(snd_pcm_hw_params(instance->handle,instance->hwparams)<0)
    goto err;

  return 0;
  err:
  return -1;
}

