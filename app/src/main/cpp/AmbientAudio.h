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
#include "attysjava2cpp.h"

using namespace oboe;

static constexpr int samplingRate = 48000;
const std::string nameBackgroundSound = "ocean-waves.pcm";
static constexpr int numOfWaveSounds = 5;
static constexpr int buffersize = 4;
const std::string namesOfWaves[numOfWaveSounds] = {"wave1.pcm",
                                                   "wave2.pcm",
                                                   "wave3.pcm",
                                                   "wave4.pcm",
                                                   "wave5.pcm"};

class AmbientAudio {
public:
    void init(AAssetManager *aAssetManager);
    void start();
    void stop();

private:

    struct __attribute__ ((packed)) FrameData {
        float left;
        float right;
    };

    class MyCallback : public oboe::AudioStreamDataCallback {
    public:
        oboe::DataCallbackResult
        onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override;
        AmbientAudio* ambientAudio;
    };

    class AudioSource {
    public:
        void loadWAV(AAssetManager *aAssetManager, const std::string &name);
        void fillBuffer(FrameData* buffer, int numFrames,float gain = 1.0f);
        void play(bool doLoopPlaying = true);
        void stop() { isPlaying = false; }
    private:
        std::vector<FrameData> wave;
        int offset = 0;
        int offset2 = 0;
        bool isPlaying = false;
        bool loopPlaying = false;
    };

    AudioSource waveSounds[numOfWaveSounds];
    AudioSource backgroundSound;

    void hasHR(float hr);

    MyCallback myCallback;
    std::shared_ptr<oboe::AudioStream> mStream;
    std::vector<float> hrBuffer;
};


#endif //OCULUSECG_AMBIENTAUDIO_H
