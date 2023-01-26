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
 * @param f Callback with the currnet heartrate in BPM
 */
void registerAttysHRCallback(const std::function<void(float)>& f);

/**
 * Registering a callback when the Attys has been initialised or failed.
 * @param f Callback function which has the sampling rate as the argument
 */
void registerAttysInitCallback(const std::function<void(float)> &f);


void unregisterAllAttysCallbacks();

#endif //OCULUSECG_ATTYSJAVA2CPP_H
