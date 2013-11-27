
#include "sles.h"

struct openSLEopenSLES = {0, };

static int _OpenSLES_one_time_setup() {

	extern AAssetManager * __rh_hack_get_android_asset_manager();

	static const SLEngineOption options[] = { { SL_ENGINEOPTION_THREADSAFE,
			SL_BOOLEAN_TRUE },
			{ SL_ENGINEOPTION_LOSSOFCONTROL, SL_BOOLEAN_FALSE }, };

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

	return 0;

	err5: err4: err3: err2: err1: err0: return -1;
}

static int _OpenSLES_one_time_shutdown() {

	aout_OpenSLES_io_teardown();

	if( openSLES.outputMix )
		(*openSLES.outputMix)->Destroy(openSLES.outputMix);

	if( openSLES.engineObject )
		(*openSLES.engineObject)->Destroy(openSLES.engineObject);

	openSLES.engineObject = NULL;
	openSLES.engineItf = NULL;
	openSLES.outputMix = NULL;

	return 0;
}

static void _buffer_queue_cb(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	aout_handle h = (aout_handle)context;

    aout_OpenSLES_io_return_buffer( h ); // causes io thread to call buffer_queue_return_drain_buffer();
}

static int destroy_channel(aout_handle h) {

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



static SLuint32 _sles_get_channelmask(int channels) {

	switch(channels) {
	default:
	case 0:
		return 0;
	case 1:
		return SL_SPEAKER_FRONT_CENTER;
	case 2:
		return SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
	}
}

static int create_channel(aout_handle h) {

	off_t start;
	off_t length;

	struct priv_internal * p = get_priv(h);

	if(p->playerObject)
		return 0;

	{
		int e = 0;

		SLuint32 channelMask = _sles_get_channelmask(p->channels);

		const SLInterfaceID ids[] = { SL_IID_SEEK, SL_IID_BUFFERQUEUE };
		const SLboolean req[] = { SL_BOOLEAN_FALSE, SL_BOOLEAN_TRUE };

		// configure audio source
		SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, p->bq.nb_buffers};
		SLDataFormat_PCM format_pcm =
		{
			SL_DATAFORMAT_PCM,
			p->channels,
			p->samplerate * 1000,
			p->samplesize * 8,
			p->samplesize * 8,
			channelMask,
			SL_BYTEORDER_LITTLEENDIAN
		};
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

int aout_OpenSLES_open(aout_handle h, unsigned int channels, unsigned int rate, unsigned int samplesize) {

	if( h->priv != NULL ) {

		struct priv_internal * p = get_priv(h);

		if(p->channels == channels && p->samplerate == rate && p->samplesize != samplesize)
			return 0; // channel is already open, and the correct format.

		// channel is open, but the wrong format, close it.
		aout_OpenSLES_close(h);
	}

	// open new channel.
	{
		struct priv_internal * p = calloc(1, sizeof(struct priv_internal) );

		if( p ) {

			p->channels = channels;
			p->samplerate = rate;
			p->samplesize = samplesize;
			h->priv = p;

			buffer_queue_alloc( &p->bq, 3, p->samplesize * p->channels * p->samplerate ); // 3 one second buffers ( assuming 16bit )

			buffer_queue_alloc_buffers(&p->bq);

			create_channel(h);

			return 0;
		}
		return -1;
	}
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

    return 0;
}

int aout_create_OpenSLES() {

	return _OpenSLES_one_time_setup();
}

int aout_destroy_OpenSLES() {

	return _OpenSLES_one_time_shutdown();
}


