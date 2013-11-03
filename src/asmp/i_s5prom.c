
/***
 *
 * Loading data from a scorpion5 sound prom.
 *
 */

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <byteswap.h>
#include <stdint.h>

#include "asmp_internal.h"

typedef FILE	AssetType;
typedef void	AssetManagerType;

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

struct priv_internal {

	int stat;
	AssetType * asset;
	int sample_index;
	sample_header_t sample_header;
	size_t readpos;

	struct frame frame;
};


static int is_little_endian() {
	int n = 1;
	return(*(char *)&n == 1);
}

static int is_big_endian() {
	int n = 1;
	return(*(char *)&n == 0);
}

static inline struct priv_internal * get_priv(aud_sample_handle p) {

  return (struct priv_internal *)p->priv;
}

static int _seek(aud_sample_handle handle, long offset, int whence) {

	struct priv_internal * priv = NULL;

	if(!handle)
		return -1;

	priv = get_priv(handle);

	if(!priv)
		return -1;

	switch(whence) {
		case SEEK_SET:
			priv->readpos = offset;
			break;
		case SEEK_CUR:
			priv->readpos += offset;
			break;
		case SEEK_END:
			priv->readpos = (priv->sample_header.end - priv->sample_header.start) + offset;
			break;
	}

	if(priv->readpos >= 0 && priv->readpos <= (priv->sample_header.end - priv->sample_header.start) )
		return 0;

	return -1;
}

static inline int _read(void* data, size_t size, size_t nmemb, aud_sample_handle p) {

	struct priv_internal * priv = get_priv(p);

	int err = 0;

	err = fseek(priv->asset, priv->readpos + priv->sample_header.start, SEEK_SET);

	if(!err)
		err = fread(data, size, nmemb, priv->asset );

	if( err < 0)
		return err;

	priv->readpos += size * err;

	if(priv->readpos > (priv->sample_header.end - priv->sample_header.start) ) {
		priv->readpos = (priv->sample_header.end - priv->sample_header.start);
		return -1; // read past end of embedded file.
	}

	return err;
}

static int _adpcm_read_packet(aud_sample_handle p) {

	int err = 0;
	int frameFinished = 0;

	struct priv_internal * priv = get_priv(p);

	priv->frame.nbsamples = 0;

	err = _read( priv->frame.buffer, 1, priv->frame.buffersize, p );

	if( err <= 0) {

		priv->stat = 1; // SET END OF STREAM
		return err;
	}

	priv->frame.nbsamples = err * 2; // ASSUMING 16bit mono,

	return err;
}

static int _aud_sample_resetter(aud_sample_handle p) {

	struct priv_internal * priv = get_priv(p);

	if(priv->frame.is_reset)
		return 0;

	printf("RESETTING DISK BUFFER\n");

	_seek(p, 0, SEEK_SET);
	get_priv(p)->stat = 0;

	// reset decoder state.
	priv->frame.signal = 0;
	priv->frame.step = 0x7f;

	// reset buffer state.
	priv->frame.processed_samples = 0;
	priv->frame.nbsamples = 0;

	// pre-load first frame.
	_adpcm_read_packet(p);

	// flag buffer as being in a reset state;
	priv->frame.is_reset = 1;

	return 0;
}

static int _aud_sample_opener(aud_sample_handle p, const char * const fn) {

  struct priv_internal *priv = calloc(1, sizeof(struct priv_internal));

  // ADPCM - MUST BE A MULTIPLE OF 4!
  priv->frame.buffersize = 1000; // 125ms
  priv->frame.step = 0x7f;

  if( (priv->frame.buffer = malloc(priv->frame.buffersize) ) == NULL ) {

	  free(priv);
	  priv = NULL;
  }

  if(priv) {

		if(strncmp("PROM://",fn,7)==0) { /* e.g. "FILE://file_ptr/sample_id */

			void * p = NULL;
			int    sample_index = 0;
			short  nsamples;

			if(sscanf(fn,"PROM://%p/%d",&p,&sample_index) != 2) {
				free(priv);
				return -1;
			}

			priv->asset = (FILE*)p;
			priv->sample_index = sample_index;

			fseek(priv->asset, 14, SEEK_SET);
			fread(&nsamples, 2, 1, priv->asset);
			if(is_little_endian()) {
				nsamples = bswap_16(nsamples);
			}

			if(sample_index >= nsamples) {
				free(priv->frame.buffer);
				free(priv);
				return -1;
			}

			fseek(priv->asset, 16 + 10 * sample_index, SEEK_SET);
			fread(&priv->sample_header, sizeof priv->sample_header, 1, priv->asset);
			if(is_little_endian())
			{
				priv->sample_header.freq  = bswap_16(priv->sample_header.freq);
				priv->sample_header.start = bswap_32(priv->sample_header.start);
				priv->sample_header.end   = bswap_32(priv->sample_header.end);
			}
		}

		p->priv = (void*)priv;

		// todo - what format is your raw data in?
		p->channels 	= 1;
		p->samplerate 	= 16000;
//		p->samplerate 	= priv->sample_header.freq;
		p->samplesize	= 2;

		// reset all state, and pre-load first packet.
		_aud_sample_resetter(p);

		return 0;
  }

  return -1;
}

