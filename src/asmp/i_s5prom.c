
/***
 * Read in audio-data through ADPCM decoder.
 */

// RH_RAW_LOADER is only really needed as an Android disk/asset IO wrapper!
#if defined(__ANDROID__)
#define WITH_RH_RAW_LOADER 0
#endif

// ADPCM - MUST BE A MULTIPLE OF 4! ( 1000 bytes == 125 milliseconds @ 16khz )
//#define S5PROM_MAX_DISK_BUFFER_SIZE (2 * 1024 * 1024)
#define S5PROM_MAX_DISK_BUFFER_SIZE (128)

// TODO: volume control!
#define VOLUME 10 // was 500 !?


#define RESAMPLE_48_KHZ 0

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <byteswap.h>
#include <stdint.h>
#include <stdarg.h>
#include <linux/limits.h>

#if WITH_RH_RAW_LOADER
#include<rh_raw_loader.h>
#endif

#include "asmp.h"

struct sample_header_struct {

	short freq;
	int   start;
	int   end;

} __attribute__((packed));

typedef struct sample_header_struct sample_header_t;

struct frame {

	uint8_t * buffer;
	size_t buffersize;
	size_t nbsamples;
	size_t processed_samples;
	int32_t signal;
	int32_t step;
	uint8_t is_reset;

#if(RESAMPLE_48_KHZ)
	int8_t phase;
#endif

	int8_t mixBuffer[4];
};

typedef struct frame frame_t;

struct asmp_instance {

	// interface ptr must be the first item in the instance.
	struct rh_asmp * interface;

	// private data
	asmp_cb_func_t  cb_func;
	void        *   cb_data;
	int				ref;
	int 			ate;
	FILE * 			asset_file;
#if WITH_RH_RAW_LOADER
	rh_rawpak_ctx   asset_pak;
#endif
	int 			sample_index;
	sample_header_t sample_header;
	size_t 			readpos;
	frame_t 		frame;
	pthread_mutex_t monitor;
};

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define RH_BIG_ENDIAN
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define RH_LITTLE_ENDIAN
#endif

#if defined(RH_LITTLE_ENDIAN)
	#define RH_IS_LITTLE_ENDIAN 1
	#define RH_IS_BIG_ENDIAN 0
#elif defined(RH_BIG_ENDIAN)
	#define RH_IS_LITTLE_ENDIAN 0
	#define RH_IS_BIG_ENDIAN 1
#else
	#error cannot determine endianness!
#endif

static int _impl_on_output_event(rh_asmp_itf self, rh_output_event_enum_t ev) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	int e = 0;

	if( instance->cb_func )
		e = (*instance->cb_func)(instance->cb_data, ev);

	return e;
}

static int _read_from_asset(rh_asmp_itf self, size_t pos, size_t size, size_t nmemb, void * dst) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	int e = -1;

	if(instance->asset_file) {

		if(fseek(instance->asset_file, pos, SEEK_SET)==0)
			e = fread(dst, size, nmemb, instance->asset_file);
	}
#if WITH_RH_RAW_LOADER
	else if( instance->asset_pak ) {

		if(rh_rawpak_seek(instance->asset_pak, pos, SEEK_SET)==0)
			e = rh_rawpak_read(dst, size, nmemb, instance->asset_pak );
	}
#endif

	return e;
}

