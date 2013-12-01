
/***
 *
 * Using ADPCM to decode audio data from a scorp-5 sound prom image.
 *
 *      This code supports reading audio from ...
 *        * PROM on the filesystem.
 *        * PROM in an android APK. ( TODO )
 *        * PROM in a rawpak container ( either on filesystem, or android APK ) ( TODO )
 *
 * example,
 *
 *     rh_asmp_itf itf;
 *     rh_asmp_create_s5prom( &itf, ... );
 *     (*itf)->openf( itf, "prom_file://%p/%d",   FILE,       sample_idx );  // load audio from file sounds/sound0.ogg
 *     (*itf)->openf( itf, "prom_asset://%p/%d",  asset,      sample_idx );  // load audio from a rawpak context.
 *     (*itf)->openf( itf, "prom_rawpak://%p/%d", rawpak_ctx, sample_idx );  // load audio from android assest sounds/sound0.ogg
 */


#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <byteswap.h>
#include <stdint.h>
#include <stdarg.h>
#include <linux/limits.h>

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
	FILE * 			asset;
	int 			sample_index;
	sample_header_t sample_header;
	size_t 			readpos;
	frame_t 		frame;
	pthread_mutex_t monitor;
};

static int is_little_endian() 	{ int n = 1; return(*(char *)&n == 1); }
static int is_big_endian() 		{ int n = 1; return(*(char *)&n == 0); }

static int _impl_on_output_event(rh_asmp_itf self, rh_output_event_enum_t ev) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	if( instance->cb_func )
		(*instance->cb_func)(instance->cb_data, ev);
}

