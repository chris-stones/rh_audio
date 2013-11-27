

#include<stdlib.h>
#include<pthread.h>
#include<string.h>
#include<stdio.h>

#include "asmp.h"

int rh_asmp_create( rh_asmp_itf * itf, rh_asmp_imp_enum_t implementation, asmp_cb_func_t cb, void * cb_data ) {

	if( implementation & RH_ASMP_IMP_FFMPEG ) {
		int rh_asmp_create_ffmpeg( rh_asmp_itf * itf, asmp_cb_func_t cb, void * cb_data );
		if( rh_asmp_create_ffmpeg(itf, cb, cb_data) == 0 )
			return 0;
	}

	if( implementation & RH_ASMP_IMP_S5PROM ) {
		int rh_asmp_create_s5prom( rh_asmp_itf * itf, asmp_cb_func_t cb, void * cb_data );
		if( rh_asmp_create_s5prom(itf, cb, cb_data) == 0 )
			return 0;
	}

	return -1;
}

