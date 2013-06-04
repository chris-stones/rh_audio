
#pragma once

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif /** __cplusplus **/

struct aud_sample_type;

typedef struct aud_sample_type * aud_sample_handle;

#define asmp_handle aud_sample_handle // TODO

int asmp_open(aud_sample_handle * h, const char * const fn);
aud_sample_handle asmp_addref(aud_sample_handle p);
int asmp_close(aud_sample_handle p);
int asmp_seek(aud_sample_handle p, int frames, int whence);
int asmp_tell(aud_sample_handle p);
int asmp_stat(aud_sample_handle p, int * frames);
int asmp_read(aud_sample_handle p, int frames, void * dst, size_t dst_size);
int asmp_async_read(aud_sample_handle p, int frames, void * dst, size_t dst_size);
int asmp_get_channels(aud_sample_handle p);
size_t asmp_size(aud_sample_handle p);
int asmp_get_samplerate(aud_sample_handle p);
int asmp_get_seekable(aud_sample_handle p);

#ifdef __cplusplus
} // extern "C" {
#endif /** __cplusplus **/

