
#pragma once

#include "rh_audio.h"

#include <exception>
#include <stdexcept>

namespace rh {

	class AudioSample {

		rh_audio_itf audio_itf;

	public:

		class OpenException     : public std::exception {public: const char * what() const throw() { return "AudioSample::OpenException";     } };
		class SetupApiException : public std::exception {public: const char * what() const throw() { return "AudioSample::SetupApiException"; } };

		AudioSample(int flags, const char * source)
			:	audio_itf(NULL)
		{
			__addref();

			if( rh_audio_create(&audio_itf) != 0 )
				throw OpenException();

			if( (*audio_itf)->open(audio_itf, source, flags) != 0 )
				throw OpenException();
		}

		AudioSample(int flags, const char * format, ...)
					:	audio_itf(NULL)
		{
			__addref();

			if( rh_audio_create(&audio_itf) != 0 )
				throw OpenException();

			va_list va;
			va_start(va, format);

			int e = (*audio_itf)->vopenf(audio_itf, flags, format, va);

			va_end(va);

			if( e != 0 )
				throw OpenException();
		}

		~AudioSample() {

			if(audio_itf)
				(*audio_itf)->close(&audio_itf);

			__subref();
		}

		bool Play()      { return (*audio_itf)->play      (audio_itf) == 0 ? true : false; }
		bool Loop()      { return (*audio_itf)->loop      (audio_itf) == 0 ? true : false; }
		bool Stop()      { return (*audio_itf)->stop      (audio_itf) == 0 ? true : false; }
		bool Wait()      { return (*audio_itf)->wait      (audio_itf) == 0 ? true : false; }
		bool IsPlaying() { return (*audio_itf)->is_playing(audio_itf) == 0 ? true : false; }

	private:

		size_t & __getrefcount() {
			static size_t __refcount = 0;
			return __refcount;
		}
		void __addref() {
			if(__getrefcount()++ ==0)
				if(rh_audio_setup_api() != 0)
					throw SetupApiException();
		}
		void __subref() {
			__getrefcount()--;
			if(__getrefcount()==0)
				rh_audio_shutdown_api();
		}
	};
}

