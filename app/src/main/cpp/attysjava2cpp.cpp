//
// Created by bp1 on 13/11/2022.
//

#include "attysjava2cpp.h"
#include "util.h"
#include "ecg_rr_det.h"
#include "Iir.h"

////////////////////////////////
// Heartrate callback from java
std::vector<std::function<void(float)>> attysHRCallbacks;

void registerAttysHRCallback(const std::function<void(float)> &f) {
    attysHRCallbacks.emplace_back(f);
}


class MyHRCallBack: public ECG_rr_det::RRlistener {
public:
    void hasRpeak(long,
                          float bpm,
                          double,
                          double) override {
        ALOGV("HR = %f",bpm);
        for (auto &cb: attysHRCallbacks) {
            cb(bpm);
        }
    }
};

MyHRCallBack hrCallBack;
ECG_rr_det rrDet(&hrCallBack);

/////////////////////////////////
// Raw data callback from JAVA
std::vector<std::function<void(float)>> attysDataCallbacks;

void registerAttysDataCallback(const std::function<void(float)> &f) {
    ALOGV("Registered callback # %lu for Attys data.", (long) attysDataCallbacks.size());
    attysDataCallbacks.emplace_back(f);
}

Iir::Butterworth::BandStop<2> iirnotch;

extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_attyshrv_ANativeActivity_dataUpdate(JNIEnv *, jclass, jlong instance, jfloat data) {
    data = iirnotch.filter(data);
    rrDet.detect(data);
    for (auto &v : attysDataCallbacks) {
        v(data);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_attyshrv_ANativeActivity_initJava2CPP(JNIEnv *env,
                                                              jclass clazz,
                                                              jfloat fs) {
    ALOGV("Settting up the notch filter and HR detector: fs = %f", fs);
    iirnotch.setup(fs, 50, 2.5);
    rrDet.init(fs);
}

void unregisterAllAttysCallbacks() {
    ALOGV("Unregistering all Attys callbacks");
    attysHRCallbacks.clear();
    attysDataCallbacks.clear();
}