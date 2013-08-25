
#include "sles.h"

struct openSLES openSLES = {0, };

static int _OpenSLES_one_time_setup() {

	extern AAssetManager * __rh_hack_get_android_asset_manager();

	static const SLEngineOption options[] = { { SL_ENGINEOPTION_THREADSAFE,
			SL_BOOLEAN_TRUE },
			{ SL_ENGINEOPTION_LOSSOFCONTROL, SL_BOOLEAN_FALSE }, };

	if(openSLES.is_setup)
		return 0;

	if( aout_OpenSLES_io_setup() != 0 )
		goto err0;

	openSLES.asset_manager = __rh_hack_get_android_asset_manager();

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


	openSLES.is_setup = 1;

	return 0;

	err5: err4: err3: err2: err1: err0: return -1;
}

/*
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
*/

static void _buffer_queue_cb(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	buffer_t * db;
	rh_audiosample_handle h = (rh_audiosample_handle)context;
	struct priv_internal * p = get_priv(h);

	db = buffer_queue_return_and_get_drain_buffer( &p->bq );

    if(db) {
        SLresult result;
        result = ( *p->bufferQueueItf )->Enqueue(p->bufferQueueItf, db->buffer, db->bytes_used);
    }
}

static int destroy_channel(rh_audiosample_handle h) {

	struct priv_internal * p = get_priv(h);

	if(p) {
		if (p->playerObject)
			(*p->playerObject)->Destroy(p->playerObject);

		p->playerObject = NULL;
		p->playItf = NULL;
		p->bufferQueueItf = NULL;
	}
	return 0;
}

static int create_channel(rh_audiosample_handle h) {

	off_t start;
	off_t length;

	struct priv_internal * p = get_priv(h);

	if(p->playerObject)
		return 0;

	{
		int e = 0;

		const SLInterfaceID ids[] = { SL_IID_SEEK, SL_IID_BUFFERQUEUE };
		const SLboolean req[] = { SL_BOOLEAN_FALSE, SL_BOOLEAN_TRUE };

		// configure audio source
		SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, p->bq.nb_buffers};
		SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_8,
			SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
			SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
		SLDataSource audioSrc = {&loc_bufq, &format_pcm};

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
//		else if (SL_RESULT_SUCCESS
//				!= (*p->playerObject)->GetInterface(p->playerObject,
//						SL_IID_SEEK, &p->seekItf))
//			e = 4;
		else if(SL_RESULT_SUCCESS
			!= (*p->playerObject)->GetInterface(p->playerObject,
						SL_IID_BUFFERQUEUE, &p->bufferQueueItf))
			e = 5;
		else if(SL_RESULT_SUCCESS
			!= (*p->bufferQueueItf)->RegisterCallback(p->bufferQueueItf,
						&_buffer_queue_cb, h))
			e = 6;
		else if (SL_RESULT_SUCCESS
				!= (*p->playItf)->SetCallbackEventsMask(p->playItf,
						SL_PLAYEVENT_HEADATEND ))
			e = 7;
//		else if (SL_RESULT_SUCCESS
//				!= (*p->playItf)->RegisterCallback(p->playItf, &_cb, h))
//			e = 8;

		if (e) {

			destroy_channel(h);
			return -1;
		}
	}

	return 0;
}

int aout_OpenSLES_open(aout_handle h, unsigned int channels, unsigned int rate) {

	struct priv_internal * p = calloc(1, sizeof(struct priv_internal) );

	if( p ) {

		h->priv = p;

		// FIXME: more assuming formats!!!
		buffer_queue_alloc( &p->bq, 2, 2 * channels * rate ); // 2 one second buffers ( assumeing 16bit )
		buffer_queue_alloc_buffers(&p->bq);

		create_channel(h);

		return 0;
	}
	return -1;
}

int aout_OpenSLES_close(aout_handle h) {

	if(h) {

		struct priv_internal * p = get_priv(h);

		if(p) {

			destroy_channel(h);
			buffer_queue_free_buffers( &p->bq );
			buffer_queue_free( &p->bq );
			free(p);
			h->priv = NULL;
		}
	}
	return 0;
}

int aout_OpenSLES_start( aout_handle h) {

  int e = -1;

  if( ! ( h->status & AOUT_STATUS_PLAYING ) ) {

    if(aout_OpenSLES_io_add(h)==0)
      e = aout_started( h );
  }

  return e;
}

int aout_OpenSLES_stop( aout_handle h) {

  if( h->status & AOUT_STATUS_PLAYING ) {

	struct priv_internal * p = get_priv(h);

	(*p->playItf)->SetPlayState( p->playItf, SL_PLAYSTATE_STOPPED );

    aout_OpenSLES_io_rem(h);

    aout_stopped( h );

    aout_handle_events( h ); // todo - move this to the io-thread !
  }

  return 0;
}

int aout_init_interface_OpenSLES(aout_handle p) {

    p->channel_start 	= &aout_OpenSLES_start;
    p->channel_stop 	= &aout_OpenSLES_stop;
    p->channel_open 	= &aout_OpenSLES_open;
    p->channel_close 	= &aout_OpenSLES_close;

    return _OpenSLES_one_time_setup(); // TODO: use pthread_once();
}

