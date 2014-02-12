
/***
 * Read in audio-data through ADPCM decoder.
 */

//#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
  #define DEBUG_PRINTF(...) do{}while(0)

// TODO: per sample volume ?
#define VOLUME 10 // was 500 !?

// resample 16khz prom to 48khz?
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

#include <libesprom.h>

#include "asmp.h"

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define RH_BIG_ENDIAN
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define RH_LITTLE_ENDIAN
#endif

#if defined(RH_LITTLE_ENDIAN)
	#define CPU_TO_BE_16(x) (bswap_16((x)))
	#define BE_TO_CPU_16(x) (bswap_16((x)))
	#define COPY_TWO_CPU16_TO_BE16(out, in)\
	do {\
		((uint16_t*)out)[0] = CPU_TO_BE_16(((uint16_t*)in)[0]);\
		((uint16_t*)out)[1] = CPU_TO_BE_16(((uint16_t*)in)[1]);\
	}while(0)
#elif defined(RH_BIG_ENDIAN)
	#define CPU_TO_BE_16(x) ((x))
    #define BE_TO_CPU_16(x) ((x))
	#define COPY_TWO_CPU16_TO_BE16(out, in)\
		(*((uint32_t*)(out))) = (*((uint32_t*)(in)))
#else
	#error cannot determine endianness!
#endif


#define BE_MIX_CPU_16(be_out16, cpu_in16)\
	be_out16 =  CPU_TO_BE_16( BE_TO_CPU_16(be_out16) + cpu_in16 )

typedef struct sample_header_struct sample_header_t;

typedef	union {
	int16_t value;
	struct {
#if defined(RH_LITTLE_ENDIAN)
		uint8_t lower;
		uint8_t upper;
#else
		uint8_t upper;
		uint8_t lower;
#endif
	} field;
} audio_sample16_t;

struct frame {

	esprom_sample_handle asset_libesprom;

	uint8_t * buffer;
	size_t buffersize;

	size_t nbsamples;
	size_t processed_samples;
	int32_t signal;
	int32_t step;
	uint8_t is_reset;

	audio_sample16_t s0; /* sample 0, store in-case of re-sample */
	audio_sample16_t s1; /* sample 1, store in-case of re-sample */

#if(RESAMPLE_48_KHZ)
	int8_t phase;
#endif
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
	frame_t 		frame;
	pthread_mutex_t monitor;
};

static int _impl_on_output_event(rh_asmp_itf self, rh_output_event_enum_t ev) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	int e = 0;

	if( instance->cb_func )
		e = (*instance->cb_func)(instance->cb_data, ev);

	return e;
}

