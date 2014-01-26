
#pragma once

#include "sles.h"
#include "../../bucket.h"

struct sles_api_instance {

    // interface ptr must be the first item in the instance.
    struct rh_aout_api * interface;

    // private data
    volatile pthread_t  thread;
    bucket_handle       aout_itf_bucket;

    struct {
        int read;
        int write;
    } cmd_pipe;

    SLObjectItf engineObject;
	SLEngineItf engineItf;
	SLObjectItf outputMix;
	AAssetManager *asset_manager;
};

typedef enum {

	RH_AOUT_STATUS_STOPPED = 1<<0,
	RH_AOUT_STATUS_PLAYING = 1<<1,
	RH_AOUT_STATUS_LOOPING = 1<<2,

} rh_aout_sles_status_flags;

struct aout_instance {

	// interface ptr must be the first item in the instance.
	struct rh_aout * interface;

	// private data //

	rh_aout_api_itf api;

	// audio-sample ( may be NULL if channel is free )
	rh_asmp_itf audio_sample;

	SLObjectItf playerObject;
	SLPlayItf playItf;
	SLAndroidSimpleBufferQueueItf bufferQueueItf;

	int channels;
	int samplerate;
	int samplesize;
	int bigendian;

	buffer_queue_t bq;

	int status_flags;
};

int aout_sles_open(rh_aout_itf self, unsigned int channels, unsigned int rate, unsigned int samplesize, unsigned int bigendian);
int aout_sles_close(rh_aout_itf * pself);
int aout_sles_update(rh_aout_itf self);
int aout_sles_play( rh_aout_itf self );
int aout_sles_loop( rh_aout_itf self );
int aout_sles_stop( rh_aout_itf self );
int aout_sles_set_sample(rh_aout_itf self, rh_asmp_itf sample);
int aout_sles_get_sample(rh_aout_itf self, rh_asmp_itf * sample);

