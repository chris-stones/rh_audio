/*
 * test.cpp
 *
 *  Created on: 12 Feb 2014
 *      Author: cds
 */


#include "rh_audio.hpp"


#define WITH_RH_RAW_LOADER 0
#define WITH_LIBESPROM 1

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


int main_libesprom(int argc, char ** argv ) {

	int err = 0;

	esprom_handle prom = NULL;
	esprom_alloc( argv[1], &prom);

	printf("%d == rh_audio_setup_api()\n", err);

	{
		// NOTE: samples constructed AFTER api is setup!
		rh::AudioSample sample0(0, RH_AUDIO_URL_PROM_LIBESPROM( prom, atoi(argv[2]) ) );
		rh::AudioSample sample1(0, RH_AUDIO_URL_PROM_LIBESPROM( prom, atoi(argv[3]) ) );

		sample0.Play();
		sample1.Loop();

		printf("looping - hit return to stop!\n");
		getchar();

		sample1.Stop();

		sample0.Wait();
		sample1.Wait();
		// NOTE: samples destructed BEFORE api is torn down!
	}

	printf("%d == rh_audio_shutdown_api()\n", err);

	esprom_free(prom);

	return 0;
}

int main(int argc, char ** argv ) {

	return main_libesprom(argc, argv);
}

