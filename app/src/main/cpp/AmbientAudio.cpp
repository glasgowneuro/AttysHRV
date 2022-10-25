//
// Created by bp1 on 18/10/2022.
//

#include "AmbientAudio.h"
#include <android/log.h>

void AmbientAudio::start() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setFormat(oboe::AudioFormat::Float);
    builder.setChannelCount(oboe::ChannelCount::Mono);
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
    wave1 = loadWAV(aAssetManager,"wave1.pcm");
}

std::vector<float> AmbientAudio::loadWAV(AAssetManager *aAssetManager, const char *name) {
    std::vector<uint16_t> raw;
    std::vector<float> audio;
    AAsset* asset = AAssetManager_open(aAssetManager,name,AASSET_MODE_BUFFER);
    if (!asset) {
        ALOGE("Asset %s does not exist.",name);
        return audio;
    }
    size_t nSamples = AAsset_getLength(asset) / 2;
    raw.resize(nSamples);
    unsigned n = AAsset_read(asset,raw.data(),nSamples * 2);
    if (n != (nSamples*2) ) {
        ALOGE("Only %d bytes loaded from %s.",n,name);
        return audio;
    }
    audio.resize(n);
    for(int i = 0; i < nSamples; i++) {
        audio[i] = (float)(raw[i]) / 32768.0f;
    }
    return audio;
}
