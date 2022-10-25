//
// Created by bp1 on 18/10/2022.
//

#include "AmbientAudio.h"
#include <android/log.h>

void AmbientAudio::start() {
    myCallback.ambientAudio = this;
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setFormat(oboe::AudioFormat::I16);
    builder.setChannelCount(oboe::ChannelCount::Stereo);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setDataCallback(&myCallback)
            ->setFormat(oboe::AudioFormat::I16);
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
    wave1 = loadWAV(aAssetManager,"wave1.pcm");
}

std::vector<int16_t> AmbientAudio::loadWAV(AAssetManager *aAssetManager, const char *name) {
    std::vector<int16_t> raw;
    const int bytesinsample = sizeof(int16_t);
    ALOGV("Loading asset %s.",name);
    AAsset* asset = AAssetManager_open(aAssetManager,name,AASSET_MODE_BUFFER);
    if (!asset) {
        ALOGE("Asset %s does not exist.",name);
        return raw;
    }
    size_t nSamples = AAsset_getLength(asset) / bytesinsample;
    raw.resize(nSamples);
    const int actualNumberOfBytes = AAsset_read(asset, raw.data(), nSamples * 2);
    AAsset_close(asset);
    const int actualNumberOfSamples = actualNumberOfBytes / bytesinsample;
    raw.resize(actualNumberOfSamples);
    ALOGV("Loaded %d samples from %s.",actualNumberOfSamples, name);
    return raw;
}

oboe::DataCallbackResult
AmbientAudio::MyCallback::onAudioReady(oboe::AudioStream *audioStream, void *audioData,
                                       int32_t numFrames) {
    auto *outputData = static_cast<int16_t*>(audioData);
    int outputDataIndex = 0;

    for (int i = 0; i < numFrames; ++i) {
        outputData[outputDataIndex++] = ambientAudio->wave1[ambientAudio->frameptr++];
        outputData[outputDataIndex++] = ambientAudio->wave1[ambientAudio->frameptr++];
        if (ambientAudio->frameptr >= ambientAudio->wave1.size()) {
            ambientAudio->frameptr = 0;
        }
    }
    return DataCallbackResult::Continue;
}
