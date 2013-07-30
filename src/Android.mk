
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(rockhopper_INCLUDE_BASE)
LOCAL_EXPORT_CFLAGS := -DRH_TARGET_ANDROID=1

LOCAL_MODULE    := rh_audio
LOCAL_SRC_FILES := rh_audio_android.c
LOCAL_SRC_FILES += rh_audio_common.c
LOCAL_SRC_FILES += bucket.c

LOCAL_LDLIBS    += -lOpenSLES
LOCAL_LDLIBS    += -llog
LOCAL_LDLIBS    += -landroid

#include $(BUILD_SHARED_LIBRARY)
include $(BUILD_STATIC_LIBRARY)

