
#include "buffer_queue.h"
#include "sles.h"

#if defined(BUFFER_QUEUE_THREAD_SAFE)
#define mutex_lock(M) 	pthread_mutex_lock(M);
#define mutex_unlock(M) pthread_unmutex_lock(M);
#else
#define mutex_lock(M) 0
#define mutex_unlock(M)
#endif

static void _buffer_queue_reset(buffer_queue_t * bq) {

	bq->drain_buffer =
	bq->fill_buffer =
	bq->buffers;

	bq->free_drain_buffers = 0;
	bq->free_fill_buffers = bq->nb_buffers;
}

int buffer_queue_reset(buffer_queue_t * bq) {

	int i = -1;

	if( mutex_lock( &bq->monitor ) == 0 ) {

		_buffer_queue_reset(bq);

		mutex_unlock( &bq->monitor );

		i = 0;
	}

	return i;
}

int buffer_queue_drain_buffers_in_use(buffer_queue_t * bq) {

	int i = -1;

	if( mutex_lock( &bq->monitor ) == 0 ) {

		i = bq->nb_buffers - bq->free_fill_buffers;

		mutex_unlock( &bq->monitor );
	}

	return i;
}

buffer_t * buffer_queue_get_drain_buffer(buffer_queue_t * bq) {

	if( mutex_lock(&bq->monitor) == 0) {

		buffer_t * buffer = NULL;

		if(bq->free_drain_buffers ) {

			buffer = bq->drain_buffer;
			bq->free_drain_buffers--;

			LOGE("buffer_queue_get_drain_buffer %d", bq->free_drain_buffers);

			if(bq->drain_buffer == ( bq->buffers + (bq->nb_buffers-1)))
				bq->drain_buffer = bq->buffers;
			else
				bq->drain_buffer++;
		}

		mutex_unlock(&bq->monitor);

		return buffer;
	}

	return bq->drain_buffer; // SHOULDNT HAPPEN
}

void buffer_queue_return_drain_buffer(buffer_queue_t * bq) {

	if( mutex_lock(&bq->monitor) == 0) {

		bq->free_fill_buffers++;

		mutex_unlock(&bq->monitor);
	}
}
buffer_t * buffer_queue_get_fill_buffer(buffer_queue_t * bq) {

	if( mutex_lock(&bq->monitor) == 0) {

		buffer_t * buffer = NULL;

		if(bq->free_fill_buffers ) {

			bq->free_fill_buffers--;

			buffer = bq->fill_buffer;
		}

		mutex_unlock(&bq->monitor);

//		if(buffer) {
//			LOGE("buffer_queue_get_fill_buffer returning buffer with %p", buffer->buffer);
//		}
//		else {
//			LOGE("buffer_queue_get_fill_buffer returning NULL BUFFER");
//		}

		return buffer;
	}
//	LOGE("SHOULDNT HAPPEN %s %d", __FUNCTION__, __LINE__);
	return NULL; // SHOULDNT HAPPEN
}

void buffer_queue_return_fill_buffer(buffer_queue_t * bq) {

	if( mutex_lock(&bq->monitor) == 0) {

		bq->free_drain_buffers++;

		LOGE("buffer_queue_return_fill_buffer %d", bq->free_drain_buffers);

		if(bq->fill_buffer == (bq->buffers + (bq->nb_buffers-1) ) )
			bq->fill_buffer = bq->buffers;
		else
			bq->fill_buffer++;

		mutex_unlock(&bq->monitor);
	}
}

void buffer_queue_cancel_fill_buffer(buffer_queue_t * bq) {

	if( mutex_lock(&bq->monitor) == 0) {

		bq->free_fill_buffers++;

		mutex_unlock(&bq->monitor);
	}
}

int buffer_queue_alloc(buffer_queue_t * bq, int buffers, int buffersize) {

	if(bq) {

		bq->nb_buffers = buffers;
		bq->buffersize = buffersize;

#if defined(BUFFER_QUEUE_THREAD_SAFE)
		if(pthread_mutex_init( &bq->monitor, NULL ) != 0)
			return -1;
#endif

		return 0;
	}

	return -1;
}

void buffer_queue_free_buffers(buffer_queue_t * bq) {

	if(bq) {
		if(bq->buffers) {
			int i;
			for( i=0; i<bq->nb_buffers; i++ )
				free(bq->buffers[i].buffer);
			free(bq->buffers);
			bq->buffers = NULL;
		}
	}
}

void buffer_queue_free(buffer_queue_t * bq) {

	buffer_queue_free_buffers(bq);
#if defined(BUFFER_QUEUE_THREAD_SAFE)
	pthread_mutex_destroy(&bq->monitor);
#endif
}

int buffer_queue_alloc_buffers(buffer_queue_t * bq) {

	if( ( bq->buffers = calloc( bq->nb_buffers, sizeof( buffer_t ) ) ) ) {

		int i = 0;

		for(i=0;i<bq->nb_buffers;i++) {

			bq->buffers[i].buffer = calloc(1, bq->buffersize);
//			LOGE("buffer_queue_alloc_buffers bq->buffers[%d] = %p",i,bq->buffers[i].buffer);

			if(!bq->buffers[i].buffer) {

				for(i=0;i<bq->nb_buffers;i++)
					free(bq->buffers[i].buffer);
				free(bq->buffers);

//				LOGE("buffer_queue_alloc_buffers ERROR");
				return -1;
			}
		}

		_buffer_queue_reset(bq);

		return 0;
	}

	LOGE("buffer_queue_alloc_buffers ERROR");
	return -1;
}

