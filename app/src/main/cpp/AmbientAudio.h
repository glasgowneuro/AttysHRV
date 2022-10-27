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

    struct __attribute__ ((packed)) FrameData {
        int16_t left;
        int16_t right;
    };

    class MyCallback : public oboe::AudioStreamDataCallback {
    public:
        oboe::DataCallbackResult
        onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override;
        AmbientAudio* ambientAudio;
    };

    class AudioSource {
    public:
        void loadWAV(AAssetManager *aAssetManager, const char* name);
        void fillBuffer(FrameData* buffer, int numFrames);
    private:
        std::vector<FrameData> wave;
        int offset = 0;
    };

    AudioSource audioSource1;

    MyCallback myCallback;
    std::shared_ptr<oboe::AudioStream> mStream;
};


#endif //OCULUSECG_AMBIENTAUDIO_H
