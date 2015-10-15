
#define WITH_RH_RAW_LOADER 0
#define WITH_LIBESPROM 0
#define WITH_FILESYSTEM 1

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#if(WITH_RH_RAW_LOADER)
	#include<rh_raw_loader.h>
#endif
#if(WITH_LIBESPROM)
	#include<libesprom.h>
#endif
#include<stdio.h>
#include<unistd.h>
#include "rh_audio.h"

int main_libesprom(int argc, char ** argv ) {

	int err = 0;

	rh_audio_itf itf0;
	rh_audio_itf itf1;

#if(WITH_LIBESPROM)
	esprom_handle prom = NULL;
	esprom_alloc( argv[1], &prom);
#endif

	err = rh_audio_setup_api();
	printf("%d == rh_audio_setup_api()\n", err);
	err = rh_audio_create( &itf0 );
	err = rh_audio_create( &itf1 );
	printf("%d == rh_audio_create()\n", err);
#if(WITH_LIBESPROM)
	err = (*itf0)->openf( itf0, 0, RH_AUDIO_URL_PROM_LIBESPROM( prom, atoi(argv[2]) ) );
	err = (*itf1)->openf( itf1, 0, RH_AUDIO_URL_PROM_LIBESPROM( prom, atoi(argv[3]) ) );
#endif
#if(WITH_FILESYSTEM)
	err = (*itf0)->open( itf0, RH_AUDIO_URL_FILESYSTEM(argv[1]), 0 );
	err = (*itf1)->open( itf1, RH_AUDIO_URL_FILESYSTEM(argv[2]), 0 );
#endif
	printf("%d == open()\n", err);
	err = (*itf0)->play( itf0 );
	err = (*itf1)->loop( itf1 );
	printf("%d == play()\n", err);

	printf("looping - hit return to stop!\n");
	getchar();

	(*itf0)->stop(itf0);
	(*itf1)->stop(itf1);

	err = (*itf0)->wait( itf0 );
	err = (*itf1)->wait( itf1 );
	printf("%d == wait()\n", err);
	err = (*itf0)->close( &itf0 );
	err = (*itf1)->close( &itf1 );
	printf("%d == close()\n", err);
	err = rh_audio_shutdown_api();
	printf("%d == rh_audio_shutdown_api()\n", err);

#if(WITH_LIBESPROM)
	esprom_free(prom);
#endif

	return 0;
}

int main(int argc, char ** argv ) {

	return main_libesprom(argc, argv);
}

