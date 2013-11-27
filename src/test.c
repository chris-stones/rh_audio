
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<rh_raw_loader.h>
#include<stdio.h>
#include<unistd.h>
#include "rh_audio.h"

int main(int argc, char ** argv ) {

	int err = 0;

	rh_audio_itf itf;

	err = rh_audio_setup_api();
	printf("%d == rh_audio_setup_api()\n", err);
	err = rh_audio_create( &itf );
	printf("%d == rh_audio_create()\n", err);
	err = (*itf)->open( itf, argv[1], 0 );
	printf("%d == open()\n", err);
	err = (*itf)->play( itf );
	printf("%d == play()\n", err);
	err = (*itf)->wait( itf );
	printf("%d == wait()\n", err);
	err = (*itf)->close( &itf );
	printf("%d == close()\n", err);
	err = rh_audio_shutdown_api();
	printf("%d == rh_audio_shutdown_api()\n", err);

	return 0;
}

