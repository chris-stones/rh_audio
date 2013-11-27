
#include "alsa_private.h"

int aout_alsa_close_api_nolock(rh_aout_itf * self) {

	struct aout_instance *instance = (struct aout_instance *)(*self);

	if(instance) {

	  if(instance->handle)
		snd_pcm_close(instance->handle);
      free(instance->swparams);
      free(instance->hwparams);
      free(instance->imp_buffer);

	  instance->handle = NULL;
	  instance->swparams = NULL;
	  instance->hwparams = NULL;
	  instance->imp_buffer = NULL;
	}

	return 0;
}

int aout_alsa_close(rh_aout_itf * self) {

  struct aout_instance *instance = (struct aout_instance *)(*self);

  if(instance) {

	if(instance->audio_sample)
		(*instance->audio_sample)->close(&instance->audio_sample);

	aout_alsa_close_api_nolock(self);

	free(instance->interface);
	free(instance);

    *self = NULL;
  }
  return 0;
}

