
#include "alsa.h"

int aout_alsa_stop( aout_handle h) {

  if( h->status & AOUT_STATUS_PLAYING ) {

    snd_pcm_drop( get_priv(h)->handle );

    aout_alsa_io_rem(h);
    aout_stopped(h);
	aout_alsa_io_reset(h);

    aout_handle_events( h ); // todo - move this to the alsa-io-thread !
  }

  return 0;
}

