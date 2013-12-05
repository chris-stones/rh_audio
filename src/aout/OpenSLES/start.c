
#include "sles_private.h"

int aout_sles_play( rh_aout_itf self ) {

  struct aout_instance * instance = (struct aout_instance *)self;

  if( ( instance->status_flags & RH_AOUT_STATUS_PLAYING ) == 0 ) {

	rh_asmp_itf audio_sample = instance->audio_sample;

    if( audio_sample && !(instance->status_flags & RH_AOUT_STATUS_LOOPING))
		(*audio_sample)->on_output_event( audio_sample, RH_ASMP_OUTPUT_EVENT_STARTED );

	instance->status_flags = RH_AOUT_STATUS_PLAYING;
  }

  return 0;
}

int aout_sles_loop( rh_aout_itf self ) {

  struct aout_instance * instance = (struct aout_instance *)self;

  if( ( instance->status_flags & RH_AOUT_STATUS_LOOPING ) == 0 ) {

	rh_asmp_itf audio_sample = instance->audio_sample;

    if( audio_sample && !(instance->status_flags & RH_AOUT_STATUS_PLAYING))
		(*audio_sample)->on_output_event( audio_sample, RH_ASMP_OUTPUT_EVENT_STARTED );

	instance->status_flags = RH_AOUT_STATUS_LOOPING;
  }

  return 0;
}


