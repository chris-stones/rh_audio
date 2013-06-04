
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(rockhopper_INCLUDE_BASE)
LOCAL_EXPORT_CFLAGS := -DRH_TARGET_ANDROID=1

LOCAL_MODULE    := rh_audio
LOCAL_SRC_FILES := rh_audio_android.cpp
LOCAL_SRC_FILES += rh_audio_common.cpp

LOCAL_LDLIBS    += -lOpenSLES
LOCAL_LDLIBS    += -llog
LOCAL_LDLIBS    += -landroid

LOCAL_STATIC_LIBRARIES += bucket

#include $(BUILD_SHARED_LIBRARY)
include $(BUILD_STATIC_LIBRARY)

