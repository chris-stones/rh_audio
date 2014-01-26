
#pragma once

#include "aout.h"

#include <pthread.h>
#include <string.h>
#include <stdlib.h>

/***************************************** OUTPUT CHANNEL INTERFACE *********************************/

struct rh_aout;

typedef const struct rh_aout * const * rh_aout_itf; /* RockHopper audio output interface */

struct rh_aout {

	int         (*open)        (rh_aout_itf  self, uint32_t channels, uint32_t samplerate, uint32_t samplesize, uint32_t bigendian);
	int         (*close)       (rh_aout_itf *self);
	int         (*update)      (rh_aout_itf  self);
	int         (*play)        (rh_aout_itf  self);
	int         (*loop)        (rh_aout_itf  self);
	int         (*stop)        (rh_aout_itf  self);
	int         (*set_sample)  (rh_aout_itf  self, rh_asmp_itf  sample);
	int         (*get_sample)  (rh_aout_itf  self, rh_asmp_itf *sample);
};

