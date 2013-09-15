
#include "aout_internal.h"

int aout_call_callback(aout_handle h, aout_cb_event_enum_t ev) {

  if( h->cb ) {
    ++(h->cb_depth);
    h->cb(h, h->samp_data, h->cb_data, ev);
    --(h->cb_depth);
  }
  return 0;
}


int aout_handle_events(aout_handle h) {

  int e = 0;

  if(h->events) {

    e = aout_call_callback(h, (aout_cb_event_enum_t)h->events);

    h->events = 0;
  }

  return e;
}

int aout_stopped(aout_handle h) {

  h->status &= ~AOUT_STATUS_PLAYING;
  h->events |= AOUT_STOPPED;

  return 0;
}

int aout_started(aout_handle h) {

    h->status |= AOUT_STATUS_PLAYING;
    h->events |= AOUT_STARTED;

  return 0;
}

int aout_error(aout_handle h) {

  h->status |= AOUT_STATUS_ERROR;
  h->events |= AOUT_ERROR;

  if(h->status & AOUT_STATUS_PLAYING)
    aout_stopped(h);

  return -1;
}

