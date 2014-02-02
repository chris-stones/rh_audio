

#include <stdio.h>

#include "embedded_private.h"

static int _aout_embedded_open(rh_aout_itf self, unsigned int channels, unsigned int samplerate, unsigned int samplesize, int bigendian) {

//struct aout_instance * instance = (struct aout_instance *)self;

  if(channels != 1)
	  return -1;

  if(bigendian != 1)
	  return -1;

  if(samplesize != 2)
	  return -1;

// TODO: does the driver support 16khz? or to i need to re-sample to 48khz?
//  if(samplerate != 16000)
//	  return -1;

  return 0;
}

int aout_embedded_open(rh_aout_itf self, uint32_t channels, uint32_t samplerate, uint32_t samplesize, uint32_t bigendian) {

	struct aout_instance * instance = (struct aout_instance *)self;

	if( instance )
		return 0;

	return _aout_embedded_open(self, channels, samplerate, samplesize, bigendian);
}
