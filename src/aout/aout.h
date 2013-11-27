
#pragma once

#include<stdlib.h>
#include<stdint.h>

#include "../asmp/asmp.h"

#ifdef __cplusplus
extern "C" {
#endif /** __cplusplus **/

/***************************************** API OUTPUT API INTERFACE *********************************/

struct rh_aout_api;

typedef const struct rh_aout_api * const * rh_aout_api_itf;

struct rh_aout_api {

	int    (*setup)    (rh_aout_api_itf  self);
	int    (*shutdown) (rh_aout_api_itf *self);
	int    (*play)     (rh_aout_api_itf  self, rh_asmp_itf sample);
	int    (*loop)     (rh_aout_api_itf  self, rh_asmp_itf sample);
	int    (*stop)     (rh_aout_api_itf  self, rh_asmp_itf sample);
	int    (*sync)     (rh_aout_api_itf  self, rh_asmp_itf sample);
};

int rh_aout_create_api( rh_aout_api_itf * itf );

#ifdef __cplusplus
} /* extern "C" { */
#endif /** __cplusplus **/

