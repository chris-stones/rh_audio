
#include "sles_private.h"

#include<pthread.h>

int rh_aout_create_sles( rh_aout_api_itf api, rh_aout_itf * itf ) {

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

		instance->api = api;

		interface->open  		= &aout_sles_open;
		interface->close  		= &aout_sles_close;
		interface->update 		= &aout_sles_update;
		interface->play 		= &aout_sles_play;
		interface->loop 		= &aout_sles_loop;
		interface->stop   		= &aout_sles_stop;
		interface->set_sample 	= &aout_sles_set_sample;
		interface->get_sample   = &aout_sles_get_sample;

		*itf = (rh_aout_itf)instance;

		return 0;
	}

	return -1;
}


