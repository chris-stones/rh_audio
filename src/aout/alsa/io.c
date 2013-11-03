
#include "alsa.h"

#include<alloca.h>
#include<pthread.h>
#include<signal.h>
#include<error.h>
#include<errno.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdio.h>

#include"../../bucket.h"

typedef enum {

	ADD_COMMAND,
	REMOVE_COMMAND,
	RESET_COMMAND,

} command_enum_t;

struct io_command_struct {

  int command;
  aout_handle h;
};

struct io_struct {

  pthread_mutex_t monitor;
  pthread_t thread;
  bucket_handle aout_handle_bucket;

  struct {
    int read;
    int write;
  } cmd_pipe;

  int is_initialised;
} ;

static struct io_struct io = {

  PTHREAD_MUTEX_INITIALIZER,
  0,
  0,
  {0,0},
  0
};

static int pipe_send( struct io_command_struct *cmd ) {

  char * p = (char*)(cmd+0);
  char * e = (char*)(cmd+1);
  do {

    /*
     * The write end of the pipe is in blocking mode, and io_command_struct is smaller than PIPE_BUF.
     * So write will always write the correct number of bytes,
     * But Im paranoid!
     */
    int i = write( io.cmd_pipe.write, p, ((size_t)e)-((size_t)p) );

    if(i>=0) {
      p+=i;
    } else {
      switch(errno) {
	case EINTR:
	  break;
	default:
	  return -1;
      }
    }
  } while(p < e);

  return 0;
}

// return negative on error, 0 on nothing read, 1 on struct read.
static int pipe_recv( struct io_command_struct *cmd ) {

  char * p = (char*)(cmd+0);
  char * e = (char*)(cmd+1);
  int bytes = 0;
  do {

    int i = read( io.cmd_pipe.read, p, ((size_t)e)-((size_t)p) );

    if(i==0 && !bytes) {
      return 0;
    } else if(i>0) {
      bytes += i;
      p+=i;
    } else {
      switch(errno) {
	//case EAGAIN:
	case EWOULDBLOCK:
	  if(bytes==0)
	    return 0;
	  break;
	case EINTR:
	  break;
	default:
	  return -1;
      }
    }
  } while(p < e);

  return 1;
}

int aout_alsa_io_add(aout_handle h) {

  struct io_command_struct cmd = { ADD_COMMAND, h };

  return pipe_send( &cmd );
}

int aout_alsa_io_rem(aout_handle h) {

  struct io_command_struct cmd = { REMOVE_COMMAND, h };

  return pipe_send( &cmd );
}

int aout_alsa_io_reset(aout_handle h) {

  struct io_command_struct cmd = { RESET_COMMAND, h };

  return pipe_send( &cmd );
}

static int process_cmd_pipe() {

  struct io_command_struct cmd;

  for(;;) {

    int e = pipe_recv(&cmd);

    if(e < 0)
      return -1;

    if(e == 0)
      return 0;

    switch(cmd.command) {
		case REMOVE_COMMAND:
			e = bucket_remove(io.aout_handle_bucket, cmd.h);
			break;
		case ADD_COMMAND:
			e = bucket_add(io.aout_handle_bucket, cmd.h);
			break;
		case RESET_COMMAND:
			e = cmd.h->samp_resetter( cmd.h->samp_data );
			break;
		default:
			break;
	}

    if(e != 0)
      return -1;
  }

  return 0;
}

static int aout_alsa_io_poll() {

  aout_handle * array;
  int len;
  int err=0;
  int sleep = 0;

  if( bucket_lock( io.aout_handle_bucket, (void***)&array, &len ) == 0 ) {

    int i;
    int fd_count = 1; // reserved 1 for io.cmd_pipe

    struct pollfd *ufds = NULL;
    struct pollfd *ufds_next = NULL;

    // first pass - count file descriptors
    for(i=0; i<len; i++) {

      struct priv_internal * priv = get_priv( array[i] );

      if( priv->sleep )
	continue; // sample is draining, we are not interested in its writability.

      int dc = snd_pcm_poll_descriptors_count( priv->handle );

      if(dc >= 0)
	fd_count += dc;
      else
	++err;
    }

    // second pass - collect file descriptors.
    if(fd_count && ( ufds = ufds_next = alloca(sizeof(struct pollfd) * fd_count))) {

      ufds_next[0].fd = io.cmd_pipe.read;
      ufds_next[0].revents = 0;
      ufds_next[0].events = POLLIN | POLLPRI;
      ++ufds_next;

      for(i=0; i<len; i++) {

	struct priv_internal * priv = get_priv( array[i] );

	// find shortest sleep time of currently draining sample.
	if( priv->sleep && ( (!sleep) || ( priv->sleep < sleep ) ) )
	  sleep = priv->sleep;

	if(priv->sleep)
	  continue; // sample is draining, we have no interest in its writability.

	int n = snd_pcm_poll_descriptors_count( priv->handle );

	if(n>0) {
	  snd_pcm_poll_descriptors( priv->handle, ufds_next, n );
	  ufds_next += n;
	}
      }
    }
    bucket_unlock( io.aout_handle_bucket );

    if(ufds) {
      int ms = sleep/1000;
      if(!ms && sleep)
	ms = 1;

      poll( ufds, fd_count, ms ? ms : -1 );

//      static int i = 0;
//      printf("alsa_io_poll %d (ms %d)\n", i++, ms);
      return 0;
    }
  }

  return -1;
}

static void * io_main(void * p) {

  aout_handle * array;
  int len;

  for(;;) {

    aout_alsa_io_poll();

    process_cmd_pipe();

    if( bucket_lock( io.aout_handle_bucket, (void***)&array, &len ) == 0 ) {

      int i;
      for(i=0;i<len;i++)
		aout_alsa_update( array[i] );

      bucket_unlock( io.aout_handle_bucket );
    }
  }

  return NULL;
}

static int _aout_alsa_io_setup() {

  if( pipe( &io.cmd_pipe.read ) != 0 )
    goto err0;

  if(fcntl( io.cmd_pipe.read, F_SETFL, O_NONBLOCK) != 0)
    goto err1;

  if(bucket_create(&io.aout_handle_bucket) != 0)
    goto err2;

  if(pthread_create(&io.thread, NULL, &io_main, NULL) != 0)
    goto err3;

  pthread_detach( io.thread );

  io.is_initialised = 1;

  return 0;

err3:
  bucket_free(io.aout_handle_bucket);
err2:
err1:
  close(io.cmd_pipe.write);
  close(io.cmd_pipe.read);
err0:
  return -1;
}

int aout_alsa_io_setup() {

  int e = -1;

  if( pthread_mutex_lock( &io.monitor ) == 0 ) {

    e = 0;
    if(!io.is_initialised)
      e = _aout_alsa_io_setup();

    pthread_mutex_unlock( &io.monitor );
  }

  return e;
}

int aout_alsa_io_teardown() {

  pthread_cancel(io.thread);
//pthread_cond_signal(&io.cond);
//pthread_kill(io.thread, SIGIO);
//pthread_join(io.thread, NULL);
  io.thread = 0;

  return 0;
}

