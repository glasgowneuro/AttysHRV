//
// Created by bp1 on 18/10/2022.
//

#include "AmbientAudio.h"
#include <android/log.h>
#include <jni.h>
#include <random>

static std::vector<float> hrBuffer;

void AmbientAudio::start() {
    myCallback.ambientAudio = this;
    oboe::AudioStreamBuilder builder;
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

void AmbientAudio::AudioSource::fillBuffer(AmbientAudio::FrameData *buffer, int numFrames) {
    if (!isPlaying) return;
    FrameData* p = buffer;
    for (int i = 0; i < numFrames; ++i) {
        p->left  += wave[offset].left;
        p->right += wave[offset].right;
        p++;
        offset++;
        if (offset >= wave.size()) {
            offset = 0;
            if (!loopPlaying) {
                isPlaying = false;
                ALOGV("Stopped playing");
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


oboe::DataCallbackResult
AmbientAudio::MyCallback::onAudioReady(oboe::AudioStream *audioStream, void *audioData,
                                       int32_t numFrames) {
    auto *outputData = static_cast<FrameData *>(audioData);
    FrameData *p = outputData;

    const FrameData s = {0,0};
    for (int i = 0; i < numFrames; ++i) {
        *(p++) = s;
    }

    if (hrBuffer.size() > 2) {
        if (
                (hrBuffer[0] < hrBuffer[1]) &&
                (hrBuffer[1] < hrBuffer[2])
                ) {
            ALOGV("Monotonic HR: %f,%f,%f",
                  hrBuffer[0],
                  hrBuffer[1],
                  hrBuffer[2]);
            int i = (int)(random() % (long)numOfWaveSounds);
            ambientAudio->waveSounds[i].play(false);
        }
    }

    for(int i = 0; i < numOfWaveSounds; i++) {
        ambientAudio->waveSounds[i].fillBuffer(outputData, numFrames);
    }
    ambientAudio->backgroundSound.fillBuffer(outputData, numFrames);

    return DataCallbackResult::Continue;
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_oculusecg_ANativeActivity_hr4Sound(JNIEnv *env, jclass clazz, jlong inst,
                                                          jfloat v) {
    hrBuffer.push_back(v);
    if (hrBuffer.size() > 3) {
        hrBuffer.erase(hrBuffer.begin());
    }
}