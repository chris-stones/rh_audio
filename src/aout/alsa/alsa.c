

#include "alsa.h"

int aout_init_interface_ALSA(aout_handle p) {

    p->channel_start = &aout_alsa_start;
    p->channel_stop = &aout_alsa_stop;
//  p->channel_update = &aout_alsa_update;
    p->channel_open = &aout_alsa_open;
    p->channel_close = &aout_alsa_close;

    return aout_alsa_io_setup();
}



