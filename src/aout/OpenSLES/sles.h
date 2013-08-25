
#pragma once

#include "../aout_internal.h"
#include "buffer_queue.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/asset_manager.h>
#include <android/native_activity.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__)

struct openSLES {

	int is_setup;
	SLObjectItf engineObject;
	SLEngineItf engineItf;
	SLObjectItf outputMix;
	AAssetManager *asset_manager;


};
extern struct openSLES openSLES;

struct priv_internal {

	SLObjectItf playerObject;
	SLPlayItf playItf;
//	SLSeekItf seekItf;
	SLAndroidSimpleBufferQueueItf bufferQueueItf;

	buffer_queue_t bq;
};

static inline struct priv_internal * get_priv(aout_handle p) {

  return (struct priv_internal *)p->priv;
}

int aout_OpenSLES_open(aout_handle h, unsigned int channels, unsigned int rate);
int aout_OpenSLES_close(aout_handle h);
int aout_OpenSLES_update( aout_handle h);
int aout_OpenSLES_start( aout_handle h);
int aout_OpenSLES_stop( aout_handle h);

int aout_OpenSLES_io_setup();
int aout_OpenSLES_io_add(aout_handle h);
int aout_OpenSLES_io_rem(aout_handle h);
int aout_OpenSLES_io_teardown();