static int diff_lookup[16] =
{
	1,3,5,7,9,11,13,15,-1,-3,-5,-7,-9,-11,-13,-15
};

static int index_scale[16] =
{
	0x0e6,0x0e6,0x0e6,0x0e6,0x133,0x199,0x200,0x266,
	0x0e6,0x0e6,0x0e6,0x0e6,0x133,0x199,0x200,0x266
};

static int _aud_sample_reader(aud_sample_handle p, int samples, void * dst, size_t dst_size) {

	int ret = 0;
	struct priv_internal * priv = get_priv(p);

	samples &= (~1); // ASSUMING 16bit mono ( 2 samles per byte )

	if(priv->frame.processed_samples >= priv->frame.nbsamples ) {

		priv->frame.processed_samples = 0;
		_adpcm_read_packet(p);
	}

	{
		// ASSUMING 16bit mono
		int samplesRemainingInFrame = (priv->frame.nbsamples - priv->frame.processed_samples);
		if(samples > samplesRemainingInFrame)
			samples = samplesRemainingInFrame;
	}

	// ADPCM DECODE
	{
		uint32_t Position = 0;
		uint32_t size = (samples/2); // ASSUMING 16bit mono
		int8_t * outBuffer = (int8_t *)(dst);
		uint8_t * srcBuffer = priv->frame.buffer + (priv->frame.processed_samples / 2);
		while(Position != size)
		{
			/* compute the new amplitude and update the current step */
			uint8_t Data = srcBuffer[Position] >> 4;
			priv->frame.signal += (priv->frame.step * diff_lookup[Data & 15]) / 8;

			/* clamp to the maximum */
			if (priv->frame.signal > 32767) priv->frame.signal = 32767; else if (priv->frame.signal < -32768) priv->frame.signal = -32768;

			/* adjust the step size and clamp */
			priv->frame.step = (priv->frame.step * index_scale[Data & 7]) >> 8;

			if (priv->frame.step > 0x6000) priv->frame.step = 0x6000; else if (priv->frame.step < 0x7f) priv->frame.step = 0x7f;

			/* output to the buffer, scaling by the volume */
			if(!is_big_endian()) {
				*outBuffer++ = (int8_t)(priv->frame.signal & 0xff);
				*outBuffer++ = (int8_t)((priv->frame.signal >> 8) & 0xff);
			} else {
				*outBuffer++ = (int8_t)((priv->frame.signal >> 8) & 0xff);
				*outBuffer++ = (int8_t)(priv->frame.signal & 0xff);
			}

			//added part
			Data = srcBuffer[Position] & 0x0F;
			priv->frame.signal += (priv->frame.step * diff_lookup[Data & 15]) / 8;

			/* clamp to the maximum */
			if (priv->frame.signal > 32767) priv->frame.signal = 32767; else if (priv->frame.signal < -32768) priv->frame.signal = -32768;

			/* adjust the step size and clamp */
			priv->frame.step = (priv->frame.step * index_scale[Data & 7]) >> 8;

			if (priv->frame.step > 0x6000) priv->frame.step = 0x6000; else if (priv->frame.step < 0x7f) priv->frame.step = 0x7f;

			/* output to the buffer, scaling by the volume */
			if(!is_big_endian()) {
				*outBuffer++ = (int8_t)(priv->frame.signal & 0xff);
				*outBuffer++ = (int8_t)((priv->frame.signal >> 8) & 0xff);
			} else {
				*outBuffer++ = (int8_t)((priv->frame.signal >> 8) & 0xff);
				*outBuffer++ = (int8_t)(priv->frame.signal & 0xff);
			}

			Position++;
		}
	}

	priv->frame.processed_samples  += samples;

	if(samples)
		priv->frame.is_reset = 0;

    return samples;
}

static int _aud_sample_stater(aud_sample_handle p) {

	return get_priv(p)->stat;
}

static int _aud_sample_closer(aud_sample_handle p) {

  struct priv_internal * priv = get_priv(p);

  if(p->priv)
	  free(priv->frame.buffer);
  free(p->priv);
  p->priv = NULL;

  return 0;
}

int aud_init_interface_s5prom(aud_sample_handle p) {

  p->opener = &_aud_sample_opener;
  p->reader = &_aud_sample_reader;
  p->reseter = &_aud_sample_resetter;
  p->closer = &_aud_sample_closer;
  p->stater = &_aud_sample_stater;

  return 0;
}

