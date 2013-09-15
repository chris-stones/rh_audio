
#include "sles.h"

#include<alloca.h>
#include<pthread.h>
#include<signal.h>
//#include<error.h>
#include<errno.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdio.h>
#include<poll.h>

#include"../../bucket.h"

#define IO_CMD_ADD 				0
#define IO_CMD_REMOVE 			1
#define IO_CMD_RETURNBUFFER 	2
#define IO_CMD_EXIT				3

struct io_command_struct {

  int cmd;
  aout_handle h;
};

struct io_struct {

  volatile pthread_t thread;
  bucket_handle aout_handle_bucket;

  struct {
    int read;
    int write;
  } cmd_pipe;
} ;

static struct io_struct io = {

  0,
  0,
  {0,0},
};

static int pipe_send( struct io_command_struct *cmd ) {

  char * p = (char*)(cmd+0);
  char * e = (char*)(cmd+1);
  do {

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

  struct io_command_struct cmd = { IO_CMD_ADD, h };

  return pipe_send( &cmd );
}

int aout_OpenSLES_io_rem(aout_handle h) {

  struct io_command_struct cmd = { IO_CMD_REMOVE, h };

  return pipe_send( &cmd );
}

int aout_OpenSLES_io_return_buffer(aout_handle h) {

	struct io_command_struct cmd = { IO_CMD_RETURNBUFFER, h };

	return pipe_send( &cmd );
}

static int aout_OpenSLES_io_exit() {

	struct io_command_struct cmd = { IO_CMD_EXIT, NULL };

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

    switch(cmd.cmd)
	{
		case IO_CMD_REMOVE:
		{
			struct priv_internal * p = get_priv(cmd.h);

//			(*p->playItf)->SetPlayState( p->playItf, SL_PLAYSTATE_PAUSED );
			(*p->playItf)->SetPlayState( p->playItf, SL_PLAYSTATE_STOPPED );

			(*p->bufferQueueItf)->Clear(p->bufferQueueItf);
			buffer_queue_reset( &p->bq );

			e = bucket_remove(io.aout_handle_bucket, cmd.h);

			break;
		}
		case IO_CMD_ADD:
			e = bucket_add(io.aout_handle_bucket, cmd.h);
			break;
		case IO_CMD_RETURNBUFFER:
		{
			struct priv_internal * p = get_priv(cmd.h);
			buffer_queue_t * bq = &p->bq;
			buffer_queue_return_drain_buffer( bq );
			e = 0;
			break;
		}
		case IO_CMD_EXIT:
			io.thread = 0;
			pthread_exit(NULL);
			break;
	}

    if(e != 0)
      return -1;
  }

  return 0;
}

static int poll_cmd_pipe() {

	struct pollfd ufds[1];

	ufds[0].fd = io.cmd_pipe.read;
    ufds[0].revents = 0;
    ufds[0].events = POLLIN | POLLPRI;

	poll( ufds, sizeof ufds / sizeof ufds[0], -1 );

	return 0;
}

static void * io_main(void * p) {

  aout_handle * array;
  int len;

  for(;;) {

	poll_cmd_pipe();

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

int aout_OpenSLES_io_setup() {

  memset(&io, 0, sizeof io);

  if( pipe( &io.cmd_pipe.read ) != 0 )
    goto err0;

  if(fcntl( io.cmd_pipe.read, F_SETFL, O_NONBLOCK) != 0)
    goto err1;

  if(bucket_create(&io.aout_handle_bucket) != 0)
    goto err2;
  {
	  pthread_t thread;
	  if(pthread_create(&thread, NULL, &io_main, NULL) != 0)
		  goto err3;
	  io.thread = thread;
  }

  pthread_detach( io.thread );

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

int aout_OpenSLES_io_teardown() {

	if(io.thread) {
		aout_OpenSLES_io_exit();
		while(io.thread)
			sched_yield();
	}

	bucket_free(io.aout_handle_bucket);
	io.aout_handle_bucket = NULL;

	close(io.cmd_pipe.write);
	io.cmd_pipe.write = 0;

	close(io.cmd_pipe.read);
	io.cmd_pipe.read = 0;

	return 0;
}
