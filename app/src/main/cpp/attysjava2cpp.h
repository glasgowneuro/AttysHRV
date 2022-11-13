//
// Created by Bernd Porr on 13/11/2022.
//

#ifndef OCULUSECG_ATTYSJAVA2CPP_H
#define OCULUSECG_ATTYSJAVA2CPP_H

#include <android/native_window_jni.h> // for native window JNI
#include <functional>
#include <vector>

/**
 * Registers a callback to get the raw data from channel 1
 * @param f is a pointer to a function receiving the data
 */
void registerAttysDataCallback(const std::function<void(float)>& f);

/**
 * Registers a callback to receive the heartrate
 * @param f heartrate in BPM
 */
void registerAttysHRCallback(const std::function<void(float)>& f);

#endif //OCULUSECG_ATTYSJAVA2CPP_H
