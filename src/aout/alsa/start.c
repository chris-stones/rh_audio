
#include "alsa.h"

int aout_alsa_start( aout_handle h) {

  int e = -1;

  if( ! ( h->status & AOUT_STATUS_PLAYING ) ) {

    if(aout_alsa_io_add(h)==0) {
      e = aout_started( h );
	}
  }

  return e;
}

