
#include "sles_private.h"

static void _buffer_queue_cb(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	rh_aout_itf self = (rh_aout_itf)context;

	struct aout_instance * instance = (struct aout_instance *)self;

	// causes io thread to call buffer_queue_return_drain_buffer();
	_impl_consumed_buffer(instance->api, instance->audio_sample );
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

static int destroy_channel(rh_aout_itf self) {

	struct aout_instance * instance = (struct aout_instance *)self;

	if(instance) {
		if (instance->playerObject)
			(*instance->playerObject)->Destroy(instance->playerObject);

		instance->playerObject = NULL;
		instance->playItf = NULL;
		instance->bufferQueueItf = NULL;

		buffer_queue_free(&instance->bq);
	}
	return 0;
}

static int create_channel(rh_aout_itf self) {

	struct aout_instance * instance = (struct aout_instance *)self;

	off_t start;
	off_t length;

	struct sles_api_instance * api_instance =
		(struct sles_api_instance *)instance->api;

	if(instance->playerObject)
		return 0;

	{
		int e = 0;

		SLuint32 channelMask = _sles_get_channelmask(instance->channels);

		const SLInterfaceID ids[] = { SL_IID_SEEK, SL_IID_BUFFERQUEUE };
		const SLboolean req[] = { SL_BOOLEAN_FALSE, SL_BOOLEAN_TRUE };

		// configure audio source
		SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, instance->bq.nb_buffers};
		SLDataFormat_PCM format_pcm =
		{
			SL_DATAFORMAT_PCM,
			instance->channels,
			instance->samplerate * 1000,
			instance->samplesize * 8,
			instance->samplesize * 8,
			channelMask,
			SL_BYTEORDER_LITTLEENDIAN
		};
		SLDataSource audioSrc = {&loc_bufq, &format_pcm};

		// configure audio sink
		SLDataLocator_OutputMix loc_outmix = { SL_DATALOCATOR_OUTPUTMIX,
				api_instance->outputMix };
		SLDataSink audioSnk = { &loc_outmix, NULL };

		if (SL_RESULT_SUCCESS
				!= (*api_instance->engineItf)->CreateAudioPlayer(
						api_instance->engineItf,
						&instance->playerObject,
						&audioSrc, &audioSnk,
						sizeof(ids) / sizeof(ids[0]), ids, req))
			e = 1;
		else if (SL_RESULT_SUCCESS
				!= (*instance->playerObject)->Realize(instance->playerObject,
						SL_BOOLEAN_FALSE ))
			e = 2;
		else if (SL_RESULT_SUCCESS
				!= (*instance->playerObject)->GetInterface(instance->playerObject,
						SL_IID_PLAY, &instance->playItf))
			e = 3;
		else if(SL_RESULT_SUCCESS
			!= (*instance->playerObject)->GetInterface(instance->playerObject,
						SL_IID_BUFFERQUEUE, &instance->bufferQueueItf))
			e = 5;
		else if(SL_RESULT_SUCCESS
			!= (*instance->bufferQueueItf)->RegisterCallback(instance->bufferQueueItf,
						&_buffer_queue_cb, self))
			e = 6;
		else if (SL_RESULT_SUCCESS
				!= (*instance->playItf)->SetCallbackEventsMask(instance->playItf,
						SL_PLAYEVENT_HEADATEND ))
			e = 7;

		if (e) {

			destroy_channel(h);
			return -1;
		}
	}

	return 0;
}

int aout_sles_open(rh_aout_itf self, unsigned int channels, unsigned int rate, unsigned int samplesize) {

	struct aout_instance * instance = (struct aout_instance *)self;

	if( instance != NULL ) {

		if(instance->channels == channels && instance->samplerate == rate && instance->samplesize != samplesize)
			return 0; // channel is already open, and the correct format.

		// channel is open, but the wrong format, close it.
		destroy_channel(h);
	}

	// open new channel.
	{
		if( instance ) {

			instance->channels = channels;
			instance->samplerate = rate;
			instance->samplesize = samplesize;

			buffer_queue_alloc( &instance->bq, 3, 32 * 1024 ); // 3 32k periods.
			buffer_queue_alloc_buffers(&instance->bq);

			create_channel(h);

			return 0;
		}
		return -1;
	}
}

