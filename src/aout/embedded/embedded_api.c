
#include "embedded_private.h"

#include<alloca.h>
#include<pthread.h>
#include<signal.h>
#include<error.h>
#include<errno.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>

#include "../../bucket.h"

#define DEBUG_DUMPFILE "audiodump.s16_be"

//#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
  #define DEBUG_PRINTF(...) do{}while(0)

struct embedded_api_instance {

    // interface ptr must be the first item in the instance.
    struct rh_aout_api * interface;

    // private data
    volatile pthread_t  thread;
    bucket_handle       aout_itf_bucket;

    audio_device_t device;

    struct {
        int read;
        int write;
    } cmd_pipe;
};

typedef enum {

    EXIT_COMMAND,

    LOOP_COMMAND,
    PLAY_COMMAND,
    STOP_COMMAND,

    SYNC_COMMAND,

} command_enum_t;

struct io_command_struct {

    int command;
    rh_asmp_itf asmp_itf;
};

static int pipe_send( rh_aout_api_itf self, const struct io_command_struct *cmd ) {

    struct embedded_api_instance * api_instance = (struct embedded_api_instance *)self;

    char * p = (char*)(cmd+0);
    char * e = (char*)(cmd+1);
    do {

        /*
         * The write end of the pipe is in blocking mode, and io_command_struct is smaller than PIPE_BUF.
         * So write will always write the correct number of bytes,
         * But I'm paranoid!
         */
        int i = write( api_instance->cmd_pipe.write, p, ((size_t)e)-((size_t)p) );

        if(i>=0) {
            p+=i;
        } else {
            switch(errno) {
            case EINTR:
                break;
            default:
            	DEBUG_PRINTF("ERROR: pipe_send(%d) == %d\n", api_instance->cmd_pipe.write, i);
                return -1;
            }
        }
    } while(p < e);

    return 0;
}

