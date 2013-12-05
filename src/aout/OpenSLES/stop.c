
#include "sles_private.h"

int aout_sles_stop( rh_aout_itf self ) {

  struct aout_instance * instance = (struct aout_instance *)self;

  if( ( instance->status_flags & RH_AOUT_STATUS_STOPPED ) == 0 ) {

	rh_asmp_itf audio_sample = instance->audio_sample;

	(*instance->playItf)->SetPlayState(instance->playItf, SL_PLAYSTATE_STOPPED );
	(*instance->bufferQueueItf)->Clear(instance->bufferQueueItf);
	buffer_queue_reset( &instance->bq );

	instance->status_flags = RH_AOUT_STATUS_STOPPED;

    if( audio_sample ) {

		(*audio_sample)->on_output_event( audio_sample, RH_ASMP_OUTPUT_EVENT_STOPPED );
		(*audio_sample)->reset(audio_sample);
	}
  }

  return 0;
}

