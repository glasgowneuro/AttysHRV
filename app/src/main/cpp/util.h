//
// Created by bp1 on 03/10/2022.
//

#ifndef OCULUSECG_UTIL_H
#define OCULUSECG_UTIL_H

#include <android/log.h>

#define DEBUG 1
#define LOG_TAG "OculusECG"

#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )
#if DEBUG
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#else
#define ALOGV(...)
#endif

#endif //OCULUSECG_UTIL_H
