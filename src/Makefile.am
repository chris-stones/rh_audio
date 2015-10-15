lib_LTLIBRARIES = librh_audio.la

librh_audio_la_SOURCES = \
  bucket.c \
  bucket.h \
  rh_audio.c \
  rh_audio.h \
  rh_audio_internal.h \
  aout/aout.h \
  asmp/asmp.c \
  asmp/asmp.h

if RH_OUTPUT_TO_ALSA
  librh_audio_la_SOURCES += \
    aout/alsa/alsa_api.c \
    aout/alsa/alsa.c \
    aout/alsa/alsa.h \
    aout/alsa/alsa_private.h \
    aout/alsa/close.c \
    aout/alsa/open.c \
    aout/alsa/sample.c \
    aout/alsa/settings.c \
    aout/alsa/start.c \
    aout/alsa/stop.c \
    aout/alsa/update.c
endif

if RH_INPUT_FROM_FFMPEG
  librh_audio_la_SOURCES += \
    asmp/i_ffmpeg.c  
endif

include_HEADERS = \
  rh_audio.h \
  rh_audio.hpp

