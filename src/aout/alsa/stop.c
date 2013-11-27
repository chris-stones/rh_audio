
#include "alsa_private.h"

int aout_alsa_stop( rh_aout_itf self ) {

  struct aout_instance * instance = (struct aout_instance *)self;

  if( ( instance->status_flags & RH_AOUT_STATUS_STOPPED ) == 0 ) {

	rh_asmp_itf audio_sample = instance->audio_sample;

    snd_pcm_drop( instance->handle );

	instance->status_flags = RH_AOUT_STATUS_STOPPED;

    if( audio_sample ) {

		(*audio_sample)->on_output_event( audio_sample, RH_ASMP_OUTPUT_EVENT_STOPPED );
		(*audio_sample)->reset(audio_sample);
	}
  }

  return 0;
}

