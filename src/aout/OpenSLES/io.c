
#include "sles.h"

#include<alloca.h>
#include<pthread.h>
#include<signal.h>
#include<error.h>
#include<errno.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdio.h>

#include"../../bucket.h"


struct io_command_struct {

  int rem;
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

int aout_OpenSLES_io_add(aout_handle h) {

  struct io_command_struct cmd = { 0, h };

  return pipe_send( &cmd );
}

int aout_OpenSLES_io_rem(aout_handle h) {

  struct io_command_struct cmd = { 1, h };

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

    if(cmd.rem)
      e = bucket_remove(io.aout_handle_bucket, cmd.h);
    else
      e = bucket_add(io.aout_handle_bucket, cmd.h);

    if(e != 0)
      return -1;
  }

  return 0;
}

static void * io_main(void * p) {

  aout_handle * array;
  int len;

  for(;;) {

    process_cmd_pipe();

    if( bucket_lock( io.aout_handle_bucket, (void***)&array, &len ) == 0 ) {

      int i;
      for(i=0;i<len;i++)
		aout_OpenSLES_update( array[i] );

      bucket_unlock( io.aout_handle_bucket );
    }
  }

  return NULL;
}

static int _aout_OpenSLES_io_setup() {

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

int aout_OpenSLES_io_setup() {

  int e = -1;

  if( pthread_mutex_lock( &io.monitor ) == 0 ) {

    e = 0;
    if(!io.is_initialised)
      e = _aout_OpenSLES_io_setup();

    pthread_mutex_unlock( &io.monitor );
  }

  return e;
}

int aout_OpenSLES_io_teardown() {

  pthread_cancel(io.thread);
//pthread_cond_signal(&io.cond);
//pthread_kill(io.thread, SIGIO);
//pthread_join(io.thread, NULL);
  io.thread = 0;

  return 0;
}


