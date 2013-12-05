
#pragma once

#ifdef __cplusplus
extern "C" {
#endif /** __cplusplus **/

struct rh_audio;

typedef const struct rh_audio * const * rh_audio_itf; /* RockHopper audio interface */


#define RH_AUDIO_URL_PROM_RAWPAK(rawpak_ptr, sample_int)\
	"prom_rawpak://%p/%d",path_str,sample_int

//#define RH_AUDIO_URL_PROM_FILESYSTEM(path_string,sample_int)\
//	"prom_filesystem://%s/%d",path_string,sample_int

//#define RH_AUDIO_URL_PROM_EMBEDDED(path_string,sample_int)\
//	"prom_embedded://%s/%d",path_string,sample_int

#define RH_AUDIO_URL_FFMPEG_RAWPAK(rh_rawpak_ctx_ptr)\
	"ffmpeg_rawpak://%p",rh_rawpak_ctx_ptr

//#define RH_AUDIO_URL_FFMPEG_FILESYSTEM(path_string)\
//	"ffmpeg_filesystem://%s",path_string

//#define RH_AUDIO_URL_FFMPEG_EMBEDDED(path_string)\
//	"ffmpeg_embedded://%s",path_string

struct rh_audio {

	int         (*open)        (rh_audio_itf  self, const char * source, int flags);
	int         (*openf)       (rh_audio_itf  self, int flags, const char * format, ...);
	int         (*close)       (rh_audio_itf *self);
	int         (*play)        (rh_audio_itf  self);
	int         (*loop)        (rh_audio_itf  self);
	int         (*stop)        (rh_audio_itf  self);
	int         (*wait)        (rh_audio_itf  self);
	int         (*is_playing)  (rh_audio_itf  self);
};

int rh_audio_setup_api();
int rh_audio_shutdown_api();
int rh_audio_create( rh_audio_itf * itf );

/*
static inline int rh_audio_open        (rh_audio_itf  self, const char * source, int flags) { return (*self)->open(self, source, flags); }
static inline int rh_audio_close       (rh_audio_itf *self) { return (**self)->close(self); }
static inline int rh_audio_play        (rh_audio_itf  self) { return ( *self)->play(self); }
static inline int rh_audio_loop        (rh_audio_itf  self) { return ( *self)->loop(self); }
static inline int rh_audio_stop        (rh_audio_itf  self) { return ( *self)->stop(self); }
static inline int rh_audio_wait        (rh_audio_itf  self) { return ( *self)->wait(self); }
static inline int rh_audio_is_playing  (rh_audio_itf  self) { return ( *self)->is_playing(self); }
*/

#ifdef __cplusplus
} /* extern "C" { */
#endif /** __cplusplus **/

