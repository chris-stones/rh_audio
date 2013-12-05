
#include "rh_audio_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include<stdarg.h>
#include<linux/limits.h>

#include<pthread.h>

#include "asmp/asmp.h"
#include "aout/aout.h"

static rh_aout_api_itf api_interface = 0;

int rh_audio_setup_api() {

	if( rh_aout_create_api(&api_interface) == 0) {
		if(api_interface) {

			if( (*api_interface)->setup(api_interface) == 0)
				return 0;

			(*api_interface)->shutdown(&api_interface);
		}
	}

	return -1;
}

int rh_audio_shutdown_api() {

	if(api_interface)
		(*api_interface)->shutdown(&api_interface);

	return 0;
}


struct audio_instance {

	// interface ptr must be the first item in the instance.
	struct rh_audio * interface;

	// private data
	char * source;
	int openflags;

	int is_playing;

	rh_asmp_itf audio_sample;

	/// SYNCHRONISATION
	pthread_mutex_t sync_mutex;
	pthread_cond_t  sync_cond;
	int audio_cmd_sync;

	/// WAIT
	pthread_mutex_t wait_mutex;
	pthread_cond_t  wait_cond;
};

static int _send_sync_command(rh_audio_itf self) {

	struct audio_instance * instance = (struct audio_instance *)self;

	if( pthread_mutex_lock(&instance->sync_mutex) == 0) {

		instance->audio_cmd_sync++;

		pthread_mutex_unlock(&instance->sync_mutex);

		(*api_interface)->sync(api_interface, instance->audio_sample);

		return 0;
	}
	return -1;
}

static int _recv_sync_command(rh_audio_itf self) {

	struct audio_instance * instance = (struct audio_instance *)self;

	if( pthread_mutex_lock(&instance->sync_mutex) == 0) {

		instance->audio_cmd_sync--;

		if( instance->audio_cmd_sync == 0)
			pthread_cond_broadcast( &instance->sync_cond );

		pthread_mutex_unlock(&instance->sync_mutex);

		return 0;
	}
	return -1;
}

static int _wait_for_sync(rh_audio_itf self) {

	struct audio_instance * instance = (struct audio_instance *)self;

	if( pthread_mutex_lock(&instance->sync_mutex) == 0) {

		while( instance->audio_cmd_sync ) {

			pthread_cond_wait(&instance->sync_cond, &instance->sync_mutex);
		}

		pthread_mutex_unlock(&instance->sync_mutex);

		return 0;
	}
	return -1;
}

static int _set_is_playing(rh_audio_itf self, int is_playing) {

	struct audio_instance * instance = (struct audio_instance *)self;

	if( pthread_mutex_lock( &instance->wait_mutex ) == 0 ) {

		instance->is_playing = is_playing;

		if(!is_playing)
			pthread_cond_broadcast( &instance->wait_cond );

		pthread_mutex_unlock( &instance->wait_mutex );

		return 0;
	}
	return -1;
}

static int _on_output_event(void * _self, rh_output_event_enum_t ev) {

	rh_audio_itf self = (rh_audio_itf)_self;

	struct audio_instance * instance = (struct audio_instance *)self;

	switch(ev) {
		case RH_ASMP_OUTPUT_EVENT_STARTED:
			_set_is_playing(self, 1);
			break;
		case RH_ASMP_OUTPUT_EVENT_STOPPED:
			_set_is_playing(self, 0);
			break;
		case RH_ASMP_OUTPUT_EVENT_ERROR:
			break;
		case RH_ASMP_OUTPUT_EVENT_SYNC:
			_recv_sync_command(self);
			break;
	}

	return 0;
}

static int _impl_open(rh_audio_itf self, const char * source, int flags) {

	rh_asmp_itf audio_sample = NULL;

	char * _src = (char*)source;

	int err = -1;

	if(err != 0) {
		err = rh_asmp_create(&audio_sample, RH_ASMP_IMP_DEFAULT, &_on_output_event, (void*)self);
		printf(" rh_asmp_create(&audio_sample, RH_ASMP_IMP_DEFAULT) == %d\n", err);
	}
	if(err != 0 && RH_ASMP_IMP_DEFAULT != RH_ASMP_IMP_FFMPEG) {
		err = rh_asmp_create(&audio_sample, RH_ASMP_IMP_FFMPEG, &_on_output_event, (void*)self);
		printf(" rh_asmp_create(&audio_sample, RH_ASMP_IMP_FFMPEG) == %d\n", err);
	}
	if(err != 0 && RH_ASMP_IMP_DEFAULT != RH_ASMP_IMP_S5PROM) {
		err = rh_asmp_create(&audio_sample, RH_ASMP_IMP_S5PROM, &_on_output_event, (void*)self);
		printf(" rh_asmp_create(&audio_sample, RH_ASMP_IMP_S5PROM) == %d\n", err);
	}

	if(err != 0)
		goto bad;

	if( (*audio_sample)->open(audio_sample, source) != 0) {
		printf("error opening audiosample %s\n", source);
		goto bad;
	}

//	if((flags & RH_AUDIO_OPEN_DONTCOPYSOURCE)==0)
	{
		int l = strlen(source)+1;
		if((_src = malloc(l)))
			memcpy(_src,source,l);
		else {
			printf("impl_open rh_audio.c error malloc(%d)\n", l);
			goto bad;
		}
	}

good:
	{
		struct audio_instance * instance = (struct audio_instance *)self;
		instance->source = _src;
		instance->openflags = flags;
		instance->audio_sample = audio_sample;
		return 0;
	}
bad:
	{
		if(_src != source)
			free(_src);
		if(audio_sample)
			(*audio_sample)->close(&audio_sample);
		return -1;
	}
}