// return negative on error, 0 on nothing read, 1 on struct read.
static int pipe_recv( rh_aout_api_itf self, struct io_command_struct *cmd ) {

    struct embedded_api_instance * api_instance = (struct embedded_api_instance *)self;

    char * p = (char*)(cmd+0);
    char * e = (char*)(cmd+1);
    int bytes = 0;
    do {

        int i = read( api_instance->cmd_pipe.read, p, ((size_t)e)-((size_t)p) );

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

static int add_channels(rh_aout_api_itf self, int channels) {

    int i ;
    rh_aout_itf * itf = alloca( channels * sizeof(rh_aout_itf) );
    struct embedded_api_instance * api_instance = (struct embedded_api_instance *)self;

    for(i=0; i<channels; i++) {
		DEBUG_PRINTF("about to create audio channel\n");
        if( rh_aout_create_embedded(&(itf[i])) != 0) {
            while(i >= 0) {
                (*(itf[i]))->close(&(itf[i]));
                --i;
            }
            return -1;
        }
        DEBUG_PRINTF("created rh_aout_itf @ %p\n", itf[i] );
	}

    for(i=0; i<channels; i++)
        if((bucket_add(api_instance->aout_itf_bucket, (void*)itf[i] )) != 0) {
            while(i >= 0) {
                bucket_remove(api_instance->aout_itf_bucket,(void*)itf[i] );
                (*(itf[i]))->close(&(itf[i]));
                --i;
            }
            return -1;
        }

    return 0;
}

static int close_all_channels(rh_aout_api_itf self) {

    struct embedded_api_instance * api_instance = (struct embedded_api_instance *)self;

    int len;
    rh_aout_itf * array;

    if( bucket_lock( api_instance->aout_itf_bucket, (void***)&array, &len ) == 0 ) {

        int i;
        for(i=0; i<len; i++)
			if(array[i]) {
				printf("array[%d] == %p\n", i, array[i]);
				(*(array[i]))->close(&array[i]);
			}

        bucket_unlock( api_instance->aout_itf_bucket );
    }

    return 0;
}

static int recv_sync_cmd(rh_aout_api_itf self, rh_asmp_itf asmp_itf) {

    (*asmp_itf)->on_output_event(asmp_itf, RH_ASMP_OUTPUT_EVENT_SYNC);

    return 0;
}

static int stop(rh_aout_api_itf self, rh_asmp_itf asmp_itf) {

    struct embedded_api_instance * api_instance = (struct embedded_api_instance *)self;

    int len;
    rh_aout_itf * array;
    int e = 0;

    if( bucket_lock( api_instance->aout_itf_bucket, (void***)&array, &len ) == 0 ) {

        int i;
        for(i=0; i<len; i++) {

            rh_asmp_itf audio_sample;
            rh_aout_itf audio_channel = array[i];

            if( (*audio_channel)->get_sample(audio_channel, &audio_sample) == 0 ) {

                if( audio_sample == asmp_itf ) {

                    e = (*audio_channel)->stop(audio_channel);
                    (*audio_channel)->set_sample(audio_channel, NULL);

                    break;
                }
            }
        }

        bucket_unlock( api_instance->aout_itf_bucket );
    }

    return e;
}

static int _play(rh_aout_api_itf self, rh_asmp_itf asmp_itf, int loop_flag) {

    struct embedded_api_instance * api_instance = (struct embedded_api_instance *)self;

    int len;
    rh_aout_itf * array;
    int e = 0;

    for(;;) {
        int found = 0;
        if( bucket_lock( api_instance->aout_itf_bucket, (void***)&array, &len ) == 0 ) {

            int i;

            // is this sample already assigned to a channel? try to find its channel.

            for(i=0; i<len; i++) {

                rh_asmp_itf audio_sample;
                rh_aout_itf audio_channel = array[i];

                if( (*audio_channel)->get_sample(audio_channel, &audio_sample) == 0 ) {

                    if( audio_sample == asmp_itf ) {

                        found = 1;

                        if(loop_flag)
                            e = (*audio_channel)->loop(audio_channel);
                        else
                            e = (*audio_channel)->play(audio_channel);

                        break;
                    }
                }
            }

            if(!found) {

                // sample not yet assigned to a channel, find and empty one and assign it.

                for(i=0; i<len; i++) {

                    rh_asmp_itf audio_sample;
                    rh_aout_itf audio_channel = array[i];

                    if( (*audio_channel)->get_sample(audio_channel, &audio_sample) == 0 ) {

                        if( audio_sample == NULL ) {

                        	int bigendian = 0;
                        	if((*asmp_itf)->is_bigendian)
                        		bigendian = (*asmp_itf)->is_bigendian(asmp_itf);

                            found = 1;

                            (*audio_channel)->set_sample(audio_channel, asmp_itf);

                            (*audio_channel)->open(
                                audio_channel,
                                (*asmp_itf)->channels(asmp_itf),
                                (*asmp_itf)->samplerate(asmp_itf),
                                (*asmp_itf)->samplesize(asmp_itf),
                                bigendian);

                            if(loop_flag)
                                e = (*audio_channel)->loop(audio_channel);
                            else
                                e = (*audio_channel)->play(audio_channel);

                            break;
                        }
                    }
                }
            }

            bucket_unlock( api_instance->aout_itf_bucket );
        }

        if(!found) {
            // no empty channels available, create a new channel and try again.
            if((e = add_channels(self, 1)) != 0)
                break;
        }
        else
            break;
    }

    return e;
}

static int play(rh_aout_api_itf self, rh_asmp_itf asmp_itf) {

    return _play(self, asmp_itf, 0);
}

static int loop(rh_aout_api_itf self, rh_asmp_itf asmp_itf) {

    return _play(self, asmp_itf, 1);
}

static int process_cmd_pipe(rh_aout_api_itf self) {

    struct embedded_api_instance * api_instance = (struct embedded_api_instance *)self;

    struct io_command_struct cmd;

    for(;;) {

        int e = pipe_recv(self, &cmd);

        if(e < 0) {
        	DEBUG_PRINTF("ERROR: pipe_recv() == %d\n", e);
            return -1;
        }

        if(e == 0) {
        	DEBUG_PRINTF("pipe_recv() == %d\n", e);
            return 0;
        }

        switch(cmd.command) {
        case EXIT_COMMAND:
        	DEBUG_PRINTF("process command-pipe: EXIT_COMMAND\n");
            api_instance->thread = 0;
            break;
        case STOP_COMMAND:
        	DEBUG_PRINTF("process command-pipe: STOP_COMMAND\n");
            e = stop(self, cmd.asmp_itf);
            break;
        case PLAY_COMMAND:
        	DEBUG_PRINTF("process command-pipe: PLAY_COMMAND\n");
            e = play(self, cmd.asmp_itf);
            break;
        case LOOP_COMMAND:
        	DEBUG_PRINTF("process command-pipe: LOOP_COMMAND\n");
            e = loop(self, cmd.asmp_itf);
            break;
        case SYNC_COMMAND:
        	DEBUG_PRINTF("process command-pipe: SYNC_COMMAND\n");
            e = recv_sync_cmd(self, cmd.asmp_itf);
            break;
        default:
        	DEBUG_PRINTF("ERROR unknown command! {%d,0x%p}\n", cmd.command, cmd.asmp_itf);
            break;
        }

        // free reference created for the command pipe.
        if( cmd.asmp_itf )
            (*cmd.asmp_itf)->close(&cmd.asmp_itf);

        //
        if(cmd.command == EXIT_COMMAND)
            pthread_exit(NULL);

        if(e != 0)
            return -1;
    }

    return 0;
}

static void transfer(audio_device_t* device, int head, rh_aout_itf aoutItf, int mixflag)
{
	struct aout_instance * instance = (struct aout_instance *)aoutItf;

	rh_asmp_itf  soundItf = instance ? instance->audio_sample : NULL;

	short* dst = (short*)((size_t)(device->dma_period_table[head].addr));
	u_int  dst_frames_remaining = device->dma_period_size / sizeof(short);

	if(!soundItf) {
		// No audio-data?
		// Play silence if we are not mixing.
		if(!mixflag) {
			DEBUG_PRINTF("silencing period\n");
			memset(dst, 0, dst_frames_remaining * 2);
		}
		return;
	}

	while(dst_frames_remaining) {

		if( (*soundItf)->atend(soundItf) ) {

			if( instance->status_flags & RH_AOUT_STATUS_LOOPING ) {

				(*soundItf)->reset(soundItf);
				continue;
			}

			aout_embedded_stop( aoutItf );

			if(!mixflag)
				memset(dst, 0, dst_frames_remaining * 2);

			dst_frames_remaining = 0;
		}
		else {

			int err = -1;

			if (!mixflag) {
				err = (*soundItf)->read( soundItf, dst_frames_remaining, dst );
				DEBUG_PRINTF("read %d bytes to %p\n", err * 2, dst);
			}
			else if((*soundItf)->mix) {
				err = (*soundItf)->mix( soundItf, dst_frames_remaining, dst );
				DEBUG_PRINTF("mixed %d bytes to %p\n", err * 2, dst);
			}

			if( err >= 0) {

				dst_frames_remaining = (err > dst_frames_remaining) ?
						0 : (dst_frames_remaining - err);

				dst += err;

			} else {

				// something went wrong - recover as gracefully as possible !

				if(!mixflag)
					memset(dst, 0, dst_frames_remaining * 2);
				dst_frames_remaining = 0;
				aout_embedded_stop( aoutItf );
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////

static int _is_channel_playing(rh_aout_itf channel) {

	struct aout_instance * aout_instance = (struct aout_instance *)channel;

	if(aout_instance && aout_instance->audio_sample) {

		if(aout_instance->status_flags & (RH_AOUT_STATUS_PLAYING | RH_AOUT_STATUS_LOOPING) )
			return 1;
	}

	return 0;
}

static void * api_main_loop(void * itf) {

    struct embedded_api_instance * instance = (struct embedded_api_instance *)itf;

    rh_aout_itf * array;

    int len;

    for(;;) {

        audio_device_t* device = &(instance->device);
        size_t tail = 0;
        size_t head = 0;
		{
        	/***
			 * NOTE: returning address in IOCTL return ( not possible on 64bit arch )
			 */
#if !defined(DEBUG_DUMPFILE)
			size_t ret = ioctl(device->fd,AUDIO_IOCTL_PERIOD);
#else
			// I guess the above IOCTL returns the current play address.
			size_t ret = device->driver.dma_addr;
#endif
			if (ret<0) exception("AUDIO_IOCTL_DONE %s\n",strerror(errno));
			size_t current = (((u_int)ret)-device->driver.dma_addr)/device->dma_period_size;

			process_cmd_pipe(itf); // TODO: before or after above IOCTL????

			do {
				if (++tail==device->driver.periods) tail = 0;

				u_int sounds_playing = 0;

				if( bucket_lock( instance->aout_itf_bucket, (void***)&array, &len ) == 0 ) {

					int i;

					for(i=0; i<len; i++) {

						rh_aout_itf channel = array[i];

						if(!_is_channel_playing(channel))
							continue;

						// transfer audio-data to driver!
						transfer(device,head,channel, sounds_playing);

						sounds_playing++;
					}

					bucket_unlock( instance->aout_itf_bucket );
				}

				if (!sounds_playing)
					transfer(device,head,NULL, 0); // silence!
				else
					device->dma_period_table[head].sounds_playing = sounds_playing;

#if defined(DEBUG_DUMPFILE)
				write(device->fd, (const void *)device->dma_period_table[head].addr, device->dma_period_size);
				DEBUG_PRINTF("dumping samples @ %p (len %zu)\n",(const void *)device->dma_period_table[head].addr, device->dma_period_size);
				usleep((1000000 / 16000) * (device->dma_period_size / 2));
#endif

				if (++head==device->driver.periods)
					head = 0;

				DEBUG_PRINTF("current %zu head %zu tail %zu\n", current, head, tail);

			} while (tail!=current);
		}
    }

    return NULL;
}


static int _impl_setup(rh_aout_api_itf self) {

    struct embedded_api_instance * instance = (struct embedded_api_instance *)self;

    instance->device.fd = -1;

    if( pipe( &instance->cmd_pipe.read ) != 0 )
        goto bad;

    if(fcntl( instance->cmd_pipe.read, F_SETFL, O_NONBLOCK) != 0)
        goto bad;

    if(bucket_create(&instance->aout_itf_bucket) != 0)
        goto bad;

    if(add_channels(self, 3) != 0)
        goto bad;

#if defined(DEBUG_DUMPFILE)
    if( (instance->device.fd = open(DEBUG_DUMPFILE, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR | S_IWUSR) ) == -1 ) {
    	DEBUG_PRINTF("open(\"%s\", O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR | S_IWUSR) == -1\n", DEBUG_DUMPFILE);
    	goto bad;
    }
#else
    // TODO: device node?
    if( (instance->device.fd = open("/dev/whatever", O_RDWR) ) == -1 )
    	goto bad;
#endif

    /////////////////////// LIFTED FROM audiolib.c ///////////////////////////
    {
    	audio_device_t * device = &( instance->device );

    	device->driver.format = AUDIO_FORMAT_S16_BE;
    	device->driver.rateHz = 16000;		// CS - was 48k, driver supports 16khz?
    	device->driver.channels = 1;
    	device->driver.frame_bits = 16;		// CS - surely this should be 16 !?
    	device->driver.period_size = 512;	// JMTDEBUG 2048;
    	device->driver.periods = 2;			// CS - was 4.

    	u_int n;
    	device->dma_period_size = device->driver.period_size * device->driver.frame_bits / 8;
    	size_t dma_size = device->driver.periods * device->dma_period_size;
    	device->driver.dma_addr = (size_t)malloc(dma_size);
    	if (!device->driver.dma_addr) exception("not enough memory");
    	memset((void*)device->driver.dma_addr,0,dma_size);

    	device->dma_period_table = (audio_dma_period_t*)malloc(device->driver.periods*sizeof(audio_dma_period_t));
    	if (device->dma_period_table == NULL) exception("not enough memory");
    	audio_dma_period_t *last_period = device->dma_period_table;
    	audio_dma_period_t *period = &device->dma_period_table[1];
    	last_period->addr = device->driver.dma_addr;
    	last_period->sounds_playing = 0;
    	for (n=1; n<device->driver.periods; n++) {
    		period->addr = last_period->addr+device->dma_period_size;
    		period->sounds_playing = 0;
    		last_period = period++;
    	}

#if !defined(DEBUG_DUMPFILE)
    	int ret = ioctl(device->fd,AUDIO_IOCTL_PREPARE,&device->driver);
    	if (ret) exception("AUDIO_IOCTL_PREPARE %s\n",strerror(errno));
    	ret = ioctl(device->fd,AUDIO_IOCTL_START);
    	if (ret) exception("AUDIO_IOCTL_START %s\n",strerror(errno));
#endif

    }
    //////////////////////////////////////////////////////////////////////////

    {
        pthread_t thread;
        if(pthread_create(&thread, NULL, &api_main_loop, (void*)self) != 0)
            goto bad;
        instance->thread = thread;
        pthread_detach( instance->thread );
    }

//good:
    return 0;

bad:
    if(instance->aout_itf_bucket) {
        close_all_channels(self);
        bucket_free(instance->aout_itf_bucket);
        instance->aout_itf_bucket = NULL;
    }
    if(instance->cmd_pipe.write)
        close(instance->cmd_pipe.write);
    if(instance->cmd_pipe.read)
        close(instance->cmd_pipe.read);
    if(instance->device.fd != -1)
    	close(instance->device.fd);
    return -1;
}

static int _impl_shutdown(rh_aout_api_itf * itf) {

    struct embedded_api_instance * instance;

    if(!itf)
        return -1;

    instance = (struct embedded_api_instance *)*itf;

    if(instance) {

        if(instance->thread) {
            static const struct io_command_struct cmd = { EXIT_COMMAND, NULL };
            if( pipe_send( *itf, &cmd ) == 0)
                while(instance->thread)
                    sched_yield();
        }

        if(instance->aout_itf_bucket) {
        	close_all_channels(*itf);
        	bucket_free(instance->aout_itf_bucket);
        	instance->aout_itf_bucket = NULL;
        }

        if(instance->cmd_pipe.write) {
        	close(instance->cmd_pipe.write);
        	instance->cmd_pipe.write = 0;
        }

        if(instance->cmd_pipe.read) {
        	close(instance->cmd_pipe.read);
        	instance->cmd_pipe.read = 0;
        }

#if !defined(DEBUG_DUMPFILE)
        // CS ADDED - audiolib.c never called stop??
        ioctl(instance->device.fd, AUDIO_IOCTL_STOP);
#endif

        if(instance->device.fd) {
        	close(instance->device.fd);
        	instance->device.fd = -1;
        }

        free(instance->device.dma_period_table);
        instance->device.dma_period_table = NULL;

        free((void*)(size_t)instance->device.driver.dma_addr);
        instance->device.driver.dma_addr = 0;

        if(instance->interface) {
        	free(instance->interface);
        	instance->interface = NULL;
        }

        free(instance);
    }

    *itf = NULL;

    return 0;
}

static int _impl_play(rh_aout_api_itf self, rh_asmp_itf sample) {

    int e = -1;

    const struct io_command_struct cmd = { PLAY_COMMAND, sample };

    DEBUG_PRINTF("pipe_send {PLAY_COMMAND, 0x%p}\n", sample);

    if( sample ) {
        // create a reference for the command pipe
        (*sample)->addref(sample);
        if((e = pipe_send(self, &cmd))!=0)
            (*sample)->close(&sample);
    }

    return e;
}

static int _impl_loop(rh_aout_api_itf self, rh_asmp_itf sample) {

    int e = -1;

    const struct io_command_struct cmd = { LOOP_COMMAND, sample };

    if( sample ) {
        // create a reference for the command pipe
        (*sample)->addref(sample);
        if((e = pipe_send(self, &cmd))!=0)
            (*sample)->close(&sample);
    }

    return e;
}

static int _impl_stop(rh_aout_api_itf self, rh_asmp_itf sample) {

    int e = -1;

    const struct io_command_struct cmd = { STOP_COMMAND, sample };

    if( sample ) {
        // create a reference for the command pipe
        (*sample)->addref(sample);
        if((e = pipe_send(self, &cmd))!=0)
            (*sample)->close(&sample);
    }

    return e;
}

static int _impl_sync(rh_aout_api_itf self, rh_asmp_itf sample) {

    int e = -1;

    const struct io_command_struct cmd = { SYNC_COMMAND, sample };

    if( sample ) {
        // create a reference for the command pipe
        (*sample)->addref(sample);
        if((e = pipe_send(self, &cmd))!=0)
            (*sample)->close(&sample);
    }

    return e;
}

int rh_aout_create_api( rh_aout_api_itf * itf ) {

    struct embedded_api_instance * instance  = calloc(1, sizeof( struct embedded_api_instance ) );
    struct rh_aout_api           * interface = calloc(1, sizeof( struct rh_aout_api           ) );

    if(!instance || !interface)
        goto bad;

    instance->interface = interface;

    interface->setup    = &_impl_setup;
    interface->shutdown = &_impl_shutdown;
    interface->play     = &_impl_play;
    interface->loop     = &_impl_loop;
    interface->stop     = &_impl_stop;
    interface->sync     = &_impl_sync;

//good:
    *itf = (rh_aout_api_itf)instance;
    return 0;

bad:
    free(instance);
    free(interface);
    return -1;
}

