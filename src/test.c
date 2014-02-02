
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

	rh_audio_itf itf0;
	rh_audio_itf itf1;

	FILE * promfile = fopen(argv[1],"rb");

	err = rh_audio_setup_api();
	printf("%d == rh_audio_setup_api()\n", err);
	err = rh_audio_create( &itf0 );
	err = rh_audio_create( &itf1 );
	printf("%d == rh_audio_create()\n", err);
	err = (*itf0)->openf( itf0, 0, RH_AUDIO_URL_FROM_FILEPTR( promfile, atoi(argv[2]) ) );
	err = (*itf1)->openf( itf1, 0, RH_AUDIO_URL_FROM_FILEPTR( promfile, atoi(argv[3]) ) );
	printf("%d == open()\n", err);
	err = (*itf0)->play( itf0 );
	err = (*itf1)->play( itf1 );
	printf("%d == play()\n", err);
	err = (*itf0)->wait( itf0 );
	err = (*itf1)->wait( itf1 );
	printf("%d == wait()\n", err);
	err = (*itf0)->close( &itf0 );
	err = (*itf1)->close( &itf1 );
	printf("%d == close()\n", err);
	err = rh_audio_shutdown_api();
	printf("%d == rh_audio_shutdown_api()\n", err);

	fclose(promfile);

	return 0;
}

