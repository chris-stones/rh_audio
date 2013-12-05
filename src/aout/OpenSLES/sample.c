
#include "sles_private.h"

// set the sample interface. closing the old one.
int aout_sles_set_sample(rh_aout_itf self, rh_asmp_itf sample) {

	struct aout_instance * instance = (struct aout_instance *)self;

	int i;

	// update old samples reference counter.
	if(instance->audio_sample)
		(*instance->audio_sample)->close(&instance->audio_sample);

	instance->audio_sample = sample;

	// update new samples reference counter.
	if(instance->audio_sample)
		(*instance->audio_sample)->addref(instance->audio_sample);

	return 0;
}

// get the sample interface
int aout_sles_get_sample(rh_aout_itf self, rh_asmp_itf * sample) {

	struct aout_instance * instance = (struct aout_instance *)self;

	*sample = instance->audio_sample;

	return 0;
}

int aout_sles_read_sample(rh_aout_itf self, int frames, void * buffer) {

	struct aout_instance * instance = (struct aout_instance *)self;

	int ret = 0;

	if(instance->audio_sample)
		ret = (*instance->audio_sample)
			->read(instance->audio_sample, frames, buffer);

	return ret;
}

int aout_sles_atend_sample(rh_aout_itf self) {

	struct aout_instance * instance = (struct aout_instance *)self;

	int ret = 1;

	if(instance->audio_sample)
		ret = (*instance->audio_sample)
			->atend(instance->audio_sample);

	return ret;
}

int aout_sles_reset_sample(rh_aout_itf self) {

	struct aout_instance * instance = (struct aout_instance *)self;

	int ret = 0;

	if(instance->audio_sample)
		ret = (*instance->audio_sample)
			->reset(instance->audio_sample);

	return ret;
}

