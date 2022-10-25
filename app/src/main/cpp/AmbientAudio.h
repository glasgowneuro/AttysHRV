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
        onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override {

            // We requested AudioFormat::Float. So if the stream opens
            // we know we got the Float format.
            // If you do not specify a format then you should check what format
            // the stream has and cast to the appropriate type.
            auto *outputData = static_cast<float *>(audioData);

            // Generate random numbers (white noise) centered around zero.
            const float amplitude = 0.2f;
            for (int i = 0; i < numFrames; ++i){
                outputData[i] = ((float)drand48() - 0.5f) * 2 * amplitude;
            }

            return oboe::DataCallbackResult::Continue;
        }
    };

    MyCallback myCallback;
    std::shared_ptr<oboe::AudioStream> mStream;
    std::vector<float> wave1;

    std::vector<float> loadWAV(AAssetManager *aAssetManager, const char* name);
};


#endif //OCULUSECG_AMBIENTAUDIO_H
