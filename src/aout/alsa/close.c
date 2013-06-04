
#include "alsa.h"

int aout_alsa_close(aout_handle h) {

  struct priv_internal *priv = get_priv(h);
  
  if(priv) {
    snd_pcm_close(priv->handle);
    free(priv->swparams);
    free(priv->hwparams);
    free(priv->imp_buffer);
    free(priv);
    h->priv = NULL;
  }
  return 0;
}

