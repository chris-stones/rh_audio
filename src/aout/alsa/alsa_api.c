
#include "alsa_private.h"

#include<alloca.h>
#include<pthread.h>
#include<signal.h>
#include<error.h>
#include<errno.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdio.h>

#include"../../bucket.h"


struct alsa_api_instance {

    // interface ptr must be the first item in the instance.
    struct rh_aout_api * interface;

    // private data
    volatile pthread_t  thread;
    bucket_handle       aout_itf_bucket;

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

    struct alsa_api_instance * api_instance = (struct alsa_api_instance *)self;

    char * p = (char*)(cmd+0);
    char * e = (char*)(cmd+1);
    do {

        /*
         * The write end of the pipe is in blocking mode, and io_command_struct is smaller than PIPE_BUF.
         * So write will always write the correct number of bytes,
         * But Im paranoid!
         */
        int i = write( api_instance->cmd_pipe.write, p, ((size_t)e)-((size_t)p) );

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
static int pipe_recv( rh_aout_api_itf self, struct io_command_struct *cmd ) {

    struct alsa_api_instance * api_instance = (struct alsa_api_instance *)self;

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
    struct alsa_api_instance * api_instance = (struct alsa_api_instance *)self;

    for(i=0; i<channels; i++) {
		printf("about to create audio channel\n");
        if( rh_aout_create_alsa(&(itf[i])) != 0) {
            while(i >= 0) {
                (*(itf[i]))->close(&(itf[i]));
                --i;
            }
            return -1;
        }
        printf("created rh_aout_itf @ %p\n", itf[i] );
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

    struct alsa_api_instance * api_instance = (struct alsa_api_instance *)self;

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

    struct alsa_api_instance * api_instance = (struct alsa_api_instance *)self;

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

    struct alsa_api_instance * api_instance = (struct alsa_api_instance *)self;

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

    struct alsa_api_instance * api_instance = (struct alsa_api_instance *)self;

    struct io_command_struct cmd;

    for(;;) {

        int e = pipe_recv(self, &cmd);

        if(e < 0)
            return -1;

        if(e == 0)
            return 0;

        switch(cmd.command) {
        case EXIT_COMMAND:
            api_instance->thread = 0;
            break;
        case STOP_COMMAND:
            e = stop(self, cmd.asmp_itf);
            break;
        case PLAY_COMMAND:
            e = play(self, cmd.asmp_itf);
            break;
        case LOOP_COMMAND:
            e = loop(self, cmd.asmp_itf);
            break;
        case SYNC_COMMAND:
            e = recv_sync_cmd(self, cmd.asmp_itf);
            break;
        default:
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

static int aout_alsa_io_poll(rh_aout_api_itf self) {

    struct alsa_api_instance * api_instance = (struct alsa_api_instance *)self;

    rh_aout_itf * array;
    int len;
    int err=0;
    int sleep = 0;

    if( bucket_lock( api_instance->aout_itf_bucket, (void***)&array, &len ) == 0 ) {

        int i;
        int fd_count = 1; // reserved 1 for io.cmd_pipe

        struct pollfd *ufds = NULL;
        struct pollfd *ufds_next = NULL;

        // first pass - count file descriptors
        for(i=0; i<len; i++) {

            struct aout_instance * instance = (struct aout_instance*)array[i];
            int dc;

            if(!instance || !instance->handle)
                continue; // output not yet opened.

            if( instance->sleep )
                continue; // sample is draining, we are not interested in its writability.

            dc = snd_pcm_poll_descriptors_count( instance->handle );

            if(dc >= 0)
                fd_count += dc;
            else
                ++err;
        }

        // second pass - collect file descriptors.
        if(fd_count && ( ufds = ufds_next = alloca(sizeof(struct pollfd) * fd_count))) {

            ufds_next[0].fd = api_instance->cmd_pipe.read;
            ufds_next[0].revents = 0;
            ufds_next[0].events = POLLIN | POLLPRI;
            ++ufds_next;

            for(i=0; i<len; i++) {

                struct aout_instance * instance = (struct aout_instance*)array[i];

                if(!instance || !instance->handle)
                    continue; // output not yet opened.

                // find shortest sleep time of currently draining sample.
                if( instance->sleep && ( (!sleep) || ( instance->sleep < sleep ) ) )
                    sleep = instance->sleep;

                if(instance->sleep)
                    continue; // sample is draining, we have no interest in its writability.

                int n = snd_pcm_poll_descriptors_count( instance->handle );

                if(n>0) {
                    snd_pcm_poll_descriptors( instance->handle, ufds_next, n );
                    ufds_next += n;
                }
            }
        }
        bucket_unlock( api_instance->aout_itf_bucket );

        if(ufds) {
            int ms = sleep/1000;
            if(!ms && sleep)
                ms = 1;
            poll( ufds, fd_count, ms ? ms : -1 );
            return 0;
        }
    }

    return -1;
}


static void * api_main_loop(void * itf) {

    struct alsa_api_instance * instance = (struct alsa_api_instance *)itf;

    rh_aout_itf * array;

    int len;

    for(;;) {

        aout_alsa_io_poll(itf);

        process_cmd_pipe(itf);

        if( bucket_lock( instance->aout_itf_bucket, (void***)&array, &len ) == 0 ) {

            int i;
            for(i=0; i<len; i++)
                (*(array[i]))->update(array[i]);

            bucket_unlock( instance->aout_itf_bucket );
        }
    }

    return NULL;
}

static int _impl_setup(rh_aout_api_itf self) {

    struct alsa_api_instance * instance = (struct alsa_api_instance *)self;

    if( pipe( &instance->cmd_pipe.read ) != 0 )
        goto bad;

    if(fcntl( instance->cmd_pipe.read, F_SETFL, O_NONBLOCK) != 0)
        goto bad;

    if(bucket_create(&instance->aout_itf_bucket) != 0)
        goto bad;

    if(add_channels(self, 3) != 0)
        goto bad;

    {
        pthread_t thread;
        if(pthread_create(&thread, NULL, &api_main_loop, (void*)self) != 0)
            goto bad;
        instance->thread = thread;
    }

    pthread_detach( instance->thread );

good:
    return 0;

bad:
    if(instance->aout_itf_bucket) {
        close_all_channels(self);
        bucket_free(instance->aout_itf_bucket);
    }
    if(instance->cmd_pipe.write)
        close(instance->cmd_pipe.write);
    if(instance->cmd_pipe.read)
        close(instance->cmd_pipe.read);
    return -1;
}

static int _impl_shutdown(rh_aout_api_itf * itf) {

    struct alsa_api_instance * instance;

    if(!itf)
        return -1;

    instance = (struct alsa_api_instance *)*itf;

    if(instance) {

        if(instance->thread) {
            const struct io_command_struct cmd = { EXIT_COMMAND, NULL };
            if( pipe_send( *itf, &cmd ) == 0)
                while(instance->thread)
                    sched_yield();
        }

        close_all_channels(*itf);

        bucket_free(instance->aout_itf_bucket);
        instance->aout_itf_bucket = NULL;

        close(instance->cmd_pipe.write);
        instance->cmd_pipe.write = 0;

        close(instance->cmd_pipe.read);
        instance->cmd_pipe.read = 0;

        free(instance->interface);
        free(instance);
    }

    *itf = NULL;

    snd_config_update_free_global();

    return 0;
}

static int _impl_play(rh_aout_api_itf self, rh_asmp_itf sample) {

    int e = -1;

    struct alsa_api_instance * instance = (struct alsa_api_instance *)self;

    const struct io_command_struct cmd = { PLAY_COMMAND, sample };

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

    struct alsa_api_instance * instance = (struct alsa_api_instance *)self;

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

    struct alsa_api_instance * instance = (struct alsa_api_instance *)self;

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

    struct alsa_api_instance * instance = (struct alsa_api_instance *)self;

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

    struct alsa_api_instance * instance  = calloc(1, sizeof( struct alsa_api_instance ) );
    struct rh_aout_api       * interface = calloc(1, sizeof( struct rh_aout_api       ) );

    if(!instance || !interface)
        goto bad;

    instance->interface = interface;

    interface->setup    = &_impl_setup;
    interface->shutdown = &_impl_shutdown;
    interface->play     = &_impl_play;
    interface->loop     = &_impl_loop;
    interface->stop     = &_impl_stop;
    interface->sync     = &_impl_sync;

good:
    *itf = (rh_aout_api_itf)instance;
    return 0;

bad:
    free(instance);
    free(interface);
    return -1;
}

