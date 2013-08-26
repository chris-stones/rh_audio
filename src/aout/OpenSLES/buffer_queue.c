
#include "buffer_queue.h"

//#define BUFFER_QUEUE_THREAD_SAFE 1

static inline int mutex_lock(pthread_mutex_t * m) {

#if defined(BUFFER_QUEUE_THREAD_SAFE)
	return pthread_mutex_lock(m);
#else
	return 0;
#endif
}

static inline int mutex_unlock(pthread_mutex_t * m) {

#if defined(BUFFER_QUEUE_THREAD_SAFE)
	return pthread_mutex_unlock(m);
#else
	return 0;
#endif
}

int buffer_queue_drain_buffers_in_use(buffer_queue_t * bq) {

	int i = 0;

	if( mutex_lock( &bq->monitor ) == 0 ) {

		i = bq->nb_buffers - bq->free_drain_buffers;

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

			if(bq->drain_buffer == bq->buffers[bq->nb_buffers-1])
				bq->drain_buffer = bq->buffers[0];
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

		return buffer;
	}
	return NULL; // SHOULDNT HAPPEN
}

void buffer_queue_return_fill_buffer(buffer_queue_t * bq) {

	if( mutex_lock(&bq->monitor) == 0) {

		bq->free_drain_buffers++;

		if(bq->fill_buffer == bq->buffers[bq->nb_buffers-1])
			bq->fill_buffer = bq->buffers[0];
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
				free(bq->buffers[i]);
			free(bq->buffers);
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

	if( ( bq->buffers = calloc( bq->nb_buffers, sizeof( buffer_t * ) ) ) ) {

		int i = 0;

		for(i=0;i<bq->nb_buffers;i++) {

			bq->buffers[i] = calloc(1, bq->buffersize);

			if(!bq->buffers[i]) {
				for(i=0;i<bq->nb_buffers;i++)
					free(bq->buffers[i]);
				free(bq->buffers);
				return -1;
			}
		}

		bq->drain_buffer =
		bq->fill_buffer =
		bq->buffers[0];

		bq->free_drain_buffers = 0;
		bq->free_fill_buffers = bq->nb_buffers;

		return 0;
	}
	return -1;
}

