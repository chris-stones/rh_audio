
#pragma once

#include "rh_audio.h"

#include <exception>
#include <stdexcept>

namespace rh {

	class AudioSample {

		rh_audio_itf audio_itf;

	public:

		class OpenException     : public std::exception {public: const char * what() const throw() { return "AudioSample::OpenException"; } };

		typedef enum {

			OPEN_DONTCOPYSOURCE = RH_AUDIO_OPEN_DONTCOPYSOURCE,

		} open_flags_enum_t;

		AudioSample(const char * source, int flags)
			:	audio_itf(NULL)
		{
			if( rh_audio_create(&audio_itf) != 0 )
				throw OpenException();

			if( (*audio_itf)->open(audio_itf, source, flags) != 0 )
				throw OpenException();
		}

		~AudioSample() {

			if(audio_itf) {
				Stop();
				(*audio_itf)->close(&audio_itf);
			}
		}

		bool Play()      { return (*audio_itf)->play      (audio_itf) == 0 ? true : false; }
		bool Loop()      { return (*audio_itf)->loop      (audio_itf) == 0 ? true : false; }
		bool Stop()      { return (*audio_itf)->stop      (audio_itf) == 0 ? true : false; }
		bool Wait()      { return (*audio_itf)->wait      (audio_itf) == 0 ? true : false; }
		bool IsPlaying() { return (*audio_itf)->is_playing(audio_itf) == 0 ? true : false; }
	};
}

