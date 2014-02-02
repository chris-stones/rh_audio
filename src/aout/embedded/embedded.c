
#include "embedded_private.h"

int rh_aout_create_embedded( rh_aout_itf * itf ) {

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

		interface->open  		= &aout_embedded_open;
		interface->close  		= &aout_embedded_close;
		interface->update 		= &aout_embedded_update;
		interface->play 		= &aout_embedded_play;
		interface->loop 		= &aout_embedded_loop;
		interface->stop   		= &aout_embedded_stop;
		interface->set_sample 	= &aout_embedded_set_sample;
		interface->get_sample   = &aout_embedded_get_sample;

		*itf = (rh_aout_itf)instance;

		return 0;
	}

	return -1;
}

