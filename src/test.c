
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<rh_raw_loader.h>

#include "rh_audio.h"

int main(int argc, char ** argv ) {

	int err = 0;

	rh_rawpak_handle rawpak_handle 	= 0;
	rh_rawpak_ctx rawpak_ctx 		= 0;
	rh_audiosample_handle sample 	= 0;

	err = rh_rawpak_open(argv[1], &rawpak_handle);
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);
	err = rh_rawpak_open_ctx(rawpak_handle, argv[2], &rawpak_ctx);
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);
	err = rh_audiosample_setup();
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);
	err = rh_audiosample_open_rawpak(&sample, rawpak_ctx, 0);
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);
	err = rh_audiosample_play(sample);
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);
	getchar();
	err = rh_audiosample_stop(sample);
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);
	err = rh_audiosample_shutdown();
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);
	return 0;
}