static int _impl_open(rh_asmp_itf self, const char * const fn) {

  struct asmp_instance * instance = (struct asmp_instance *)self;

  instance->frame.buffersize = 0;
  instance->frame.buffer = NULL;
  instance->frame.step = 0x7f;

  {
	  	esprom_handle esprom = NULL;
	  	int    sample_index = 0;

	  	if((sscanf(fn,"prom_libesprom://%p/%d",&esprom, &sample_index) != 2))
			return -1;

		if(esprom) {
			if(esprom_sample_alloc( esprom, sample_index, &instance->frame.asset_libesprom ) != 0)
				return -1;

			// reset all state, and pre-load first packet.
			instance->interface->reset(self);

			return 0;
		}

		return -1;
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

static int _adpcm_read_packet(rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	int err;

	instance->frame.nbsamples = 0;
	instance->frame.processed_samples = 0;

	err = esprom_sample_getbuffer(
			 instance->frame.asset_libesprom,
			(void**)&instance->frame.buffer,
			&instance->frame.buffersize);

	if( (err < 0) || ( instance->frame.buffersize <= 0 ) ) {

		instance->ate = 1;
		return err;
	}

	esprom_sample_seek( instance->frame.asset_libesprom, instance->frame.buffersize, SEEK_CUR );

	#if(RESAMPLE_48_KHZ)
		instance->frame.nbsamples = instance->frame.buffersize * 6;
	#else
		instance->frame.nbsamples = instance->frame.buffersize * 2;
	#endif

	return instance->frame.buffersize;
}

static int _impl_reset(rh_asmp_itf self) {

	struct asmp_instance * instance = (struct asmp_instance *)self;

	if(instance->frame.is_reset)
		return 0;

	esprom_sample_seek( instance->frame.asset_libesprom ,0, SEEK_SET);

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
		 int32_t samplesRemaining = samples;
		int8_t * outBuffer = (int8_t *)(dst);

		frame_t * frame = &instance->frame;

#if(RESAMPLE_48_KHZ)
		uint8_t * srcBuffer = instance->frame.buffer +
				(instance->frame.processed_samples / 6);

		if( instance->frame.processed_samples % 6 )
			++srcBuffer;
#else
		uint8_t * srcBuffer = instance->frame.buffer +
				(instance->frame.processed_samples / 2);
#endif

		while(samplesRemaining >= 2) {

#if(RESAMPLE_48_KHZ)
			if(instance->frame.phase == 0)
#endif
			{
				uint8_t srcData = *srcBuffer;

				/* compute the new amplitude and update the current step */
				uint8_t Data = srcData >> 4;

				instance->frame.signal +=
						(instance->frame.step * diff_lookup[Data & 15]) / 8;

				/* clamp to the maximum */
				if (instance->frame.signal > 32767)
					instance->frame.signal = 32767;
				else if (instance->frame.signal < -32768)
					instance->frame.signal = -32768;

				/* adjust the step size and clamp */
				instance->frame.step =
						(instance->frame.step * index_scale[Data & 7]) >> 8;

				if (instance->frame.step > 0x6000)
					instance->frame.step = 0x6000;
				else if (instance->frame.step < 0x7f)
					instance->frame.step = 0x7f;

				/* output to the buffer */
				frame->s0.field.upper = (int8_t)((instance->frame.signal >> 8) & 0xff);
				frame->s0.field.lower = (int8_t)( instance->frame.signal       & 0xff);

				//added part
				Data = srcData & 0x0F;
				instance->frame.signal +=
						(instance->frame.step * diff_lookup[Data & 15]) / 8;

				/* clamp to the maximum */
				if (instance->frame.signal > 32767)
					instance->frame.signal = 32767;
				else if (instance->frame.signal < -32768)
					instance->frame.signal = -32768;

				/* adjust the step size and clamp */
				instance->frame.step =
						(instance->frame.step * index_scale[Data & 7]) >> 8;

				if (instance->frame.step > 0x6000)
					instance->frame.step = 0x6000;
				else if (instance->frame.step < 0x7f)
					instance->frame.step = 0x7f;

				/* output to the buffer  */
				frame->s1.field.upper = (int8_t)((instance->frame.signal >> 8) & 0xff);
				frame->s1.field.lower = (int8_t)( instance->frame.signal       & 0xff);

				/* VOLUME! */
				frame->s0.value /= VOLUME;
				frame->s1.value /= VOLUME;

				++srcBuffer;
			}
			// audio-data is big-endian.

			if(mixmode) {
#if(RESAMPLE_48_KHZ)
				switch(frame->phase) {
				case 0:
					BE_MIX_CPU_16((*(int16_t*)(outBuffer+0)), frame->s0.value);
					BE_MIX_CPU_16((*(int16_t*)(outBuffer+2)), frame->s0.value);
					break;
				case 1:
					BE_MIX_CPU_16((*(int16_t*)(outBuffer+0)), frame->s0.value);
					BE_MIX_CPU_16((*(int16_t*)(outBuffer+2)), frame->s1.value);
					break;
				default:
				case 2:
					BE_MIX_CPU_16((*(int16_t*)(outBuffer+0)), frame->s1.value);
					BE_MIX_CPU_16((*(int16_t*)(outBuffer+2)), frame->s1.value);
					break;
				}
#else
				BE_MIX_CPU_16((*(int16_t*)(outBuffer+0)), frame->s0.value);
				BE_MIX_CPU_16((*(int16_t*)(outBuffer+2)), frame->s1.value);
#endif
			}
			else {
#if(RESAMPLE_48_KHZ)
				switch(frame->phase) {
				case 0:
//					DEBUG_PRINTF("phase0: writing %08d,%08d to %p\n",frame->s0.value,frame->s0.value,outBuffer);
					(*(int16_t*)(outBuffer+0)) =
					(*(int16_t*)(outBuffer+2)) = CPU_TO_BE_16(frame->s0.value);
					break;
				case 1:
//					DEBUG_PRINTF("phase1: writing %08d,%08d to %p\n",frame->s0.value,frame->s1.value,outBuffer);
//					(*(int16_t*)(outBuffer+0)) = CPU_TO_BE_16(frame->s0.value);
//					(*(int16_t*)(outBuffer+2)) = CPU_TO_BE_16(frame->s1.value);
					COPY_TWO_CPU16_TO_BE16(outBuffer, &(frame->s0.value));
					break;
				default:
				case 2:
//					DEBUG_PRINTF("phase2: writing %08d,%08d to %p\n",frame->s1.value,frame->s1.value,outBuffer);
					(*(int16_t*)(outBuffer+0)) =
					(*(int16_t*)(outBuffer+2)) = CPU_TO_BE_16(frame->s1.value);
					break;
				}
#else
//				(*(int16_t*)(outBuffer+0)) = CPU_TO_BE_16(frame->s0.value);
//				(*(int16_t*)(outBuffer+2)) = CPU_TO_BE_16(frame->s1.value);
				COPY_TWO_CPU16_TO_BE16(outBuffer, &(frame->s0.value));
#endif
			}

#if(RESAMPLE_48_KHZ)
			instance->frame.phase++;
			if(instance->frame.phase >= 3)
				instance->frame.phase = 0;
#endif
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

		esprom_sample_free( instance->frame.asset_libesprom );
		free( instance->interface    );
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