static int _impl_open(rh_asmp_itf self, const char * const fn) {

  struct asmp_instance * instance = (struct asmp_instance *)self;


  const int buffersize = S5PROM_MAX_DISK_BUFFER_SIZE;

  instance->frame.buffersize = 0;
  instance->frame.buffer = NULL;
  instance->frame.step = 0x7f;

//if(( instance->frame.buffer = malloc(instance->frame.buffersize) ))
  {
	  	FILE * file         = NULL;
#if WITH_RH_RAW_LOADER
	  	rh_rawpak_ctx pak   = NULL;
#endif
	  	int    sample_index = 0;
	  	short  nsamples     = 0;

	  	if(
	  		  (sscanf(fn,"prom_fileptr://%p/%d" ,&file,&sample_index) != 2)
#if WITH_RH_RAW_LOADER
	  	   && (sscanf(fn,"prom_rawpak://%p/%d",&pak, &sample_index) != 2)
#endif
	  	  )
	  	{
			return -1;
	  	}

		instance->asset_file = file;
#if WITH_RH_RAW_LOADER
		instance->asset_pak  = pak;
#endif
		instance->sample_index = sample_index;

		if( _read_from_asset(self, 14, 2, 1, &nsamples) != 1 )
			nsamples = 0;

		if(RH_IS_LITTLE_ENDIAN) {
			nsamples = bswap_16(nsamples);
		}

		if(sample_index >= nsamples)
			return -1;

		if( _read_from_asset(
				self,
				16 + 10 * sample_index,
				sizeof instance->sample_header, 1,
				&instance->sample_header) != 1 )
		{
			nsamples = 0;
		}

		if(RH_IS_LITTLE_ENDIAN)
		{
			instance->sample_header.freq  = bswap_16(instance->sample_header.freq);
			instance->sample_header.start = bswap_32(instance->sample_header.start);
			instance->sample_header.end   = bswap_32(instance->sample_header.end);
		}

		instance->frame.buffersize = buffersize;
		if(instance->frame.buffersize >= (instance->sample_header.end - instance->sample_header.start) )
			instance->frame.buffersize = (instance->sample_header.end - instance->sample_header.start);

		if((instance->frame.buffer = malloc( instance->frame.buffersize )) == NULL ) {
			instance->frame.buffersize = 0;
			return -1;
		}

		// reset all state, and pre-load first packet.
		instance->interface->reset(self);

		return 0;
  }

  return -1;
}

static int _impl_openf(rh_asmp_itf self, const char * const format, ...) {

	int err = 0;
	char *path = NULL;
	va_list va;
	va_start(va, format);
    if(!((path = malloc(sizeof (char) * PATH_MAX))))
       err = -1;
    else if(vsnprintf(path,PATH_MAX,format,va)>=PATH_MAX)
        err = -1; /* truncated */
    va_end(va);

	if(!err)
		err = _impl_open(self, path);

	free(path);

	return err;
}

static int _seek(rh_asmp_itf self, long offset, int whence) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	switch(whence) {
		case SEEK_SET:
			instance->readpos = offset;
			break;
		case SEEK_CUR:
			instance->readpos += offset;
			break;
		case SEEK_END:
			instance->readpos = (instance->sample_header.end - instance->sample_header.start) + offset;
			break;
	}

	if(instance->readpos >= 0 && instance->readpos <= (instance->sample_header.end - instance->sample_header.start) )
		return 0;

	return -1;
}

static inline int _read(void* data, size_t size, size_t nmemb, rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	int err =
		_read_from_asset(
			self,
			instance->readpos + instance->sample_header.start,
			size,nmemb,
			data);

	if( err < 0)
		return err;

	instance->readpos += size * err;

	if(instance->readpos > (instance->sample_header.end - instance->sample_header.start) ) {
		instance->readpos = (instance->sample_header.end - instance->sample_header.start);
		return -1; // read past end of embedded file.
	}

	return err;
}

static int _adpcm_read_packet(rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	int err = 0;

	size_t s = instance->sample_header.end - instance->sample_header.start;

#if(RESAMPLE_48_KHZ)
	s *= 6;
#else
	s *= 2;
#endif

	if(instance->frame.nbsamples == s) {

		// frame buffer holds entire sample, avoid the disk!
		if( instance->frame.processed_samples >= instance->frame.nbsamples ) {

			// paranoia!
			instance->frame.processed_samples = instance->frame.nbsamples;

			// at-end - set the flag!
			instance->ate = 1;
		}
		return err;
	}
	else {

		instance->frame.nbsamples = 0;
		instance->frame.processed_samples = 0;

		err = _read( instance->frame.buffer, 1, instance->frame.buffersize, self );

		if( err <= 0) {

			instance->ate = 1; // SET END OF STREAM
			return err;
		}

#if(RESAMPLE_48_KHZ)
	instance->frame.nbsamples = err * 6;
#else
	instance->frame.nbsamples = err * 2;
#endif

		return err;
	}
}

static int _impl_reset(rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	if(instance->frame.is_reset)
		return 0;

	_seek(self, 0, SEEK_SET);

	// reset decoder state.
	instance->frame.signal = 0;
	instance->frame.step = 0x7f;
#if(RESAMPLE_48_KHZ)
	instance->frame.phase = 0;
#endif

	// pre-load first frame.
	instance->frame.processed_samples = 0;
	_adpcm_read_packet(self);

	// flag buffer as being in a reset state;
	instance->frame.is_reset = 1;

	// clear at-end flag.
	instance->ate= 0;

	return 0;
}

