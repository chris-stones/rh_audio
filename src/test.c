
#include<stdio.h>

#include "rh_audio.h"

int main(int argc, char ** argv ) {

	rh_audiosample_handle sample = 0;

	rh_audiosample_setup();

	rh_audiosample_open(&sample, argv[1], RH_AUDIOSAMPLE_DONTCOPYSRC);

	rh_audiosample_play(sample);

//	rh_audiosample_wait(sample);
	getchar();
	rh_audiosample_stop(sample);

	rh_audiosample_shutdown();

	return 0;
}

