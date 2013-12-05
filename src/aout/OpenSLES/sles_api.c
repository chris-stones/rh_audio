#include "sles_private.h"

#include<alloca.h>
#include<pthread.h>
#include<signal.h>
#include<error.h>
#include<errno.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdio.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/asset_manager.h>
#include <android/native_activity.h>
#include <android/log.h>

#include"../../bucket.h"

typedef enum {

    EXIT_COMMAND,

    LOOP_COMMAND,
    PLAY_COMMAND,
    STOP_COMMAND,

    CONSUMED_BUFFER,

    SYNC_COMMAND,

} command_enum_t;

struct io_command_struct {

    int command;
    rh_asmp_itf asmp_itf;
};

static int pipe_send( rh_aout_api_itf self, const struct io_command_struct *cmd ) {

    struct sles_api_instance * api_instance = (struct sles_api_instance *)self;

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
                return -1;
            }
        }
    } while(p < e);

    return 0;
}

// return negative on error, 0 on nothing read, 1 on struct read.
static int pipe_recv( rh_aout_api_itf self, struct io_command_struct *cmd ) {

    struct sles_api_instance * api_instance = (struct sles_api_instance *)self;

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
    struct sles_api_instance * api_instance = (struct sles_api_instance *)self;

    for(i=0; i<channels; i++) {
		printf("about to create audio channel\n");
        if( rh_aout_create_sles(self, &(itf[i])) != 0) {
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

    struct sles_api_instance * api_instance = (struct sles_api_instance *)self;

    int len;
    rh_aout_itf * array;

    if( bucket_lock( api_instance->aout_itf_bucket, (void***)&array, &len ) == 0 ) {

        int i;
        for(i=0; i<len; i++)
			if(array[i])
				(*(array[i]))->close(&array[i]);

        bucket_unlock( api_instance->aout_itf_bucket );
    }

    return 0;
}

static int recv_sync_cmd(rh_aout_api_itf self, rh_asmp_itf asmp_itf) {

    (*asmp_itf)->on_output_event(asmp_itf, RH_ASMP_OUTPUT_EVENT_SYNC);

    return 0;
}

static int stop(rh_aout_api_itf self, rh_asmp_itf asmp_itf) {

    struct sles_api_instance * api_instance = (struct sles_api_instance *)self;

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

static int recv_consumed_buffer_cmd(rh_aout_api_itf self, rh_asmp_itf asmp_itf) {

	struct sles_api_instance * api_instance = (struct sles_api_instance *)self;

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

					struct aout_instance * channel_instance =
						(struct aout_instance *)audio_channel;

					e = buffer_queue_return_drain_buffer( &channel_instance->bq );

					break;
				}
			}
		}

		bucket_unlock( api_instance->aout_itf_bucket );
	}

	return e;
}

static int _play(rh_aout_api_itf self, rh_asmp_itf asmp_itf, int loop_flag) {

    struct sles_api_instance * api_instance = (struct sles_api_instance *)self;

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

                            found = 1;

                            (*audio_channel)->set_sample(audio_channel, asmp_itf);

                            (*audio_channel)->open(
                                audio_channel,
                                (*asmp_itf)->channels(asmp_itf),
                                (*asmp_itf)->samplerate(asmp_itf),
                                (*asmp_itf)->samplesize(asmp_itf) );

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

    struct sles_api_instance * api_instance = (struct sles_api_instance *)self;

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
        case CONSUMED_BUFFER:
        	e = recv_consumed_buffer_cmd(self, cmd.asmp_itf);
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

static int poll_cmd_pipe(rh_aout_api_itf self) {

	struct sles_api_instance * instance = (struct sles_api_instance *)self;

	struct pollfd ufds[1];

	ufds[0].fd = instance->cmd_pipe.read;
    ufds[0].revents = 0;
    ufds[0].events = POLLIN | POLLPRI;

	poll( ufds, sizeof ufds / sizeof ufds[0], -1 );

	return 0;
}

static void * api_main_loop(void * itf) {

	rh_aout_api_itf self = (rh_aout_api_itf)itf;

    struct sles_api_instance * instance = (struct sles_api_instance *)itf;

    rh_aout_itf * array;

    int len;

    for(;;) {

    	poll_cmd_pipe(self);

        process_cmd_pipe(self);

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

	extern AAssetManager * __rh_hack_get_android_asset_manager();

	static const SLEngineOption options[] = {
			{ SL_ENGINEOPTION_THREADSAFE, 		SL_BOOLEAN_TRUE },
			{ SL_ENGINEOPTION_LOSSOFCONTROL, 	SL_BOOLEAN_FALSE },
	};

    struct sles_api_instance * instance = (struct sles_api_instance *)self;

    instance->asset_manager = __rh_hack_get_android_asset_manager();

    if(!instance->asset_manager)
    	goto bad;

    if (SL_RESULT_SUCCESS
			!= slCreateEngine(&instance->engineObject,
					sizeof(options) / sizeof(options[0]), options, 0, NULL,
					NULL))
		goto bad;

	if (SL_RESULT_SUCCESS
			!= (*instance->engineObject)->Realize(instance->engineObject,
					SL_BOOLEAN_FALSE ))
		goto bad;

	if (SL_RESULT_SUCCESS
			!= (*instance->engineObject)->GetInterface(instance->engineObject,
					SL_IID_ENGINE, &instance->engineItf))
		goto bad;

	if (SL_RESULT_SUCCESS
			!= (*instance->engineItf)->CreateOutputMix(instance->engineItf,
					&instance->outputMix, 0, NULL, NULL))
		goto bad;

	if (SL_RESULT_SUCCESS
			!= (*instance->outputMix)->Realize(instance->outputMix,
					SL_BOOLEAN_FALSE ))
		goto bad;

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

	if( instance->outputMix )
		(*instance->outputMix)->Destroy(instance->outputMix);

	if( instance->engineObject )
		(*instance->engineObject)->Destroy(instance->engineObject);

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

    struct sles_api_instance * instance;

    if(!itf)
        return -1;

    instance = (struct sles_api_instance *)*itf;

    if(instance) {

    	if( instance->outputMix )
			(*instance->outputMix)->Destroy(instance->outputMix);

		if( instance->engineObject )
			(*instance->engineObject)->Destroy(instance->engineObject);

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

    return 0;
}

static int _impl_play(rh_aout_api_itf self, rh_asmp_itf sample) {

    int e = -1;

    struct sles_api_instance * instance = (struct sles_api_instance *)self;

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

    struct sles_api_instance * instance = (struct sles_api_instance *)self;

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

    struct sles_api_instance * instance = (struct sles_api_instance *)self;

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

    struct sles_api_instance * instance = (struct sles_api_instance *)self;

    const struct io_command_struct cmd = { SYNC_COMMAND, sample };

    if( sample ) {
        // create a reference for the command pipe
        (*sample)->addref(sample);
        if((e = pipe_send(self, &cmd))!=0)
            (*sample)->close(&sample);
    }

    return e;
}

int _impl_consumed_buffer(rh_aout_api_itf self, rh_asmp_itf sample) {

    int e = -1;

    struct sles_api_instance * instance = (struct sles_api_instance *)self;

    const struct io_command_struct cmd = { CONSUMED_BUFFER, sample };

    if( sample ) {
        // create a reference for the command pipe
        (*sample)->addref(sample);
        if((e = pipe_send(self, &cmd))!=0)
            (*sample)->close(&sample);
    }

    return e;
}

int rh_aout_create_api( rh_aout_api_itf * itf ) {

    struct sles_api_instance * instance  = calloc(1, sizeof( struct sles_api_instance ) );
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

