
#include "sles_private.h"

static int destroy_channel(rh_aout_itf self) {

	struct aout_instance * instance = (struct aout_instance *)self;

	if(instance) {
		if (instance->playerObject)
			(*instance->playerObject)->Destroy(instance->playerObject);

		instance->playerObject = NULL;
		instance->playItf = NULL;
		instance->bufferQueueItf = NULL;

		buffer_queue_free( &instance->bq );
	}
	return 0;
}

int aout_sles_close(rh_aout_itf * pself) {

	if(pself) {

		struct aout_instance * instance = (struct aout_instance *)*pself;

		if(instance) {

			destroy_channel(h);
			free(instance->interface);
			free(instance);
		}
		*pself = NULL;
	}
	return 0;
}