typedef	union {
	int16_t value;
	struct {
#if RH_IS_LITTLE_ENDIAN
		uint8_t lower;
		uint8_t upper;
#else
		uint8_t upper;
		uint8_t lower;
#endif
	} field;
} audio_sample16_t;

static int _de_adpcm(rh_asmp_itf self, int samples, void * dst, int mixmode) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	static int diff_lookup[16] =
	{
		1,3,5,7,9,11,13,15,-1,-3,-5,-7,-9,-11,-13,-15
	};

	static int index_scale[16] =
	{
		0x0e6,0x0e6,0x0e6,0x0e6,0x133,0x199,0x200,0x266,
		0x0e6,0x0e6,0x0e6,0x0e6,0x133,0x199,0x200,0x266
	};

	int ret = 0;

	if(instance->frame.processed_samples >= instance->frame.nbsamples )
		_adpcm_read_packet(self);

	{
		// ASSUMING 16bit mono
		int samplesRemainingInFrame = (instance->frame.nbsamples - instance->frame.processed_samples);
		if(samples > samplesRemainingInFrame)
			samples = samplesRemainingInFrame;
	}

	samples &= ( ~1 ); // ASSUMING 16bit mono ( 2 samples per byte )

	// ADPCM DECODE
	{
		uint32_t Position = 0;
		 int32_t samplesRemaining = samples;
		int8_t * outBuffer = (int8_t *)(dst);

#if(RESAMPLE_48_KHZ)
		uint8_t * srcBuffer = instance->frame.buffer +
				(instance->frame.processed_samples / 6);
#else
		uint8_t * srcBuffer = instance->frame.buffer +
				(instance->frame.processed_samples / 2);
#endif

		int8_t *mixBuffer = instance->frame.mixBuffer;

		while(samplesRemaining >= 2) {

#if(RESAMPLE_48_KHZ)
			if(instance->frame.phase == 0)
#endif
			{
				/* compute the new amplitude and update the current step */
				uint8_t Data = srcBuffer[Position] >> 4;
				instance->frame.signal += (instance->frame.step * diff_lookup[Data & 15]) / 8;

				/* clamp to the maximum */
				if (instance->frame.signal > 32767) instance->frame.signal = 32767; else if (instance->frame.signal < -32768) instance->frame.signal = -32768;

				/* adjust the step size and clamp */
				instance->frame.step = (instance->frame.step * index_scale[Data & 7]) >> 8;

				if (instance->frame.step > 0x6000) instance->frame.step = 0x6000; else if (instance->frame.step < 0x7f) instance->frame.step = 0x7f;

				/* output to the buffer */
				mixBuffer[0] = (int8_t)((instance->frame.signal >> 8) & 0xff);
				mixBuffer[1] = (int8_t)(instance->frame.signal & 0xff);
				//added part
				Data = srcBuffer[Position] & 0x0F;
				instance->frame.signal += (instance->frame.step * diff_lookup[Data & 15]) / 8;

				/* clamp to the maximum */
				if (instance->frame.signal > 32767) instance->frame.signal = 32767; else if (instance->frame.signal < -32768) instance->frame.signal = -32768;

				/* adjust the step size and clamp */
				instance->frame.step = (instance->frame.step * index_scale[Data & 7]) >> 8;

				if (instance->frame.step > 0x6000) instance->frame.step = 0x6000; else if (instance->frame.step < 0x7f) instance->frame.step = 0x7f;

				/* output to the buffer  */
				mixBuffer[2] = (int8_t)((instance->frame.signal >> 8) & 0xff);
				mixBuffer[3] = (int8_t)(instance->frame.signal & 0xff);

				Position++;
			}

#if(RESAMPLE_48_KHZ)
			instance->frame.phase++;
			if(instance->frame.phase >= 3)
				instance->frame.phase = 0;
#endif

			// audio-data is big-endian.

			if(mixmode) {

#if RH_IS_BIG_ENDIAN
				(*(int16_t*)(outBuffer+0)) += (*(int16_t*)mixBuffer+0) / VOLUME;
				(*(int16_t*)(outBuffer+2)) += (*(int16_t*)mixBuffer+2) / VOLUME;
#else
		// switch to CPU endian-ness, volume adjust, mix, and convert back to big-endian.
				audio_sample16_t d0,d1,s0,s1;
				d0.field.upper = outBuffer[0];
				d0.field.lower = outBuffer[1];
				d1.field.upper = outBuffer[2];
				d1.field.lower = outBuffer[3];
				s0.field.upper = mixBuffer[0];
				s0.field.lower = mixBuffer[1];
				s1.field.upper = mixBuffer[2];
				s1.field.lower = mixBuffer[3];
				d0.value += s0.value / VOLUME;
				d1.value += s1.value / VOLUME;

				(*(int16_t*)(outBuffer+0)) += bswap_16(d0.value);
				(*(int16_t*)(outBuffer+2)) += bswap_16(d1.value);
#endif
			}
			else {
#if RH_IS_BIG_ENDIAN
				(*(int16_t*)(outBuffer+0)) = (*(int16_t*)(mixBuffer+0)) / VOLUME;
				(*(int16_t*)(outBuffer+2)) = (*(int16_t*)(mixBuffer+2)) / VOLUME;
#else
	// switch to CPU endian-ness, volume adjust and convert back to big-endian.

				audio_sample16_t s0,s1;
				s0.field.upper = mixBuffer[0];
				s0.field.lower = mixBuffer[1];
				s1.field.upper = mixBuffer[2];
				s1.field.lower = mixBuffer[3];

				(*(int16_t*)(outBuffer+0)) = bswap_16(s0.value / VOLUME);
				(*(int16_t*)(outBuffer+2)) = bswap_16(s1.value / VOLUME);
#endif
			}

			outBuffer+=4;
			samplesRemaining-=2;
		}
	}

	instance->frame.processed_samples  += samples;

	if(samples)
		instance->frame.is_reset = 0;

    return samples;
}

