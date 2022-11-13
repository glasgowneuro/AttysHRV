//
// Created by Bernd Porr on 13/11/2022.
//

#ifndef OCULUSECG_ATTYSJAVA2CPP_H
#define OCULUSECG_ATTYSJAVA2CPP_H

//////////////////////////////
// Raw data callback from JAVA
std::vector<std::function<void(float)>> attysDataCallbacks;

extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_oculusecg_ANativeActivity_dataUpdate(JNIEnv *,jclass,
                                                            jlong instance,
                                                            jfloat data) {
    for (auto &v: attysDataCallbacks) {
        v(data);
    }
}

void registerAttysDataCallback(std::function<void(float)> f) {
    ALOGV("Registered callback # %lu for Attys data.",(long)attysDataCallbacks.size());
    attysDataCallbacks.emplace_back(f);
}


////////////////////////////////
// Heartrate callback from java
std::vector<std::function<void(float)>> attysHRCallbacks;

extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_oculusecg_ANativeActivity_hrUpdate(JNIEnv *,jclass, jlong, jfloat v) {
    for(auto &cb: attysHRCallbacks) {
        cb(v);
    }
}

void registerAttysHRCallback(std::function<void(float)> f){
    attysHRCallbacks.emplace_back(f);
}


#endif //OCULUSECG_ATTYSJAVA2CPP_H
