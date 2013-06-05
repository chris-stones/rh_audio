
#pragma once

#include<pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bucket_type;
typedef struct bucket_type * bucket_handle;

int bucket_create(bucket_handle * out);
int bucket_free( bucket_handle h );
int bucket_add(bucket_handle bucket, void * data);
int bucket_remove(bucket_handle bucket, void * data);
int bucket_lock(bucket_handle bucket, void *** array, int * length);
int bucket_unlock(bucket_handle bucket);
int bucket_reset(bucket_handle bucket);

#ifdef __cplusplus
} // extern "C" {
#endif

