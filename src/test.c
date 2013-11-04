
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<rh_raw_loader.h>
#include<stdio.h>
#include<unistd.h>
#include "rh_audio.h"

int main(int argc, char ** argv ) {

	int err = 0;

	rh_rawpak_handle rawpak_handle 	= 0;
	rh_rawpak_ctx rawpak_ctx 		= 0;
	rh_audiosample_handle sample 	= 0;
	FILE * prom = NULL;

	err = rh_audiosample_setup();
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);

	/* TEST PLAYING AUDIO FROM RAWPAK FILE
	err = rh_rawpak_open(argv[1], &rawpak_handle, RH_RAWPAK_APP);
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);
	err = rh_rawpak_open_ctx(rawpak_handle, argv[2], &rawpak_ctx);
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);
	err = rh_audiosample_open_rawpak(&sample, rawpak_ctx, 0);
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);
	*/



	/* TEST PLAYING AUDIO FROM SCORP5 SOUND FROM FILE  */
	prom = fopen(argv[1],"rb");
//	prom = fopen("/home/cds/DROSSND/ALICECLUB.bin", "rb");

	if(!prom) printf("prom == %p ( line %d )\n", prom, __LINE__);
	err = rh_audiosample_open_s5prom(&sample, prom, atoi(argv[2]), 0);
//	err = rh_audiosample_open_s5prom(&sample, prom, atoi("33"), 0);
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);

	err = rh_audiosample_play(sample);

	usleep(2000000);

	err = rh_audiosample_play(sample);

//	err = rh_audiosample_loop(sample);
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);
//	getchar();

	rh_audiosample_wait(sample);
//	while(rh_audiosample_isplaying(sample) > 0)
//		usleep(100);

	err = rh_audiosample_stop(sample);
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);

	rh_audiosample_close(sample);

	err = rh_audiosample_shutdown();
	if(err) printf("err == %d ( line %d )\n", err, __LINE__);

	if(prom)
		fclose(prom);

	return 0;
}

