//
// Created by bp1 on 13/11/2022.
//

#include "attysjava2cpp.h"
#include "util.h"

//////////////////////////////
// Raw data callback from JAVA
std::vector<std::function<void(float)>> attysDataCallbacks;

void registerAttysDataCallback(const std::function<void(float)>& f) {
    ALOGV("Registered callback # %lu for Attys data.",(long)attysDataCallbacks.size());
    attysDataCallbacks.emplace_back(f);
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_oculusecg_ANativeActivity_dataUpdate(JNIEnv *,jclass,
                                                            jlong instance,
                                                            jfloat data) {
    for (
        auto &v
            : attysDataCallbacks) {
        v(data);
    }
}

////////////////////////////////
// Heartrate callback from java
std::vector<std::function<void(float)>> attysHRCallbacks;

void registerAttysHRCallback(const std::function<void(float)>& f){
    attysHRCallbacks.emplace_back(f);
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_oculusecg_ANativeActivity_hrUpdate(JNIEnv *,jclass, jlong, jfloat v) {
    for (auto &cb: attysHRCallbacks) {
        cb(v);
    }
}

