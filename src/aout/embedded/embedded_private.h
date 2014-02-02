

#pragma once

#include <sys/ioctl.h>

#include "embedded.h"

int rh_aout_create_embedded( rh_aout_itf * itf );

int aout_embedded_stop( rh_aout_itf self );
int aout_embedded_open(rh_aout_itf self, uint32_t channels, uint32_t samplerate, uint32_t samplesize, uint32_t bigendian);
int aout_embedded_close_api_nolock(rh_aout_itf * self);
int aout_embedded_close(rh_aout_itf * self);
int aout_embedded_update(rh_aout_itf self);

int aout_embedded_set_sample(rh_aout_itf self, rh_asmp_itf sample);
int aout_embedded_get_sample(rh_aout_itf self, rh_asmp_itf * sample);
int aout_embedded_read_sample(rh_aout_itf self, int frames, void * buffer);
int aout_embedded_atend_sample(rh_aout_itf self);
int aout_embedded_reset_sample(rh_aout_itf self);

int aout_embedded_play( rh_aout_itf self );
int aout_embedded_loop( rh_aout_itf self );

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define exception(...) do {\
	fprintf(stderr, "%s:%d: ", __FUNCTION__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	putc('\n', stderr); \
	exit(EXIT_FAILURE); \
} while (0)
#else
#define exception(args...) do {\
	fprintf(stderr, "%s:%d: ", __FUNCTION__, __LINE__); \
	fprintf(stderr, ##args); \
	putc('\n', stderr); \
	exit(EXIT_FAILURE); \
} while (0)
#endif

typedef enum { // based on ALSA snd_pcm_format_t

	AUDIO_FORMAT_S16_BE	= 3,

} audio_format_t;


typedef unsigned long snd_pcm_uframes_t;

struct _audio_device;

typedef struct {
	audio_format_t format;
	u_int rateHz;
	u_int channels;
	u_int frame_bits;
	snd_pcm_uframes_t period_size;
	u_int periods;
	size_t dma_addr; // ( u_int in embedded kernel )
} audio_driver_t;

typedef struct {
	size_t addr;
	u_int sounds_playing;
} audio_dma_period_t;

typedef struct _audio_device {
	int fd;
	short volume;
	audio_driver_t driver;
	ssize_t dma_period_size;
	audio_dma_period_t* dma_period_table;

} audio_device_t;

typedef enum {

	RH_AOUT_STATUS_STOPPED = 1<<0,
	RH_AOUT_STATUS_PLAYING = 1<<1,
	RH_AOUT_STATUS_LOOPING = 1<<2,

} rh_aout_alsa_status_flags;

struct aout_instance {

  // interface ptr must be the first item in the instance.
  struct rh_aout * interface;

  // private data //

  // audio-sample ( may be NULL if channel is free )
  rh_asmp_itf audio_sample;

  int status_flags;
};

#define AUDIO_IOCTL_PREPARE	_IOW('T', 1, audio_driver_t)
#define AUDIO_IOCTL_START	_IO('T', 2)
#define AUDIO_IOCTL_PERIOD	_IO('T', 3)
#define AUDIO_IOCTL_STOP	_IO('T', 4)

