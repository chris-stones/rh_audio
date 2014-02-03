
#pragma once

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif /** __cplusplus **/

typedef enum {

	RH_ASMP_OUTPUT_EVENT_STARTED = (1<<0),
	RH_ASMP_OUTPUT_EVENT_STOPPED = (1<<1),
	RH_ASMP_OUTPUT_EVENT_ERROR   = (1<<2),
	RH_ASMP_OUTPUT_EVENT_SYNC    = (1<<3),

} rh_output_event_enum_t;

typedef int (*asmp_cb_func_t)(void * data, rh_output_event_enum_t ev);

struct rh_asmp;

typedef const struct rh_asmp * const * rh_asmp_itf; /* RockHopper audiosample interface */

struct rh_asmp {

	int         (*open)            (rh_asmp_itf  self, const char * const fn);
	int         (*openf)           (rh_asmp_itf  self, const char * const fn, ...);
	int         (*close)           (rh_asmp_itf *self); /* thread safe */
	rh_asmp_itf (*addref)          (rh_asmp_itf  self); /* thread safe */
	int         (*atend)           (rh_asmp_itf  self);
	int         (*reset)           (rh_asmp_itf  self);
	int         (*read)            (rh_asmp_itf  self, int frames, void * dst);
	int         (*mix)             (rh_asmp_itf  self, int frames, void * dst); /* OPTIONAL - NULL TEST BEFORE USE! */
	int         (*channels)        (rh_asmp_itf  self);
	int         (*samplerate)      (rh_asmp_itf  self);
	int         (*samplesize)      (rh_asmp_itf  self);
	int         (*is_bigendian)    (rh_asmp_itf  self); /* OPTIONAL - NULL TEST BEFORE USE! */

	/*** TODO:
	 * optional get/set volume!
	 */

	int         (*on_output_event)(rh_asmp_itf  self, rh_output_event_enum_t ev); // CALLED BY THE AUDIO OUTPUT INTERFACE IMPLEMENTATION
};

typedef enum {

	RH_ASMP_IMP_FFMPEG = (1<<0),
	RH_ASMP_IMP_S5PROM = (1<<1),

	RH_ASMP_IMP_DEFAULT = RH_ASMP_IMP_FFMPEG,

} rh_asmp_imp_enum_t;

int rh_asmp_create( rh_asmp_itf * itf, rh_asmp_imp_enum_t implementation, asmp_cb_func_t cb, void * cb_data );

#ifdef __cplusplus
} // extern "C" {
#endif /** __cplusplus **/

