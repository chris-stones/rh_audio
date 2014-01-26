
#define WITH_RH_RAW_LOADER 0

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#if(WITH_RH_RAW_LOADER)
	#include<rh_raw_loader.h>
#endif
#include<stdio.h>
#include<unistd.h>
#include "rh_audio.h"

int main(int argc, char ** argv ) {

	int err = 0;

	rh_audio_itf itf;

	FILE * promfile = fopen(argv[1],"rb");

	err = rh_audio_setup_api();
	printf("%d == rh_audio_setup_api()\n", err);
	err = rh_audio_create( &itf );
	printf("%d == rh_audio_create()\n", err);
	err = (*itf)->openf( itf, 0, RH_AUDIO_URL_FROM_FILEPTR( promfile, atoi(argv[2]) ) );
	printf("%d == open()\n", err);
	err = (*itf)->play( itf );
	printf("%d == play()\n", err);
	err = (*itf)->wait( itf );
	printf("%d == wait()\n", err);
	err = (*itf)->close( &itf );
	printf("%d == close()\n", err);
	err = rh_audio_shutdown_api();
	printf("%d == rh_audio_shutdown_api()\n", err);

	fclose(promfile);

	return 0;
}