static int _impl_openf(rh_audio_itf  self, int flags, const char * format, ...) {

	int err = 0;
	char *path = NULL;
	va_list va;
	va_start(va, format);
    if(!((path = malloc(sizeof (char) * PATH_MAX))))
       err = -1;
    else if(vsnprintf(path,PATH_MAX,format,va)>=PATH_MAX)
        err = -1; /* truncated */
    va_end(va);

	if(!err)
		err = _impl_open(self, path, flags /* & ~RH_AUDIO_OPEN_DONTCOPYSOURCE */);

	free(path);

	return err;
}

static int _impl_close(rh_audio_itf *pself) {

	struct audio_instance * instance = (struct audio_instance *)*pself;

	if(instance->audio_sample)
		(*instance->audio_sample)->close(&instance->audio_sample);

	{
//		if((instance->openflags & RH_AUDIO_OPEN_DONTCOPYSOURCE)==0)
			free(instance->source);
		instance->source = NULL;
	}

	free(instance->interface);
	free(instance);
	*pself = NULL;
	return 0;
}

static int _impl_play(rh_audio_itf self) {

	struct audio_instance * instance = (struct audio_instance *)self;

	int e = 0;

	e = (*api_interface)->play( api_interface, instance->audio_sample );

	_send_sync_command(self);

	return e;
}

static int _impl_loop(rh_audio_itf self) {

	struct audio_instance * instance = (struct audio_instance *)self;

	int e = 0;

	e = (*api_interface)->loop( api_interface, instance->audio_sample );

	_send_sync_command(self);

	return e;
}

static int _impl_stop(rh_audio_itf self) {

	struct audio_instance * instance = (struct audio_instance *)self;

	int e = 0;

	e = (*api_interface)->stop( api_interface, instance->audio_sample );

	_send_sync_command(self);

	return e;
}

static int _impl_is_playing(rh_audio_itf self) {

	struct audio_instance * instance = (struct audio_instance *)self;

	_wait_for_sync(self);

	return instance->is_playing;
}

static int _impl_wait(rh_audio_itf self) {

	struct audio_instance * instance = (struct audio_instance *)self;

	_wait_for_sync(self);

	if( pthread_mutex_lock( &instance->wait_mutex ) == 0 ) {

		while( instance->is_playing )
			pthread_cond_wait( &instance->wait_cond, &instance->wait_mutex);

		pthread_mutex_unlock( &instance->wait_mutex );

		return 0;
	}
	return -1;
}

int rh_audio_create( rh_audio_itf * itf ) {

	{
		struct audio_instance * instance  = calloc(1, sizeof( struct audio_instance ) );
		struct rh_audio       * interface = calloc(1, sizeof( struct rh_audio       ) );

		if(!instance || !interface)
			goto bad;

		if( pthread_mutex_init( &instance->sync_mutex, NULL ) != 0)
			goto bad;

		if(pthread_cond_init ( &instance->sync_cond , NULL ) != 0)
			goto bad;

		if(pthread_mutex_init ( &instance->wait_mutex , NULL ) != 0)
			goto bad;

		if(pthread_cond_init ( &instance->wait_cond , NULL ) != 0)
			goto bad;

		instance->interface = interface;

		interface->open         = &_impl_open;
		interface->openf        = &_impl_openf;
		interface->close        = &_impl_close;
		interface->play         = &_impl_play;
		interface->loop         = &_impl_loop;
		interface->stop         = &_impl_stop;
		interface->wait         = &_impl_wait;
		interface->is_playing   = &_impl_is_playing;

good:
		*itf = (rh_audio_itf)instance;
		return 0;
bad:
		free(instance);
		free(interface);
		return -1;
	}

	return -1;
}

