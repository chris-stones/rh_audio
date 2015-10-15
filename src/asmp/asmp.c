
#include<stdlib.h>
#include<pthread.h>
#include<string.h>
#include<stdio.h>

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include "asmp.h"

int rh_asmp_create( rh_asmp_itf * itf, rh_asmp_imp_enum_t implementation, asmp_cb_func_t cb, void * cb_data ) {

#if defined(RH_WITH_FFMPEG)
	if( implementation & RH_ASMP_IMP_FFMPEG ) {
		int rh_asmp_create_ffmpeg( rh_asmp_itf * itf, asmp_cb_func_t cb, void * cb_data );
		if( rh_asmp_create_ffmpeg(itf, cb, cb_data) == 0 )
			return 0;
	}
#endif

#if defined(RH_WITH_S5PROM)
	if( implementation & RH_ASMP_IMP_S5PROM ) {
		int rh_asmp_create_s5prom( rh_asmp_itf * itf, asmp_cb_func_t cb, void * cb_data );
		if( rh_asmp_create_s5prom(itf, cb, cb_data) == 0 )
			return 0;
	}
#endif

	return -1;
}

