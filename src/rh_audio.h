
#pragma once

//#include<rh_raw_loader.h>

#ifdef __cplusplus
extern "C" {
#endif /** __cplusplus **/

struct rh_audiosample_type;

typedef struct rh_audiosample_type * rh_audiosample_handle;

typedef enum {

  RH_AUDIOSAMPLE_STOPPED = (1<<0),
  RH_AUDIOSAMPLE_STARTED = (1<<1),
  RH_AUDIOSAMPLE_ERROR   = (1<<2)

} rh_audioevent_cb_event_enum_t;

typedef enum {

  RH_AUDIOSAMPLE_DONTCOPYSRC = (1<<0),

} rh_audiosample_openflags_enum_t;

typedef int (*rh_audiosample_cb_type)(rh_audiosample_handle h, void * cb_data, rh_audioevent_cb_event_enum_t ev);

int rh_audiosample_setup();
int rh_audiosample_shutdown();

int rh_audiosample_open			( rh_audiosample_handle * h, const char * source, int flags );
int rh_audiosample_open_rawpak	( rh_audiosample_handle * h, void * ctx, int flags);
int rh_audiosample_close		( rh_audiosample_handle h );
int rh_audiosample_play 		( rh_audiosample_handle h );
int rh_audiosample_loop 		( rh_audiosample_handle h );
int rh_audiosample_stop 		( rh_audiosample_handle h );
int rh_audiosample_wait			( rh_audiosample_handle h );
int rh_audiosample_isplaying	( rh_audiosample_handle h );
int rh_audiosample_seek			( rh_audiosample_handle h, int offset, int whence );
int rh_audiosample_register_cb	( rh_audiosample_handle h, rh_audiosample_cb_type cb, void * cb_data );

int rh_audiosample_stopall();
int rh_audiosample_closeall();

#ifdef __cplusplus
} /* extern "C" { */
#endif /** __cplusplus **/

