
#include "buffer_queue.h"

buffer_t * buffer_queue_get_drain_buffer(buffer_queue_t * bq) {

	if( pthread_mutex_lock(&bq->monitor) == 0) {

		buffer_t * buffer = NULL;

		if(bq->free_drain_buffers ) {

			buffer = bq->drain_buffer;
			bq->free_drain_buffers--;
		}

		pthread_mutex_unlock(&bq->monitor);

		return buffer;
	}

	return bq->drain_buffer; // SHOULDNT HAPPEN
}

void buffer_queue_return_drain_buffer(buffer_queue_t * bq) {

	if( pthread_mutex_lock(&bq->monitor) == 0) {

		if(bq->drain_buffer == bq->buffers[bq->nb_buffers-1])
			bq->drain_buffer = bq->buffers[0];
		else
			bq->drain_buffer++;
		bq->free_fill_buffers++;

		pthread_mutex_unlock(&bq->monitor);
	}
}

buffer_t * buffer_queue_return_and_get_drain_buffer(buffer_queue_t * bq) {

	buffer_t * buffer = NULL;

	if( pthread_mutex_lock(&bq->monitor) == 0) {

		if(bq->free_drain_buffers) {

			if(bq->drain_buffer == bq->buffers[bq->nb_buffers-1])
				bq->drain_buffer = bq->buffers[0];
			else
				bq->drain_buffer++;
			bq->free_fill_buffers++;

			buffer = bq->drain_buffer;
			bq->free_drain_buffers--;
		}
		else
			bq->underflow = 1;

		pthread_mutex_unlock(&bq->monitor);
	}

	return buffer;
}

int buffer_queue_get_underflow(buffer_queue_t * bq) {

	int underflow = 0;

	if( pthread_mutex_lock(&bq->monitor) == 0) {

		underflow = bq->underflow;

		pthread_mutex_unlock(&bq->monitor);
	}
	return underflow;
}

buffer_t * buffer_queue_get_fill_buffer(buffer_queue_t * bq) {

	if( pthread_mutex_lock(&bq->monitor) == 0) {

		buffer_t * buffer = NULL;

		if(bq->free_fill_buffers ) {

			bq->free_fill_buffers--;

			buffer = bq->fill_buffer;
		}

		pthread_mutex_unlock(&bq->monitor);

		return buffer;
	}
	return NULL; // SHOULDNT HAPPEN
}

void buffer_queue_return_fill_buffer(buffer_queue_t * bq) {

	if( pthread_mutex_lock(&bq->monitor) == 0) {

		bq->free_drain_buffers++;

		if(bq->fill_buffer == bq->buffers[bq->nb_buffers-1])
			bq->fill_buffer = bq->buffers[0];
		else
			bq->fill_buffer++;

		bq->underflow = 0;

		pthread_mutex_unlock(&bq->monitor);
	}
}

void buffer_queue_cancel_fill_buffer(buffer_queue_t * bq) {

	if( pthread_mutex_lock(&bq->monitor) == 0) {

		bq->free_fill_buffers++;

		pthread_mutex_unlock(&bq->monitor);
	}
}

int buffer_queue_alloc(buffer_queue_t * bq, int buffers, int buffersize) {

	if(bq) {

		bq->nb_buffers = buffers;
		bq->buffersize = buffersize;

		if(pthread_mutex_init( &bq->monitor, NULL ) != 0)
			return -1;

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
	pthread_mutex_destroy(&bq->monitor);
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

