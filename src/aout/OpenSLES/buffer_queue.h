
#pragma once

#include<pthread.h>

//#define BUFFER_QUEUE_THREAD_SAFE 1

typedef struct {

	void * buffer;
	size_t bytes_used;

} buffer_t;

typedef struct {

#ifdef BUFFER_QUEUE_THREAD_SAFE
	pthread_mutex_t monitor;
#endif

	int buffersize;

	buffer_t * buffers;
	int nb_buffers;

	buffer_t * drain_buffer;
	buffer_t * fill_buffer;

	int free_drain_buffers;
	int free_fill_buffers;

} buffer_queue_t;

int 				buffer_queue_reset							(buffer_queue_t * bq);
int 				buffer_queue_alloc							(buffer_queue_t * bq, int buffers, int buffersize);
void 				buffer_queue_free							(buffer_queue_t * bq);
int 				buffer_queue_alloc_buffers					(buffer_queue_t * bq);
void 				buffer_queue_free_buffers					(buffer_queue_t * bq);
buffer_t * 			buffer_queue_get_drain_buffer				(buffer_queue_t * bq);
void 				buffer_queue_return_drain_buffer			(buffer_queue_t * bq);
buffer_t * 			buffer_queue_get_fill_buffer				(buffer_queue_t * bq);
void 				buffer_queue_return_fill_buffer				(buffer_queue_t * bq);
void 				buffer_queue_cancel_fill_buffer				(buffer_queue_t * bq);

int 				buffer_queue_drain_buffers_in_use			(buffer_queue_t * bq);