static int _impl_read(rh_asmp_itf self, int samples, void * dst) {

	return _de_adpcm(self, samples, dst, 0);
}

static int _impl_mix(rh_asmp_itf self, int samples, void * dst) {

	return _de_adpcm(self, samples, dst, 1);
}

static int _impl_atend(rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	return instance->ate;
}

static int _impl_close(rh_asmp_itf *pself) {

  struct asmp_instance * instance = (struct asmp_instance *)(*pself);

  if(instance) {

	  int ref;

	  if( pthread_mutex_lock(&instance->monitor) != 0 )
		  return -1;
	  ref = --(instance->ref);
	  pthread_mutex_unlock(&instance->monitor);

	  if(ref == 0) {
		free( instance->interface    );
		free( instance->frame.buffer );
		free( instance );
	  }

	  *pself = NULL;
  }
  return 0;
}

static int _impl_samplesize(rh_asmp_itf pself) {

	return 2; // signed 16bit audio
}

static int _impl_samplerate(rh_asmp_itf pself) {

#if(RESAMPLE_48_KHZ)
	return 48000; // 48khz
#else
	return 16000; // 16khz
#endif
}

static int _impl_channels(rh_asmp_itf pself) {

	return 1; // mono
}

static int _impl_bigendian(rh_asmp_itf pself) {

	return 1; // big endian!
}

static rh_asmp_itf _impl_addref(rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	if( pthread_mutex_lock(&instance->monitor) != 0 )
		return NULL;

	instance->ref++;

	pthread_mutex_unlock(&instance->monitor);

	return self;
}

int rh_asmp_create_s5prom( rh_asmp_itf * itf, asmp_cb_func_t cb_func, void * cb_data ) {

	{
		struct asmp_instance * instance  = calloc(1, sizeof( struct asmp_instance ) );
		struct rh_asmp       * interface = calloc(1, sizeof( struct rh_asmp       ) );

		if(!instance || !interface) {
			free(instance);
			free(interface);
			return -1;
		}

		if(pthread_mutex_init(&instance->monitor, NULL)!= 0) {
			free(interface);
			free(instance);
			return -1;
		}

		instance->ref = 1;
		instance->interface = interface;
		instance->cb_func = cb_func;
		instance->cb_data = cb_data;

		interface->open  		= &_impl_open;
		interface->openf        = &_impl_openf;
		interface->addref       = &_impl_addref;
		interface->reset		= &_impl_reset;
		interface->read			= &_impl_read;
		interface->mix			= &_impl_mix;
		interface->atend 		= &_impl_atend;
		interface->close 		= &_impl_close;
		interface->samplerate 	= &_impl_samplerate;
		interface->samplesize 	= &_impl_samplesize;
		interface->channels		= &_impl_channels;
		interface->is_bigendian = &_impl_bigendian;

		interface->on_output_event = &_impl_on_output_event;

		*itf = (rh_asmp_itf)instance;

		return 0;
	}

	return -1;
}

