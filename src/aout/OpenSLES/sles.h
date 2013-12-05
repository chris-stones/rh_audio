
#pragma once

#include "../aout_internal.h"
#include "buffer_queue.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/asset_manager.h>
#include <android/native_activity.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "native-activity", __VA_ARGS__)



