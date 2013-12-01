
#include "bucket.h"

#include<stdlib.h>
#include<alloca.h>
#include<string.h>

#include "bucket.h"

//#define BUCKET_THREADED 1

struct bucket_type {

#ifdef BUCKET_THREADED
    pthread_mutex_t monitor;
#else
    int locked;
#endif

    void **items;
    int n_outputs;
    int n_outputs_free;
    int n_outputs_used;
};

#ifdef BUCKET_THREADED
static int __lock(struct bucket_type * bt) {

    return pthread_mutex_lock( &bt->monitor );
}
static int __unlock(struct bucket_type * bt) {

    return pthread_mutex_unlock( &bt->monitor );
}
#else
static int __lock(struct bucket_type * bt) {

    if( bt->locked != 0)
        return -1;
    bt->locked++;
    return 0;
}
static int __unlock(struct bucket_type * bt) {

    if( bt->locked != 1)
        return -1;
    bt->locked--;
    return 0;
}
#endif

int bucket_create(bucket_handle * out) {

    bucket_handle h = calloc(1, sizeof(struct bucket_type) );

#ifdef BUCKET_THREADED
    if( h && ( pthread_mutex_init(&h->monitor, NULL) == 0) ) {
        *out = h;
        return 0;
    }
#else
    if(h) {
        *out = h;
        return 0;
    }
#endif

    free(h);

    return -1;
}

int bucket_free( bucket_handle h ) {

    if(h) {
        free(h->items);
#ifdef BUCKET_THREADED
        pthread_mutex_destroy(&h->monitor);
#endif
    }

    free(h);

    return 0;
}

static int _add(bucket_handle bucket, void * data, int start_search) {

    for(; start_search<bucket->n_outputs; start_search++)
        if( !bucket->items[start_search] ) {
            bucket->items[start_search] = data;
            --bucket->n_outputs_free;
            ++bucket->n_outputs_used;
            return 0;
        }
    return -1;
}

static int _grow_and_add(bucket_handle bucket, void * data, const int extra) {

    void ** items = realloc( bucket->items, bucket->n_outputs + extra * sizeof(void*));

    if( !items )
        return -1;

    memset( items + bucket->n_outputs, 0, extra * sizeof(void*) );

    bucket->items = items;
    bucket->n_outputs += extra;
    bucket->n_outputs_free += extra;

    return _add( bucket, data, bucket->n_outputs - extra );
}

int bucket_add(bucket_handle bucket, void * data) {

    int e = -1;

    if(data && __lock( bucket ) == 0) {

        if( bucket->n_outputs_free )
            e = _add( bucket, data, 0 );
        else
            e= _grow_and_add(bucket, data, 4);

        __unlock( bucket );
    }

    return e;
}

int bucket_reset(bucket_handle bucket) {

    int e = -1;

    if( __lock( bucket ) == 0) {

        memset( bucket->items, 0, bucket->n_outputs_used * sizeof bucket->items[0] );
        bucket->n_outputs_free = bucket->n_outputs;
        bucket->n_outputs_used = 0;

        e = 0;

        __unlock( bucket );
    }

    return e;
}


int bucket_remove(bucket_handle bucket, void * data) {

    int e = -1;

    if(data && __lock( bucket ) == 0) {

        int i;

        for(i=0; i<bucket->n_outputs; i++) {

            if( bucket->items[i] == data ) {

                bucket->items[i] = NULL;
                ++bucket->n_outputs_free;
                --bucket->n_outputs_used;
                e = 0;

                for(; i<bucket->n_outputs-1; i++)
                    bucket->items[i] = bucket->items[i+1];

                break;
            }
        }
        __unlock( bucket );
    }
    return e;
}

int bucket_lock(bucket_handle bucket, void *** array, int * length) {

    if( __lock( bucket ) == 0 ) {

        *array = bucket->items;
        *length = bucket->n_outputs_used;

        return 0;
    }
    return -1;
}

int bucket_unlock(bucket_handle bucket) {

    return __unlock( bucket);
}

