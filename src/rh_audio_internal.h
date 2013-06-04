
#pragma once

#include "rh_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

int rh_audiosample_add_to_internal_bucket		( rh_audiosample_handle sample );
int rh_audiosample_remove_from_internal_bucket	( rh_audiosample_handle sample );

#ifdef __cplusplus
} // extern "C" {
#endif

