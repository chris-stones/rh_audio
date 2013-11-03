
#include <alloca.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "bucket.h"

#include "rh_audio_internal.h"
//////////#include "../../rh_texture_packer.git/rh_texture_loader/rh_texture_loader.hpp"

#ifdef __cplusplus
extern "C" {
#endif

static bucket_handle sample_bucket = 0;

int rh_audiosample_create_internal_bucket() {

	return bucket_create( &sample_bucket );
}

int rh_audiosample_destroy_internal_bucket() {

	int e = bucket_free( sample_bucket );
	sample_bucket = NULL;
	return e;
}

int rh_audiosample_add_to_internal_bucket( rh_audiosample_handle sample ) {

	return bucket_add( sample_bucket, sample );
}

int rh_audiosample_remove_from_internal_bucket( rh_audiosample_handle sample ) {

	int e = -1;

	if(sample_bucket)
		e = bucket_remove(sample_bucket, sample);

	return e;
}

typedef enum {

	DOALL_ACTION_STOP,
	DOALL_ACTION_CLOSE,

} doall_action_enum_t;

static int _rh_audiosample_do_all(doall_action_enum_t action) {

	rh_audiosample_handle * samples = NULL;
	int samples_length = 0;

	if( bucket_lock( sample_bucket, (void***)&samples, &samples_length ) == 0) {

		rh_audiosample_handle * samples_copy = NULL;

		if(samples_length) {

			samples_copy = (rh_audiosample_handle *)alloca( sizeof(rh_audiosample_handle) * samples_length  );

			memcpy(samples_copy, samples, sizeof(rh_audiosample_handle) * samples_length);
		}

		bucket_unlock( sample_bucket );

		{
			int s;

			if(action == DOALL_ACTION_STOP)
				for(s = 0; s<samples_length; s++)
					rh_audiosample_stop( samples_copy[s] );
			else if(action == DOALL_ACTION_CLOSE)
				for(s = 0; s<samples_length; s++)
					rh_audiosample_close( samples_copy[s] );
		}
	}

	return 0;
}

int rh_audiosample_closeall() {

	return _rh_audiosample_do_all( DOALL_ACTION_CLOSE );
}

int rh_audiosample_stopall() {

	return _rh_audiosample_do_all( DOALL_ACTION_STOP );
}

int rh_audiosample_open_rawpak( rh_audiosample_handle * h, void * ctx, int flags) {

	char * resname = (char*)alloca(64);
	sprintf(resname,"rh_rawpak_ctx://%p", ctx);
	return rh_audiosample_open(h, resname, flags & ~RH_AUDIOSAMPLE_DONTCOPYSRC);
}

int rh_audiosample_open_s5prom(rh_audiosample_handle * h, FILE * promfile, int sound, int flags) {

	char * resname = (char*)alloca(64);
	sprintf(resname,"PROM://%p/%d", promfile, sound);
	return rh_audiosample_open(h, resname, flags & ~RH_AUDIOSAMPLE_DONTCOPYSRC);
}

#ifdef __cplusplus
} // extern "C" {
#endif

