
#include "sles_private.h"

#include <stdio.h>
#include <string.h>

/*
 * Something went wrong!
 * IF the sample was supposed to be playing, send a stopped event. ( prevent locking up a thread waiting for a sample to finish. )
 * Send an error event, set the channel to free, and stopped.
 */
static int error(rh_aout_itf self) {

	struct aout_instance * instance = (struct aout_instance *)self;

	rh_asmp_itf audio_sample = instance->audio_sample;

	if(audio_sample) {
		if(instance->status_flags & (RH_AOUT_STATUS_PLAYING | RH_AOUT_STATUS_LOOPING) )
			(*audio_sample)->on_output_event(audio_sample, RH_ASMP_OUTPUT_EVENT_STOPPED);

		(*audio_sample)->on_output_event(audio_sample, RH_ASMP_OUTPUT_EVENT_ERROR);
	}

	instance->status_flags = RH_AOUT_STATUS_STOPPED;

	aout_sles_set_sample(self, NULL);

	return -1;
}

static int enqueue(rh_aout_itf self) {

	struct aout_instance * instance = (struct aout_instance *)self;

	int e = -1;

	buffer_queue_t * bq = &instance->bq;

	buffer_t * buffer = buffer_queue_get_drain_buffer(bq);

	if( buffer ) {
		SLresult result;

		result = ( *instance->bufferQueueItf )->Enqueue(instance->bufferQueueItf, buffer->buffer, buffer->bytes_used);

		if( SL_RESULT_SUCCESS != result) {
			// TODO: reclaim the buffer we failed to enqueue.
		}
		else
			e = 0;
	}

	return e;
}

static int load(rh_aout_itf self) {

	struct aout_instance * instance = (struct aout_instance *)self;

	int total_frames = 0;

	buffer_t * buffer;

	int framesize = instance->channels * instance->samplesize;

	while( ( buffer = buffer_queue_get_fill_buffer( &instance->bq ) ) ) {

		int framesRead = 0;

		char * pBuffer = (char*)buffer->buffer;
		size_t bufferSize = instance->bq.buffersize;

		buffer->bytes_used = 0;

		for(;;) {

			int frames = bufferSize / framesize;

			framesRead = (*instance->audio_sample)->read(instance->audio_sample, frames, pBuffer);

			if(framesRead > 0) {
				size_t bytes = ( framesize * framesRead );
				pBuffer += bytes;
				bufferSize -= bytes;
				buffer->bytes_used += bytes;
				total_frames += framesRead;
			}

			// buffer is full.
			if(buffer->bytes_used >= instance->bq.buffersize) {
				buffer->bytes_used = instance->bq.buffersize;
				break;
			}

			// end of sample?
			if(framesRead <= 0)
				break;
		}

		if(buffer->bytes_used) {
			buffer_queue_return_fill_buffer( &instance->bq );
			enqueue(self);
		}
		else
			buffer_queue_cancel_fill_buffer( &instance->bq );

		// can't read anymore audio from stream.
		if(framesRead <= 0)
			break;
	}

	return total_frames;
}

static int update(rh_aout_itf self) {

	struct aout_instance * instance = (struct aout_instance *)self;

    for(;;) {

		if( instance->status_flags & (RH_AOUT_STATUS_PLAYING | RH_AOUT_STATUS_LOOPING ) ) {

			if( aout_sles_atend_sample( self ) ) {

				int dbiu;

				if(instance->status_flags & RH_AOUT_STATUS_LOOPING)
					if( aout_sles_reset_sample( self ) == 0 )
						continue;

				dbiu = buffer_queue_drain_buffers_in_use( &instance->bq );

				if( dbiu == 0 ) {

					return aout_sles_stop( self );
				}

				return 0;
			}

			load( self );

			return 0;
		}
    }

    return 0;
}

int aout_sles_update(rh_aout_itf self) {

	struct aout_instance * instance = (struct aout_instance *)self;

    int e = update( self );

    if( ( e == 0 ) && ( instance->status_flags & (RH_AOUT_STATUS_PLAYING | RH_AOUT_STATUS_LOOPING) ) ) {

		SLuint32 playState;

		if( SL_RESULT_SUCCESS == (*instance->playItf)->GetPlayState(instance->playItf, &playState) ) {

			if( playState != SL_PLAYSTATE_PLAYING) {

				if( SL_RESULT_SUCCESS != (*instance->playItf)->SetPlayState(instance->playItf, SL_PLAYSTATE_PLAYING) ) {

					e = error( self );
				}
			}
		}
	}

    return e;
}

