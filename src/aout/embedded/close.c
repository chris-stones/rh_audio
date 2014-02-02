
#include "embedded_private.h"

int aout_embedded_close_api_nolock(rh_aout_itf * self) {

	return 0;
}

int aout_embedded_close(rh_aout_itf * self) {

  struct aout_instance *instance = (struct aout_instance *)(*self);

  if(instance) {

	if(instance->audio_sample)
		(*instance->audio_sample)->close(&instance->audio_sample);

	aout_embedded_close_api_nolock(self);

	free(instance->interface);
	free(instance);

    *self = NULL;
  }
  return 0;
}




