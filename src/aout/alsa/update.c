
#include "alsa.h"
#include <stdio.h>

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

static int indirect_transfer(aout_handle h, int frames) {

    struct priv_internal *priv = get_priv(h);

    frames = h->samp_reader( h->samp_data, frames, priv->imp_buffer, 0 );

    int err = snd_pcm_writei( priv->handle, priv->imp_buffer, frames);

    if(err >= 0)
        frames = err;
    else {
        frames = 0;
        if( snd_pcm_recover(priv->handle, err, 0) != 0 )
            return aout_error(h);
    }

    return frames;
}

static int direct_transfer(aout_handle h, snd_pcm_uframes_t frames) {

    int err;

    const snd_pcm_channel_area_t *my_areas;

    snd_pcm_uframes_t offset;

    snd_pcm_t *handle = get_priv(h)->handle;

    if((err = snd_pcm_mmap_begin(handle,&my_areas, &offset, &frames))<0) {
        if( snd_pcm_recover(handle, err, 1) != 0 )
            return aout_error(h);

        return 0;
    }

    frames = h->samp_reader( h->samp_data, frames, get_buffer_address( my_areas, offset ), 0 );

    snd_pcm_sframes_t commitres =
        snd_pcm_mmap_commit(handle, offset, frames);

    if( commitres < 0 || commitres != frames ) {

        if( snd_pcm_recover( handle, commitres >= 0 ? -EPIPE : commitres, 1 ) != 0 )
            return aout_error(h);
    }

    return frames;
}

static int is_stream_at_end(aout_handle h) {

	if( h->samp_stater( h->samp_data ) & 1 ) // TODO: ENUM STAT MASKS!!! ( 1 == stream at end )
		return 1;

	return 0;
}

static int transfer(aout_handle h) {

    struct priv_internal *priv = get_priv(h);

    snd_pcm_t * handle = priv->handle;

    while( h->status & AOUT_STATUS_PLAYING ) {

        snd_pcm_sframes_t  avail;

        prepare_for_transfer( handle );

        while( ( avail = snd_pcm_avail_update( handle ) ) < 0 )
            if( snd_pcm_recover( handle, avail, 1 ) != 0 )
                return aout_error( h );

        if( is_stream_at_end( h ) ) {

            if(h->status & AOUT_STATUS_LOOPING)
                if( h->samp_resetter( h->samp_data ) == 0 )
                    continue;


            if( avail >= priv->buffer_size) {

                aout_alsa_io_rem( h );
                return aout_stopped( h );
            }

            priv->sleep = priv->period_time;

            return 0;
        }

        if( avail < priv->period_size )
            return 0;

        snd_pcm_uframes_t size = priv->period_size;

        while( size > 0 ) {

            snd_pcm_uframes_t frames = size;

            if( priv->imp_flags == IMP_FLAG_MMAP )
                frames = direct_transfer(h, frames);
            else if( priv->imp_flags == IMP_FLAG_RW)
                frames = indirect_transfer(h, frames);

            size -= frames;

            if(size <= 0)
                return 0;

            if(frames == 0)
                break;
        }
    }

    return 0;
}

int aout_alsa_update(aout_handle h) {

    struct priv_internal *priv = get_priv(h);

    snd_pcm_t * handle = priv->handle;

    priv->sleep = 0;

    int e = transfer( h );

    if( ( e == 0 ) && ( h->status & AOUT_STATUS_PLAYING ) )
        if( snd_pcm_state( handle ) == SND_PCM_STATE_PREPARED)
            if( snd_pcm_start( handle ) < 0)
                e = aout_stopped( h );

    aout_handle_events(h);

    return e;
}

