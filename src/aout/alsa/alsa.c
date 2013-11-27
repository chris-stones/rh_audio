

#include "alsa_private.h"

#include<pthread.h>

int rh_aout_create_alsa( rh_aout_itf * itf ) {

	{
		struct aout_instance * instance  = calloc(1, sizeof( struct aout_instance ) );
		struct rh_aout       * interface = calloc(1, sizeof( struct rh_aout       ) );

		if(!instance || !interface) {
			free(instance);
			free(interface);
			return -1;
		}

		instance->interface = interface;
		instance->status_flags = RH_AOUT_STATUS_STOPPED;

		interface->open  		= &aout_alsa_open;
		interface->close  		= &aout_alsa_close;
		interface->update 		= &aout_alsa_update;
		interface->play 		= &aout_alsa_play;
		interface->loop 		= &aout_alsa_loop;
		interface->stop   		= &aout_alsa_stop;
		interface->set_sample 	= &aout_alsa_set_sample;
		interface->get_sample   = &aout_alsa_get_sample;

		*itf = (rh_aout_itf)instance;

		return 0;
	}

	return -1;
}

