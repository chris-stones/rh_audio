
#pragma once

#include<stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif /** __cplusplus **/

struct rh_audio;

#define RH_AUDIO_URL_PROM_RAWPAK(rawpak_ptr, sample_int)\
	"prom_rawpak://%p/%d",path_str,sample_int

#define RH_AUDIO_URL_FROM_FILEPTR(FILE_ptr, sample_int)\
	"prom_fileptr://%p/%d",FILE_ptr,sample_int

#define RH_AUDIO_URL_FFMPEG_RAWPAK(rh_rawpak_ctx_ptr)\
	"ffmpeg_rawpak://%p",rh_rawpak_ctx_ptr

typedef const struct rh_audio * const * rh_audio_itf; /* RockHopper audio interface */

struct rh_audio {

	int         (*open)        (rh_audio_itf  self, const char * source, int flags);
	int         (*openf)       (rh_audio_itf  self, int flags, const char * format, ...);
	int         (*vopenf)      (rh_audio_itf  self, int flags, const char * format, va_list ap);
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

#ifdef __cplusplus
} /* extern "C" { */
#endif /** __cplusplus **/

