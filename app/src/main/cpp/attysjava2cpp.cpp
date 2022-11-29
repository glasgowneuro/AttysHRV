//
// Created by bp1 on 13/11/2022.
//

#include "attysjava2cpp.h"
#include "util.h"
#include "engzee.h"
#include "Iir.h"

////////////////////////////////
// Heartrate callback from java
std::vector<std::function<void(float)>> attysHRCallbacks;

void registerAttysHRCallback(const std::function<void(float)>& f){
    attysHRCallbacks.emplace_back(f);
}

struct MyHRCallBack : HRCallback {
    void hasHR(float hr) override {
        for (auto &cb: attysHRCallbacks) {
            cb(hr);
        }
    }
};

Iir::Butterworth::BandStop<4> iirnotch;
MyHRCallBack hrCallBack;
Engzee engzee(hrCallBack);

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
        data = iirnotch.filter(data);
        engzee.detect(data);
        v(data);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_oculusecg_ANativeActivity_init_1java2cpp(JNIEnv *env, jclass clazz,
                                                                jfloat fs) {
    iirnotch.setup(fs,50,2.5);
    engzee.init(fs);
}