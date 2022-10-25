//
// Created by bp1 on 18/10/2022.
//

#ifndef OCULUSECG_AMBIENTAUDIO_H
#define OCULUSECG_AMBIENTAUDIO_H

#include <oboe/Oboe.h>
#include <math.h>
#include <android/asset_manager.h>
#include <vector>
#include "util.h"
using namespace oboe;

class AmbientAudio {
public:
    void init(AAssetManager *aAssetManager);
    void start();
    void stop();

private:

    class MyCallback : public oboe::AudioStreamDataCallback {
    public:
        oboe::DataCallbackResult
        onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override;
        AmbientAudio* ambientAudio;
    };

    MyCallback myCallback;
    std::shared_ptr<oboe::AudioStream> mStream;
    std::vector<int16_t> wave1;

    std::vector<int16_t> loadWAV(AAssetManager *aAssetManager, const char* name);

    int frameptr = 0;
};


#endif //OCULUSECG_AMBIENTAUDIO_H
