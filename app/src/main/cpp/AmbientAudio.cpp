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
    audioSource1.loadWAV(aAssetManager,"wave1.pcm");
}

void AmbientAudio::AudioSource::loadWAV(AAssetManager *aAssetManager, const char *name) {
    const int bytesinframe = sizeof(FrameData);
    ALOGV("Loading asset %s.",name);
    AAsset* asset = AAssetManager_open(aAssetManager,name,AASSET_MODE_BUFFER);
    if (!asset) {
        ALOGE("Asset %s does not exist.",name);
        return;
    }
    size_t nFrames = AAsset_getLength(asset) / bytesinframe;
    wave.resize(nFrames);
    const int actualNumberOfBytes = AAsset_read(asset, wave.data(), nFrames * bytesinframe);
    AAsset_close(asset);
    const int actualNumberOfFrames = actualNumberOfBytes / bytesinframe;
    wave.resize(actualNumberOfFrames);
    ALOGV("Loaded %d frames from %s.",actualNumberOfFrames, name);
}

void AmbientAudio::AudioSource::fillBuffer(AmbientAudio::FrameData *buffer, int numFrames) {
    FrameData* p = buffer;
    for (int i = 0; i < numFrames; ++i) {
        *(p++) = wave[offset++];
        if (offset >= wave.size()) {
            offset = 0;
        }
    }
}

oboe::DataCallbackResult
AmbientAudio::MyCallback::onAudioReady(oboe::AudioStream *audioStream, void *audioData,
                                       int32_t numFrames) {
    auto *outputData = static_cast<FrameData*>(audioData);

    ambientAudio->audioSource1.fillBuffer(outputData,numFrames);

    return DataCallbackResult::Continue;
}
