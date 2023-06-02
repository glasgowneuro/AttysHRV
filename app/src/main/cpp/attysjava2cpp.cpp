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

////////////////////////////////////////////////
// Init callback that the Attys has been started
std::vector<std::function<void(float)>> attysInitCallbacks;

void registerAttysInitCallback(const std::function<void(float)> &f) {
    attysInitCallbacks.emplace_back(f);
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_attyshrv_ANativeActivity_initJava2CPP(JNIEnv *env,
                                                              jclass clazz,
                                                              jfloat fs) {
    ALOGV("Settting up the notch filter and HR detector: fs = %f", fs);
    for(auto &v : attysInitCallbacks) {
        v(fs);
    }
    if (fs < 125) return;
    iirnotch.setup(fs, 50, 2.5);
    rrDet.init(fs);
}

//////////////////////////////////
// filename

std::string attysHRfilepath;

std::string getAttysHRfilepath() {
    return  attysHRfilepath;
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_attyshrv_ANativeActivity_setHRfilePath(JNIEnv *env, jclass clazz,
                                                              jstring path) {
    const char *fnUTF = env->GetStringUTFChars(path, NULL);
    ALOGV("Callback from onCreate for HR with path: %s",fnUTF);
    attysHRfilepath = std::string(fnUTF);
    env->ReleaseStringUTFChars(path, fnUTF);
}


void unregisterAllAttysCallbacks() {
    ALOGV("Unregistering all Attys callbacks");
    attysHRCallbacks.clear();
    attysDataCallbacks.clear();
    attysInitCallbacks.clear();
}
