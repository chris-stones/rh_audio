AC_PREREQ([2.69])
AC_INIT(RockHopper, 0.1, chris.stones@zoho.com)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(src/rh_audio.c)
AC_CONFIG_HEADERS(src/config.h)

AC_CONFIG_MACRO_DIR([m4])

LT_INIT
AM_INIT_AUTOMAKE([subdir-objects])

AX_PTHREAD(
  [
    LIBS="$PTHREAD_LIBS $LIBS"
    CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
    CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
  ],
  [
    AC_MSG_ERROR([No pthreads library.])
  ])

AC_ARG_WITH([alsa],   AS_HELP_STRING([--with-alsa],   [Use ALSA for audio out]))
AC_ARG_WITH([ffmpeg], AS_HELP_STRING([--with-ffmpeg], [Use FFMPEG for audio in]))

AS_IF([test "x$with_alsa" = "xyes"],
[
  AC_DEFINE([AOUT_ALSA],[1],[use ALSA as output API.])
  AC_SEARCH_LIBS([snd_pcm_open],[asound],[],[AC_MSG_ERROR([Unable to find alsa library.])])
  AC_CHECK_HEADERS([alsa/asoundlib.h],[],[AC_MSG_ERROR([Missing alsa/asoundlib.h])])
])

AS_IF([test "x$with_ffmpeg" = "xyes"],
[
  AC_DEFINE([RH_WITH_FFMPEG],[1],[use FFmpeg as input API.])
  AC_CHECK_HEADERS([libavcodec/avcodec.h],[],[AC_MSG_ERROR([Missing libavcodec/libavcodec.h])])
  AC_CHECK_HEADERS([libavformat/avformat.h],[],[AC_MSG_ERROR([Missing libavformat/avformat.h])])
])

AM_CONDITIONAL([RH_OUTPUT_TO_ALSA],    [test x$with_alsa   = xyes])
AM_CONDITIONAL([RH_INPUT_FROM_FFMPEG], [test x$with_ffmpeg = xyes])

AC_PROG_INSTALL

AC_CONFIG_FILES([
        Makefile
        src/Makefile
])
AC_OUTPUT

