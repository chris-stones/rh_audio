#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<unistd.h>

#include "bucket.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/native_activity.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__)

#ifdef RH_TARGET_ANDROID
#undef RH_TARGET_ANDROID
#endif /* RH_TARGET_ANDROID */
#define RH_TARGET_ANDROID 1
#include "rh_audio_internal.h"

// ANDROID NDK BUG WORKAROUND ?
#ifdef __cplusplus
extern "C" {
#endif /*** __cplusplus ***/

typedef enum {

	PRIV_FLAG_PLAYING = 1, PRIV_FLAG_LOOPING = 2,

} priv_flag_enum_t;

typedef struct {

	SLObjectItf playerObject;
	SLPlayItf playItf;
	SLSeekItf seekItf;
	int fd;

} channel_t;

typedef channel_t* channel_ptr;

struct rh_audiosample_type {

	pthread_mutex_t monitor;
	pthread_cond_t cond;
	rh_audiosample_cb_type cb;
	void* cb_data;
	const char* src;
	int flags;
	int priv_flags;
	channel_ptr channel;
};

struct {

	SLObjectItf engineObject;
	SLEngineItf engineItf;
	SLObjectItf outputMix;

	AAssetManager *asset_manager;

} openSLES;

static int destroy_channel(channel_ptr channel) {

	if(channel) {

		if (channel->playerObject)
			(*channel->playerObject)->Destroy(channel->playerObject);

		close(channel->fd);

		free(channel);
	}
	return 0;
}

static void _cb(SLPlayItf caller, void * cb_data, SLuint32 event) {

	rh_audiosample_handle h = (rh_audiosample_handle) (cb_data);

	if( pthread_mutex_lock(&h->monitor) == 0) {

		int e = -1;
		
		int old_priv_flags = h->priv_flags;

		if (event & SL_PLAYEVENT_HEADATEND ) {

			destroy_channel( h->channel );
			h->channel = NULL;

			// inform waiting threads that this is finished.
			h->priv_flags = 0;
			pthread_cond_broadcast(&h->cond);

			if(h->cb && ((old_priv_flags & PRIV_FLAG_PLAYING) == PRIV_FLAG_PLAYING))
				(*(h->cb))(h, h->cb_data, RH_AUDIOSAMPLE_STOPPED);
		}

		pthread_mutex_unlock(&h->monitor);
	}
}

static int _rh_audiosample_setup() {
  
	extern AAssetManager * __rh_hack_get_android_asset_manager();
	  
	AAssetManager *asset_manager = __rh_hack_get_android_asset_manager();
 
	static const SLEngineOption options[] = { { SL_ENGINEOPTION_THREADSAFE,
			SL_BOOLEAN_TRUE },
			{ SL_ENGINEOPTION_LOSSOFCONTROL, SL_BOOLEAN_FALSE }, };

	openSLES.asset_manager = asset_manager;

	if (!openSLES.asset_manager)
		goto err0;

	if (SL_RESULT_SUCCESS
			!= slCreateEngine(&openSLES.engineObject,
					sizeof(options) / sizeof(options[0]), options, 0, NULL,
					NULL))
		goto err1;

	if (SL_RESULT_SUCCESS
			!= (*openSLES.engineObject)->Realize(openSLES.engineObject,
					SL_BOOLEAN_FALSE ))
		goto err2;

	if (SL_RESULT_SUCCESS
			!= (*openSLES.engineObject)->GetInterface(openSLES.engineObject,
					SL_IID_ENGINE, &openSLES.engineItf))
		goto err3;

	if (SL_RESULT_SUCCESS
			!= (*openSLES.engineItf)->CreateOutputMix(openSLES.engineItf,
					&openSLES.outputMix, 0, NULL, NULL))
		goto err4;

	if (SL_RESULT_SUCCESS
			!= (*openSLES.outputMix)->Realize(openSLES.outputMix,
					SL_BOOLEAN_FALSE ))
		goto err5;

	return 0;

	err5: err4: err3: err2: err1: err0: return -1;
}

static int _rh_audiosample_shutdown() {
  
   if(openSLES.outputMix)
     (*openSLES.outputMix)->Destroy(openSLES.outputMix);
   
   if(openSLES.engineObject)
     (*openSLES.engineObject)->Destroy(openSLES.engineObject);
   
   openSLES.engineItf = NULL;
   openSLES.engineObject = NULL;
   openSLES.outputMix = NULL;
   
   return 0;
}

static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int isInitialised = 0;

int rh_audiosample_setup() {

	int err = -1;

	if (pthread_mutex_lock(&init_mutex) == 0) {

		if (isInitialised)
			err = 0;
		else {
			err = _rh_audiosample_setup();
			if (!err)
				isInitialised = 1;
		}

		pthread_mutex_unlock(&init_mutex);
	}

	return err;
}

int rh_audiosample_shutdown() {

  
  int err = -1;

	if (pthread_mutex_lock(&init_mutex) == 0) {

		if (!isInitialised)
			err = 0;
		else {
			err = _rh_audiosample_shutdown();
			if (!err)
				isInitialised = 0;
		}

		pthread_mutex_unlock(&init_mutex);
	}

	return err;
}

static int init_recursive_pthread_mutex(pthread_mutex_t * mutex) {

	pthread_mutexattr_t attr;

	if( pthread_mutexattr_init(&attr) != 0)
		return -1;

	if(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {

		pthread_mutexattr_destroy(&attr);
		return -1;
	}

	{
		int err = pthread_mutex_init(mutex, &attr);

		pthread_mutexattr_destroy(&attr);

		return err;
	}
}

int rh_audiosample_open(rh_audiosample_handle * out, const char * source, int flags) {

	rh_audiosample_handle h = (rh_audiosample_handle) calloc(1,
			sizeof(struct rh_audiosample_type));

	if (!h)
		return -1;

	h->flags = flags;

	if (init_recursive_pthread_mutex(&h->monitor) != 0) {

		free(h);
		return -1;
	}

	if (pthread_cond_init(&h->cond, NULL) != 0) {

		pthread_mutex_destroy(&h->monitor);
		free(h);
		return -1;
	}

	if ((flags & RH_AUDIOSAMPLE_DONTCOPYSRC) == 0) {
		char * _s;
		if ((_s = (char*) calloc(1, strlen(source) + 1))) {
			strcpy(_s, source);
			h->src = _s;
			*out = h;

			rh_audiosample_add_to_internal_bucket( h );

			return 0;
		}
	} else {
		h->src = source;
		*out = h;

		rh_audiosample_add_to_internal_bucket( h );

		return 0;
	}

	pthread_mutex_destroy(&h->monitor);
	free(h);

	return -1;
}

int rh_audiosample_close(rh_audiosample_handle h) {

	if (h) {

		rh_audiosample_stop(h);

		rh_audiosample_remove_from_internal_bucket( h );

		if (pthread_mutex_lock(&h->monitor) == 0) {

			//TODO: cleanup resources - what resources?
		  
			if((h->flags & RH_AUDIOSAMPLE_DONTCOPYSRC)==0)
			  free(h->src);

			pthread_mutex_unlock(&h->monitor);
		}

		pthread_mutex_destroy(&h->monitor);

		free(h);
	}
	return 0;
}

static int get_asset_fd(AAssetManager* assetManager, const char * const fn,
		int * fd, off_t *start, off_t *length) {

	int err = -1;
	AAsset * asset = NULL;

	if (assetManager
			&& (asset = AAssetManager_open(assetManager, fn,
					AASSET_MODE_UNKNOWN))) {

		*fd = AAsset_openFileDescriptor(asset, start, length);

		if (*fd > 0)
			err = 0;

		AAsset_close(asset);
	}

	return err;
}

static channel_ptr create_channel(AAssetManager* assetManager,
		rh_audiosample_handle h) {

	int fd;
	off_t start;
	off_t length;

	channel_ptr p = NULL;

	if (!(p = (channel_ptr) calloc(1, sizeof(channel_t))))
		return NULL;

	if (get_asset_fd(assetManager, h->src, &fd, &start, &length) != 0) {

		free(p);
		return NULL;
	}

	{
		int e = 0;

		const SLInterfaceID ids[] = { SL_IID_SEEK };
		const SLboolean req[] = { SL_BOOLEAN_TRUE };

		p->fd = fd;

		// configure audio source
		SLDataLocator_AndroidFD loc_fd = { SL_DATALOCATOR_ANDROIDFD, fd, start,
				length };
		SLDataFormat_MIME format_mime = { SL_DATAFORMAT_MIME, NULL,
				SL_CONTAINERTYPE_UNSPECIFIED };
		SLDataSource audioSrc = { &loc_fd, &format_mime };

		// configure audio sink
		SLDataLocator_OutputMix loc_outmix = { SL_DATALOCATOR_OUTPUTMIX,
				openSLES.outputMix };
		SLDataSink audioSnk = { &loc_outmix, NULL };

		if (SL_RESULT_SUCCESS
				!= (*openSLES.engineItf)->CreateAudioPlayer(openSLES.engineItf,
						&p->playerObject, &audioSrc, &audioSnk,
						sizeof(ids) / sizeof(ids[0]), ids, req))
			e = 1;
		else if (SL_RESULT_SUCCESS
				!= (*p->playerObject)->Realize(p->playerObject,
						SL_BOOLEAN_FALSE ))
			e = 2;
		else if (SL_RESULT_SUCCESS
				!= (*p->playerObject)->GetInterface(p->playerObject,
						SL_IID_PLAY, &p->playItf))
			e = 3;
		else if (SL_RESULT_SUCCESS
				!= (*p->playerObject)->GetInterface(p->playerObject,
						SL_IID_SEEK, &p->seekItf))
			e = 4;
		else if (SL_RESULT_SUCCESS
				!= (*p->playItf)->SetCallbackEventsMask(p->playItf,
						SL_PLAYEVENT_HEADATEND ))
			e = 5;
		else if (SL_RESULT_SUCCESS
				!= (*p->playItf)->RegisterCallback(p->playItf, &_cb, h))
			e = 6;

		if (e) {

			destroy_channel(p);
			p = NULL;
		}
	}

	return p;
}

static int _rh_audiosample_play(rh_audiosample_handle h, int loop) {

	int err = -1;

	if (pthread_mutex_lock(&h->monitor) == 0) {

		h->priv_flags = 0;

		if (h->channel == NULL) 
			h->channel = create_channel(openSLES.asset_manager, h);

		if (h->channel) {

			(*h->channel->seekItf)->SetPosition(h->channel->seekItf, 0,
					SL_SEEKMODE_FAST );

			(*h->channel->seekItf)->SetLoop(h->channel->seekItf,
					loop ? SL_BOOLEAN_TRUE : SL_BOOLEAN_FALSE, 0,
					SL_TIME_UNKNOWN );

			if (SL_RESULT_SUCCESS
					== (*h->channel->playItf)->SetPlayState(h->channel->playItf,
							SL_PLAYSTATE_PLAYING )) {

				err = 0;

				h->priv_flags |= PRIV_FLAG_PLAYING;
				if (loop)
					h->priv_flags |= PRIV_FLAG_LOOPING;

				if (h->cb)
					(*(h->cb))(h, h->cb_data, RH_AUDIOSAMPLE_STARTED);

			} else {

				destroy_channel( h->channel );
				h->channel = NULL;

				if(h->cb)
					(*(h->cb))(h, h->cb_data, RH_AUDIOSAMPLE_ERROR);
			}
		}

		pthread_mutex_unlock(&h->monitor);
	}
	return err;
}

int rh_audiosample_play(rh_audiosample_handle h) {

	return _rh_audiosample_play(h, 0);
}

int rh_audiosample_loop(rh_audiosample_handle h) {

	return _rh_audiosample_play(h, 1);
}

int rh_audiosample_wait(rh_audiosample_handle h) {

	int err = -1;

	if (pthread_mutex_lock(&h->monitor) == 0) {

		while (h->priv_flags & (PRIV_FLAG_LOOPING | PRIV_FLAG_PLAYING))
			cond_wait_and_unlock_if_cancelled(&h->cond, &h->monitor);

		err = 0;

		pthread_mutex_unlock(&h->monitor);
	}

	return err;
}

int rh_audiosample_stop(rh_audiosample_handle h) {

	SLresult err = SL_RESULT_UNKNOWN_ERROR;

	if (h && pthread_mutex_lock(&h->monitor) == 0) {
	  
		int old_priv_flags = h->priv_flags;

		if (h && h->channel && h->channel->playItf)
			err = (*h->channel->playItf)->SetPlayState(h->channel->playItf,
					SL_PLAYSTATE_STOPPED );

		if (h->channel) {

			destroy_channel(h->channel);
			h->channel = NULL;
		}

		// inform waiting threads that this is finished.
		h->priv_flags = 0;
		pthread_cond_broadcast(&h->cond);
		pthread_mutex_unlock(&h->monitor);

		if (h->cb && ((old_priv_flags & PRIV_FLAG_PLAYING)==PRIV_FLAG_PLAYING)) {

			if (err == SL_RESULT_SUCCESS )
				(*(h->cb))(h, h->cb_data, RH_AUDIOSAMPLE_STOPPED);
			else
				(*(h->cb))(h, h->cb_data, RH_AUDIOSAMPLE_ERROR);
		}
	}

	return err == SL_RESULT_SUCCESS ? 0 : -1;
}

int rh_audiosample_isplaying(rh_audiosample_handle h) {

	int err = -1;

	if (pthread_mutex_lock(&h->monitor) == 0) {

		SLuint32 playState = SL_PLAYSTATE_STOPPED;

		if (SL_RESULT_SUCCESS
				== (*h->channel->playItf)->GetPlayState(h->channel->playItf,
						&playState))
			err = ((playState == SL_PLAYSTATE_PLAYING )? 1 : 0);

			pthread_mutex_unlock
		(&h->monitor);
	}

	return err;
}

int rh_audiosample_seek(rh_audiosample_handle h, int offset, int whence) {

	// TODO: only supports re-winding for now. ( perhaps the openSLES milli-seconds seeking would be better to use than the libsndfile frames ? )

	int err = -1;

	if (pthread_mutex_lock(&h->monitor) == 0) {

		if ((offset == 0) && (whence == SEEK_SET))
			if (SL_RESULT_SUCCESS
					== (*h->channel->seekItf)->SetPosition(h->channel->seekItf,
							0, SL_SEEKMODE_FAST ))
				err = 0;

		pthread_mutex_unlock(&h->monitor);
	}
	return err;
}

int rh_audiosample_register_cb(rh_audiosample_handle h,
		rh_audiosample_cb_type cb, void * cb_data) {

	int err = -1;

	if (pthread_mutex_lock(&h->monitor) == 0) {

		h->cb = cb;
		h->cb_data = cb_data;

		pthread_mutex_unlock(&h->monitor);
	}
	return err;
}

// ANDROID NDK BUG WORKAROUND ?
#ifdef __cplusplus
} // extern "C"
#endif /*** __cplusplus ***/

