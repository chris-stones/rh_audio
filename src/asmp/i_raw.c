
/***
 *
 * Using backend that reads raw audio data from a file ( or Android APK )
 *
 */

#ifdef __ANDROID__
  #ifndef RH_TARGET_API_GLES2
    #define RH_TARGET_API_GLES2
  #endif
  #ifndef RH_TARGET_OS_ANDROID
    #define RH_TARGET_OS_ANDROID
  #endif
#endif

#ifdef __ANDROID__
	#include <android/log.h>
	#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
	#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))
	#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "native-activity", __VA_ARGS__))
#else
	#define LOGI(...) ((void)printf(__VA_ARGS__))
	#define LOGW(...) ((void)printf(__VA_ARGS__))
	#define LOGE(...) ((void)printf(__VA_ARGS__))
#endif

#ifdef RH_TARGET_OS_ANDROID
	#include<android/asset_manager.h>
#endif

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif //  __cplusplus

	#ifdef RH_TARGET_OS_ANDROID
	typedef AAsset 			AssetType;
	typedef AAssetManager		AssetManagerType;

	static inline AssetType * _OpenAsset( AssetManagerType * manager, const char * file) {

		return AAssetManager_open( manager, file, AASSET_MODE_STREAMING);
	}

	static inline int _ReadAsset(AssetType * asset, void * ptr, size_t count) {

		return AAsset_read(asset, ptr, count);
	}

	// returns -1 on error.
	static inline int _SeekAsset(AssetType * asset, off_t offset, int whence ) {

		return AAsset_seek(asset, offset, whence);
	}

	static inline void _CloseAsset(AssetType * asset) {

		AAsset_close(asset);
	}

	static inline AssetManagerType * _GetAssetManager() {

	  /*** MEGGA HACK! ***/

	  extern AAssetManager * __rh_hack_get_android_asset_manager();

	  return __rh_hack_get_android_asset_manager();
	}
#else
	typedef FILE	AssetType;
	typedef void	AssetManagerType;

	static inline AssetType * _OpenAsset( AssetManagerType * manager, const char * file) {

		return fopen(file, "rb");
	}

	// returns -1 on error. 0 on EOF, else, the bytes read.
	static inline int _ReadAsset(AssetType * asset, void * ptr, size_t count) {

		size_t r = fread(ptr, 1, count, asset);

		if(r == 0) {
			if(feof(asset))
				return 0;
			return -1;
		}

		return r;
	}

	static inline int _SeekAsset(AssetType * asset, off_t offset, int whence ) {

		return fseek(asset, offset, whence);
	}

	static inline void _CloseAsset(AssetType * asset) {

		fclose(asset);
	}

	static inline AssetManagerType * _GetAssetManager() {

	  return NULL;
	}
#endif


#include "asmp_internal.h"
#include<rh_raw_loader.h>

struct priv_internal {

	int stat;
	AssetManagerType 	* assetManager;
	AssetType 			* asset;
	rh_rawpak_ctx		* rawpak_ctx;
};

static inline struct priv_internal * get_priv(aud_sample_handle p) {

  return (struct priv_internal *)p->priv;
}

static inline int _read(void* data, size_t size, size_t nbemb, aud_sample_handle p) {

	struct priv_internal * priv = get_priv(p);

	if(priv->rawpak_ctx)
		return rh_rawpak_read(data, size, nbemb, priv->rawpak_ctx);
	else if(priv->asset)
		return _ReadAsset(priv->asset, data, size * nbemb);
	else
		return -1;
}

static inline int _seek(aud_sample_handle p, long offset, int whence) {

	struct priv_internal * priv = get_priv(p);

	if(priv->rawpak_ctx)
		return rh_rawpak_seek(priv->rawpak_ctx, offset, whence);
	else if(priv->asset)
		return _SeekAsset(priv->asset, offset, whence);
	else
		return -1;
}

static int _aud_sample_opener(aud_sample_handle p, const char * const fn) {

  struct priv_internal *priv = calloc(1, sizeof(struct priv_internal));

  if(priv) {

		if(strncmp("rh_rawpak_ctx://",fn,16)==0) {

			void * p = NULL;

			if(sscanf(fn+16,"%p",&p) != 1) {
				free(priv);
				return -1;
			}

			priv->rawpak_ctx = (rh_rawpak_ctx)p;
		}
		else {

			priv->assetManager = _GetAssetManager();
			priv->asset = _OpenAsset(priv->assetManager, fn);

			if(!priv->asset) {
				priv->asset = NULL;
				priv->assetManager = 0;
				free(priv);
				return -1;
			}
		}

		p->priv = (void*)priv;

		// todo - what format is your raw data in?
		p->channels 	= 2;
		p->samplerate 	= 44100;
		p->samplesize	= 2;

		return 0;
  }

  return -1;
}

static int _aud_sample_reader(aud_sample_handle p, int samples, void * dst, size_t dst_size) {

	int err = _read(dst, 1, samples * p->channels * 2, p);

	if(err <= 0) {
		get_priv(p)->stat = 1;
		return 0;
	}

	return err / (p->channels * 2);
}

static int _aud_sample_stater(aud_sample_handle p) {

	return get_priv(p)->stat;
}

static int _aud_sample_resetter(aud_sample_handle p) {

	_seek(p, 0, SEEK_SET);
	get_priv(p)->stat = 0;
	return 0;
}

static int _aud_sample_closer(aud_sample_handle p) {

  struct priv_internal * priv = get_priv(p);

  if( priv->asset )
	  _CloseAsset(priv->asset);

  free(p->priv);
  p->priv = NULL;

  return 0;
}

int aud_init_interface_raw(aud_sample_handle p) {

  p->opener = &_aud_sample_opener;
  p->reader = &_aud_sample_reader;
  p->reseter = &_aud_sample_resetter;
  p->closer = &_aud_sample_closer;
  p->stater = &_aud_sample_stater;

  return 0;
}

