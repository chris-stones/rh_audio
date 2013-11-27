
#include "alsa_private.h"
#include <stdio.h>

/*
 * Somthing went wrong!
 * IF the sample was supposed to be playing, send a stopped event. ( prevent locking up a thread waiting for a sample to finish. )
 * Send an error event, set the channel to free, and stopped.
 */
static int error(rh_aout_itf self) {

	struct aout_instance * instance = (struct aout_instance *)self;

	rh_asmp_itf audio_sample = instance->audio_sample;

	if(audio_sample) {
		if(instance->status_flags & (RH_AOUT_STATUS_PLAYING | RH_AOUT_STATUS_LOOPING) )
			(*audio_sample)->on_output_event(audio_sample, RH_ASMP_OUTPUT_EVENT_STOPPED);

		(*audio_sample)->on_output_event(audio_sample, RH_ASMP_OUTPUT_EVENT_ERROR);
	}

	instance->status_flags = RH_AOUT_STATUS_STOPPED;

	aout_alsa_set_sample(self, NULL);

	return -1;
}

static void * get_buffer_address( const snd_pcm_channel_area_t *areas, snd_pcm_uframes_t offset ) {

    unsigned char* addr = (unsigned char*)areas->addr;

    return addr + (areas->first/8) + (areas->step/8) * offset;
}

static int prepare_for_transfer( snd_pcm_t * handle ) {

    int err;

    snd_pcm_state_t s = snd_pcm_state( handle );

    switch(s) {

    case SND_PCM_STATE_OPEN:
        return -1; // config failed !?

    case SND_PCM_STATE_SETUP:

        if( ( err = snd_pcm_prepare( handle ) ) != 0 )
            if( ( err = snd_pcm_recover( handle, err, 1 ) ) != 0 )
                return -1;

        return 0;

    case SND_PCM_STATE_PREPARED:

        return 0;

    case SND_PCM_STATE_RUNNING:

        return 0;

    case SND_PCM_STATE_XRUN:

        if( ( err = snd_pcm_recover( handle, -EPIPE, 1) ) != 0 )
            return -1;

        return 0;

    case SND_PCM_STATE_DRAINING:

        return -1;

    case SND_PCM_STATE_PAUSED:

        if( ( err = snd_pcm_pause( handle, 0 ) ) != 0 )
            if( ( err = snd_pcm_recover( handle, err, 1 ) ) != 0 )
                return -1;

        return 0;

    case SND_PCM_STATE_SUSPENDED:

        if( ( err = snd_pcm_recover( handle, -ESTRPIPE, 1) ) != 0 )
            return -1;

        return 0;

    case SND_PCM_STATE_DISCONNECTED:
        return -1;
    }

    return -1; // never hit
}

static int indirect_transfer(rh_aout_itf self, int frames) {

    struct aout_instance * instance = (struct aout_instance *)self;

    frames = aout_alsa_read_sample(self, frames, instance->imp_buffer);

    int err = snd_pcm_writei( instance->handle, instance->imp_buffer, frames);

    if(err >= 0)
        frames = err;
    else {
        frames = 0;
        if( snd_pcm_recover(instance->handle, err, 0) != 0 )
            return error(self);
    }

    return frames;
}

static int direct_transfer(rh_aout_itf self, snd_pcm_uframes_t frames) {

	struct aout_instance * instance = (struct aout_instance *)self;

    int err;

    const snd_pcm_channel_area_t *my_areas;

    snd_pcm_uframes_t offset;

    snd_pcm_t *handle = instance->handle;

    if((err = snd_pcm_mmap_begin(handle,&my_areas, &offset, &frames))<0) {
        if( snd_pcm_recover(handle, err, 1) != 0 )
            return error(self);

        return 0;
    }

	frames = aout_alsa_read_sample(self, frames, get_buffer_address( my_areas, offset ) );

    snd_pcm_sframes_t commitres =
        snd_pcm_mmap_commit(handle, offset, frames);

    if( commitres < 0 || commitres != frames ) {

        if( snd_pcm_recover( handle, commitres >= 0 ? -EPIPE : commitres, 1 ) != 0 )
            return error(self);
    }

    return frames;
}
static int transfer(rh_aout_itf self) {

    struct aout_instance * instance = (struct aout_instance *)self;

    snd_pcm_t * handle = instance->handle;

    while( instance->status_flags & (RH_AOUT_STATUS_PLAYING | RH_AOUT_STATUS_LOOPING )) {

        snd_pcm_sframes_t  avail;

        prepare_for_transfer( handle );

        while( ( avail = snd_pcm_avail_update( handle ) ) < 0 )
            if( snd_pcm_recover( handle, avail, 1 ) != 0 )
                return error(self);

		if( aout_alsa_atend_sample(self) ) {

            if(instance->status_flags & RH_AOUT_STATUS_LOOPING)
                if( aout_alsa_reset_sample(self) == 0 )
                    continue;


            if( avail >= instance->buffer_size) {

				aout_alsa_reset_sample(self);
                return aout_alsa_stop( self );
            }

            instance->sleep = instance->period_time;

            return 0;
        }

        if( avail < instance->period_size )
            return 0;

        snd_pcm_uframes_t size = instance->period_size;

        while( size > 0 ) {

            snd_pcm_uframes_t frames = size;

            if( instance->imp_flags == IMP_FLAG_MMAP )
                frames = direct_transfer(self, frames);
            else if( instance->imp_flags == IMP_FLAG_RW)
                frames = indirect_transfer(self, frames);

            size -= frames;

            if(size <= 0)
                return 0;

            if(frames == 0)
                break;
        }
    }

    return 0;
}

int aout_alsa_update(rh_aout_itf self) {

    struct aout_instance * instance = (struct aout_instance *)self;

    snd_pcm_t * handle = instance->handle;

    instance->sleep = 0;

    int e = transfer( self );

    if( ( e == 0 ) && ( instance->status_flags & (RH_AOUT_STATUS_PLAYING | RH_AOUT_STATUS_LOOPING ) ) )
        if( snd_pcm_state( handle ) == SND_PCM_STATE_PREPARED)
            if( snd_pcm_start( handle ) < 0)
                e = error( self );

    return e;
}

