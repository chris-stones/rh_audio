
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#LOCAL_C_INCLUDES := $(rockhopper_INCLUDE_BASE)
LOCAL_C_INCLUDES := $(ffmpeg_INCLUDES)

LOCAL_EXPORT_CFLAGS := -DRH_TARGET_ANDROID=1

LOCAL_MODULE    := rh_audio
LOCAL_SRC_FILES := rh_audio.c
LOCAL_SRC_FILES += asmp/asmp.c asmp/i_ffmpeg.c asmp/i_s5prom.c
LOCAL_SRC_FILES += aout/OpenSLES/sles.c aout/OpenSLES/update.c aout/OpenSLES/buffer_queue.c
LOCAL_SRC_FILES += bucket.c

LOCAL_LDLIBS    += -lOpenSLES
LOCAL_LDLIBS    += -llog
LOCAL_LDLIBS    += -landroid

#LOCAL_STATIC_LIBRARIES := ffmpeg-prebuilt-avformat
#LOCAL_STATIC_LIBRARIES += ffmpeg-prebuilt-avcodec
#LOCAL_STATIC_LIBRARIES += ffmpeg-prebuilt-swresample
#LOCAL_STATIC_LIBRARIES += ffmpeg-prebuilt-swscale
#LOCAL_STATIC_LIBRARIES += ffmpeg-prebuilt-avutil
LOCAL_STATIC_LIBRARIES := rh_raw_loader

#include $(BUILD_SHARED_LIBRARY)
include $(BUILD_STATIC_LIBRARY)

