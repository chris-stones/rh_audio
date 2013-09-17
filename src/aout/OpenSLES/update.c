
#include "sles.h"
#include <stdio.h>
#include <string.h>

static int enqueue(aout_handle h) {

	int e = -1;

	struct priv_internal * p = get_priv(h);
	buffer_queue_t * bq = &p->bq;

	buffer_t * buffer = buffer_queue_get_drain_buffer(bq);

	if( buffer ) {
		SLresult result;

//		if( buffer->bytes_used < bq->buffersize )
//			memset( ((char*)buffer->buffer) + buffer->bytes_used , 0, bq->buffersize - buffer->bytes_used );

		result = ( *p->bufferQueueItf )->Enqueue(p->bufferQueueItf, buffer->buffer, buffer->bytes_used);

		if( SL_RESULT_SUCCESS != result) {
			// TODO: reclaim the buffer we failed to enqueue.
		}
		else
			e = 0;
	}

	return e;
}

static int load(aout_handle h) {

	struct priv_internal *priv = get_priv(h);

	int total_frames = 0;

	buffer_t * buffer;

	int framesize = priv->channels * priv->samplesize;

	while( ( buffer = buffer_queue_get_fill_buffer( &priv->bq ) ) ) {

		int framesRead = 0;

		char * pBuffer = (char*)buffer->buffer;
		size_t bufferSize = priv->bq.buffersize;

		buffer->bytes_used = 0;

		for(;;) {

			int frames = bufferSize / framesize;

			framesRead = h->samp_reader( h->samp_data, frames, pBuffer, bufferSize );

			if(framesRead > 0) {
				size_t bytes = ( framesize * framesRead );
				pBuffer += bytes;
				bufferSize -= bytes;
				buffer->bytes_used += bytes;
				total_frames += framesRead;
			}

			// buffer is full.
			if(buffer->bytes_used >= priv->bq.buffersize) {
				buffer->bytes_used = priv->bq.buffersize;
				break;
			}

			// end of sample?
			if(framesRead <= 0)
				break;
		}

		if(buffer->bytes_used) {
			buffer_queue_return_fill_buffer( &priv->bq );
			enqueue(h);
		}
		else
			buffer_queue_cancel_fill_buffer( &priv->bq );

		// can't read anymore audio from stream.
		if(framesRead <= 0)
			break;
	}

	return total_frames;
}

static int is_stream_at_end(aout_handle h) {

	if( h->samp_stater( h->samp_data ) & 1 ) // TODO: ENUM STAT MASKS!!! ( 1 == stream at end )
		return 1;

	return 0;
}

static int update(aout_handle h) {

    struct priv_internal *priv = get_priv(h);

    for(;;) {

		if( h->status & AOUT_STATUS_PLAYING ) {

			if( is_stream_at_end( h ) ) {

				int dbiu;

				if(h->status & AOUT_STATUS_LOOPING)
					if( h->samp_resetter( h->samp_data ) == 0 )
						continue;

				dbiu = buffer_queue_drain_buffers_in_use( &priv->bq );

				LOGE("drain buffers in use == %d", dbiu);

				if( dbiu == 0 ) {

					aout_OpenSLES_io_rem( h );
					return aout_stopped( h );
				}

				return 0;
			}

			load( h );

			return 0;
		}
    }

    return 0;
}

int aout_OpenSLES_update(aout_handle h) {

    struct priv_internal *priv = get_priv(h);

    int e = update( h );

    if( ( e == 0 ) && ( h->status & AOUT_STATUS_PLAYING ) ) {

		SLuint32 playState;

		if( SL_RESULT_SUCCESS == (*priv->playItf)->GetPlayState(priv->playItf, &playState) ) {

			if( playState != SL_PLAYSTATE_PLAYING) {

//				(*priv->bufferQueueItf)->Clear(priv->bufferQueueItf);
//				buffer_queue_reset( &priv->bq );

				if( SL_RESULT_SUCCESS != (*priv->playItf)->SetPlayState(priv->playItf, SL_PLAYSTATE_PLAYING) ) {

					e = aout_stopped( h );
				}
			}
		}
	}

    aout_handle_events(h);

    return e;
}