static int _impl_open(rh_asmp_itf self, const char * const fn) {

  struct asmp_instance * instance = (struct asmp_instance *)self;

  // ADPCM - MUST BE A MULTIPLE OF 4!
  instance->frame.buffersize = 1000; // 125ms
  instance->frame.step = 0x7f;

  if(( instance->frame.buffer = malloc(instance->frame.buffersize) )) {

		// TODO ( load from rh rawpak ) : if(strncmp("rh_rawpak_ctx://",fn,16)==0) {

		if(strncmp("prom_file://",fn,12)==0) { /* e.g. "FILE://file_ptr/sample_id */

			void * p = NULL;
			int    sample_index = 0;
			short  nsamples;

			if(sscanf(fn,"prom_file://%p/%d",&p,&sample_index) != 2) {
				free(instance->frame.buffer);
				instance->frame.buffer = NULL;
				instance->frame.buffersize = 0;
				return -1;
			}

			instance->asset = (FILE*)p;
			instance->sample_index = sample_index;

			fseek(instance->asset, 14, SEEK_SET);
			fread(&nsamples, 2, 1, instance->asset);
			if(is_little_endian()) {
				nsamples = bswap_16(nsamples);
			}

			if(sample_index >= nsamples) {
				free(instance->frame.buffer);
				instance->frame.buffer = NULL;
				instance->frame.buffersize = 0;
				return -1;
			}

			fseek(instance->asset, 16 + 10 * sample_index, SEEK_SET);
			fread(&instance->sample_header, sizeof instance->sample_header, 1, instance->asset);
			if(is_little_endian())
			{
				instance->sample_header.freq  = bswap_16(instance->sample_header.freq);
				instance->sample_header.start = bswap_32(instance->sample_header.start);
				instance->sample_header.end   = bswap_32(instance->sample_header.end);
			}
		}
		else {

			free(instance->frame.buffer);
			instance->frame.buffer = NULL;
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

	int err = 0;

	err = fseek(instance->asset, instance->readpos + instance->sample_header.start, SEEK_SET);
	if(!err)
		err = fread(data, size, nmemb, instance->asset );

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

	instance->frame.nbsamples = 0;

	err = _read( instance->frame.buffer, 1, instance->frame.buffersize, self );

	if( err <= 0) {

		instance->ate = 1; // SET END OF STREAM
		return err;
	}

	instance->frame.nbsamples = err * 2; // ASSUMING 16bit mono,

	return err;
}

static int _impl_reset(rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	if(instance->frame.is_reset)
		return 0;

	_seek(self, 0, SEEK_SET);
	instance->ate= 0;

	// reset decoder state.
	instance->frame.signal = 0;
	instance->frame.step = 0x7f;

	// reset buffer state.
	instance->frame.processed_samples = 0;
	instance->frame.nbsamples = 0;

	// pre-load first frame.
	_adpcm_read_packet(self);

	// flag buffer as being in a reset state;
	instance->frame.is_reset = 1;

	return 0;
}

static int _impl_read(rh_asmp_itf self, int samples, void * dst) {

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

	samples &= (~1); // ASSUMING 16bit mono ( 2 samles per byte )

	if(instance->frame.processed_samples >= instance->frame.nbsamples ) {

		instance->frame.processed_samples = 0;
		_adpcm_read_packet(self);
	}

	{
		// ASSUMING 16bit mono
		int samplesRemainingInFrame = (instance->frame.nbsamples - instance->frame.processed_samples);
		if(samples > samplesRemainingInFrame)
			samples = samplesRemainingInFrame;
	}

	// ADPCM DECODE
	{
		uint32_t Position = 0;
		uint32_t size = (samples/2); // ASSUMING 16bit mono
		int8_t * outBuffer = (int8_t *)(dst);
		uint8_t * srcBuffer = instance->frame.buffer + (instance->frame.processed_samples / 2);
		while(Position != size)
		{
			/* compute the new amplitude and update the current step */
			uint8_t Data = srcBuffer[Position] >> 4;
			instance->frame.signal += (instance->frame.step * diff_lookup[Data & 15]) / 8;

			/* clamp to the maximum */
			if (instance->frame.signal > 32767) instance->frame.signal = 32767; else if (instance->frame.signal < -32768) instance->frame.signal = -32768;

			/* adjust the step size and clamp */
			instance->frame.step = (instance->frame.step * index_scale[Data & 7]) >> 8;

			if (instance->frame.step > 0x6000) instance->frame.step = 0x6000; else if (instance->frame.step < 0x7f) instance->frame.step = 0x7f;

			/* output to the buffer, scaling by the volume */
			if(!is_big_endian()) {
				*outBuffer++ = (int8_t)(instance->frame.signal & 0xff);
				*outBuffer++ = (int8_t)((instance->frame.signal >> 8) & 0xff);
			} else {
				*outBuffer++ = (int8_t)((instance->frame.signal >> 8) & 0xff);
				*outBuffer++ = (int8_t)(instance->frame.signal & 0xff);
			}

			//added part
			Data = srcBuffer[Position] & 0x0F;
			instance->frame.signal += (instance->frame.step * diff_lookup[Data & 15]) / 8;

			/* clamp to the maximum */
			if (instance->frame.signal > 32767) instance->frame.signal = 32767; else if (instance->frame.signal < -32768) instance->frame.signal = -32768;

			/* adjust the step size and clamp */
			instance->frame.step = (instance->frame.step * index_scale[Data & 7]) >> 8;

			if (instance->frame.step > 0x6000) instance->frame.step = 0x6000; else if (instance->frame.step < 0x7f) instance->frame.step = 0x7f;

			/* output to the buffer, scaling by the volume */
			if(!is_big_endian()) {
				*outBuffer++ = (int8_t)(instance->frame.signal & 0xff);
				*outBuffer++ = (int8_t)((instance->frame.signal >> 8) & 0xff);
			} else {
				*outBuffer++ = (int8_t)((instance->frame.signal >> 8) & 0xff);
				*outBuffer++ = (int8_t)(instance->frame.signal & 0xff);
			}

			Position++;
		}
	}

	instance->frame.processed_samples  += samples;

	if(samples)
		instance->frame.is_reset = 0;

    return samples;
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

	return 16000; // 16khz
}

static int _impl_channels(rh_asmp_itf pself) {

	return 1; // mono
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
		interface->atend 		= &_impl_atend;
		interface->close 		= &_impl_close;
		interface->samplerate 	= &_impl_samplerate;
		interface->samplesize 	= &_impl_samplesize;
		interface->channels		= &_impl_channels;

		interface->on_output_event = &_impl_on_output_event;

		*itf = (rh_asmp_itf)instance;

		return 0;
	}

	return -1;
}

