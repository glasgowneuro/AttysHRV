// (c) 2022 Bernd Porr, bernd@glasgowneuro.tech
// GNU GENERAL PUBLIC LICENSE
// Version 3, 29 June 2007
//

#include "AmbientAudio.h"
#include <android/log.h>
#include <jni.h>
#include <random>

void AmbientAudio::start() {
    myCallback.ambientAudio = this;
    oboe::AudioStreamBuilder builder;
    builder.setSampleRate(samplingRate);
    builder.setDirection(oboe::Direction::Output);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setFormat(oboe::AudioFormat::Float);
    builder.setChannelCount(oboe::ChannelCount::Stereo);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setDataCallback(&myCallback)
            ->setFormat(oboe::AudioFormat::Float);
    oboe::Result result = builder.openStream(mStream);
    if (result != oboe::Result::OK) {
        ALOGE("Failed to create stream. Error: %s", oboe::convertToText(result));
        return;
    }
    oboe::AudioFormat format = mStream->getFormat();
    ALOGV("AudioStream format is %s", oboe::convertToText(format));
    mStream->requestStart();
}

void AmbientAudio::stop() {
    if (mStream) {
        mStream->requestStop();
        mStream->close();
        mStream.reset();
    }
}

void AmbientAudio::init(AAssetManager *aAssetManager) {
    registerAttysHRCallback([this](float hr){ hasHR(hr); });
    for(int i = 0; i < numOfWaveSounds; i++) {
        waveSounds[i].loadWAV(aAssetManager, namesOfWaves[i]);
    }
    backgroundSound.loadWAV(aAssetManager, nameBackgroundSound);
    backgroundSound.play();
}

void AmbientAudio::AudioSource::loadWAV(AAssetManager *aAssetManager, const std::string &name) {
    ALOGV("Loading asset %s.",name.c_str());
    AAsset* asset = AAssetManager_open(aAssetManager,name.c_str(),AASSET_MODE_BUFFER);
    if (!asset) {
        ALOGE("Asset %s does not exist.",name.c_str());
        return;
    }
    const size_t assetLength = AAsset_getLength(asset);
    std::vector<unsigned char> tmp;
    tmp.resize(assetLength);
    const long int actualNumberOfBytes = AAsset_read(asset, tmp.data(), assetLength);
    AAsset_close(asset);
    if (assetLength != actualNumberOfBytes) {
        ALOGE("Asset read %s: expected %ld bytes but only got %ld bytes.",
              name.c_str(),AAsset_getLength(asset),actualNumberOfBytes);
        return;
    }
    const int bytesinframe = sizeof(FrameData);
    const size_t nFrames = actualNumberOfBytes / bytesinframe;
    wave.resize(nFrames);
    const long int actualNumberOfFrames = actualNumberOfBytes / bytesinframe;
    wave.resize(actualNumberOfFrames);
    for(long int i = 0; i < actualNumberOfFrames; i++) {
        wave[i].left = (float)((int16_t)(tmp[4*i]) + (int16_t)(tmp[4*i+1] << 8))/32768.f;
        wave[i].right = (float)((int16_t)(tmp[4*i+2]) + (int16_t)(tmp[4*i+3] << 8))/32768.f;
    }
    ALOGV("Loaded %ld frames from %s.",actualNumberOfFrames, name.c_str());
}

void AmbientAudio::AudioSource::fillBuffer(AmbientAudio::FrameData *buffer, int numFrames,float gain) {
    if (!isPlaying) return;
    FrameData* p = buffer;
    if (!loopPlaying) {
        for (int i = 0; i < numFrames; ++i) {
            p->left += wave[offset].left * gain;
            p->right += wave[offset].right * gain;
            p++;
            offset++;
            if (offset >= wave.size()) {
                offset = 0;
                    isPlaying = false;
                    ALOGV("Stopped playing");
            }
        }
    } else {
        const long int sampleOverlap = samplingRate;
        for (int i = 0; i < numFrames; ++i) {
            if (offset < (wave.size() - sampleOverlap)) {
                p->left += wave[offset].left * gain;
                p->right += wave[offset].right * gain;
                p++;
                offset++;
                offset2 = 0;
            } else {
                float w = (float)offset2 / (float)sampleOverlap;
                if (w > 1) w = 1;
                p->left += wave[offset].left * gain * (1 - w);
                p->right += wave[offset].right * gain * (1 - w);
                p->left += wave[offset2].left * gain * w;
                p->right += wave[offset2].right * gain * w;
                p++;
                offset++;
                offset2++;
                if (offset >= wave.size()) {
                    offset = offset2;
                    ALOGV("Rewind at %d, weight=%f",offset2,w);
                }
            }
        }
    }
}

void AmbientAudio::AudioSource::play(bool doLoopPlaying) {
    if (isPlaying) return;
    isPlaying = true;
    loopPlaying = doLoopPlaying;
    offset = 0;
    ALOGV("Started playing");
}

void AmbientAudio::hasHR(float hr) {
    hrBuffer.push_back(hr);
    if (hrBuffer.size() > buffersize) {
        hrBuffer.erase(hrBuffer.begin());
    }
}

oboe::DataCallbackResult
AmbientAudio::MyCallback::onAudioReady(oboe::AudioStream *audioStream,
                                       void *audioData,
                                       int32_t numFrames) {
    auto *outputData = static_cast<FrameData *>(audioData);
    FrameData *p = outputData;

    const FrameData s = {0,0};
    for (int i = 0; i < numFrames; ++i) {
        *(p++) = s;
    }

    if (ambientAudio->hrBuffer.size() == buffersize) {
        if (
                (ambientAudio->hrBuffer[0] < ambientAudio->hrBuffer[1]) &&
                (ambientAudio->hrBuffer[1] < ambientAudio->hrBuffer[2]) &&
                (ambientAudio->hrBuffer[2] < ambientAudio->hrBuffer[3])
                ) {
            int i = (int)(random() % (long)numOfWaveSounds);
            ambientAudio->waveSounds[i].play(false);
            ALOGV("Descending HR values: %f %f %f %f",
                  ambientAudio->hrBuffer[0],
                  ambientAudio->hrBuffer[1],
                  ambientAudio->hrBuffer[2],
                  ambientAudio->hrBuffer[3]);
        }
    }

    for(auto & waveSound : ambientAudio->waveSounds) {
        waveSound.fillBuffer(outputData, numFrames, 0.1);
    }
    ambientAudio->backgroundSound.fillBuffer(outputData, numFrames);

    return DataCallbackResult::Continue;
}
