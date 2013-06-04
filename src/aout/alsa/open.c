
#include "alsa.h"
#include <stdio.h>

int aout_alsa_hw_settings(aout_handle h, snd_pcm_format_t format, unsigned int channels, unsigned int rate);
int aout_alsa_sw_settings(aout_handle h);

int aout_alsa_open(aout_handle h, unsigned int channels, unsigned int rate) {

  snd_pcm_t * snd_handle;
  
  snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
  
  struct priv_internal *priv = (struct priv_internal *)calloc(sizeof(struct aout_type ),1);
    
  if(!priv)
    goto err0;
  
//if( snd_pcm_open(&snd_handle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0) < 0 )
  if( snd_pcm_open(&snd_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0 )
    goto err1;
  
  priv->handle = snd_handle;
  
  h->priv = priv;
  
  if( aout_alsa_hw_settings(h, format, channels, rate) < 0 ) 
    goto err2;
  
  if( aout_alsa_sw_settings(h) < 0 )
    goto err3;
  
  if(priv->imp_flags == IMP_FLAG_RW) {
    priv->imp_buffer = (void*)malloc( priv->buffer_size * 2 );// FIXME assuming framesize 2
    
    printf("allocated %d bytes for buffer\n", (int)(priv->buffer_size * 2));
    
    if(priv->imp_buffer == NULL)
      goto err3;
  }
  
  return 0;
err3:
  free(priv->swparams);
err2:
  free(priv->hwparams);
//  printf("free priv->handle %p\n",priv->handle);
  snd_pcm_close(priv->handle);
err1:
  free(priv);
err0:
  return -1;
}

