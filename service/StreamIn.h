/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_HARDWARE_AUDIO_V5_0_STREAMIN_H
#define ANDROID_HARDWARE_AUDIO_V5_0_STREAMIN_H

#include "Device.h"
#include "Stream.h"

#include <android/hardware/audio/5.0/IStreamIn.h>

#include <hidl/MQDescriptor.h>
#include <fmq/EventFlag.h>
#include <fmq/MessageQueue.h>
#include <hidl/Status.h>
#include <utils/Thread.h>

#include <atomic>
#include <memory>

namespace android {
namespace hardware {
namespace audio {
namespace V5_0 {
namespace renesas {

using ::android::hardware::audio::common::V5_0::AudioChannelMask;
using ::android::hardware::audio::common::V5_0::AudioDevice;
using ::android::hardware::audio::common::V5_0::AudioFormat;
using ::android::hardware::audio::common::V5_0::AudioSource;
using ::android::hardware::audio::common::V5_0::DeviceAddress;
using ::android::hardware::audio::V5_0::IStream;
using ::android::hardware::audio::V5_0::IStreamIn;
using ::android::hardware::audio::V5_0::ParameterValue;
using ::android::hardware::audio::V5_0::Result;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::sp;


struct StreamIn : public IStreamIn {
    typedef MessageQueue<ReadParameters, kSynchronizedReadWrite> CommandMQ;
    typedef MessageQueue<uint8_t, kSynchronizedReadWrite> DataMQ;
    typedef MessageQueue<ReadStatus, kSynchronizedReadWrite> StatusMQ;

    StreamIn(const sp<Device>& device, audio_stream_in_t* stream);

    // Methods from ::android::hardware::audio::AUDIO_HAL_VERSION::IStream follow.
    Return<uint64_t> getFrameSize() override;
    Return<uint64_t> getFrameCount() override;
    Return<uint64_t> getBufferSize() override;
    Return<uint32_t> getSampleRate() override;
    Return<void> getSupportedSampleRates(AudioFormat format, getSupportedSampleRates_cb _hidl_cb);
    Return<void> getSupportedChannelMasks(AudioFormat format, getSupportedChannelMasks_cb _hidl_cb);
    Return<Result> setSampleRate(uint32_t sampleRateHz) override;
    Return<AudioChannelBitfield> getChannelMask() override;
    Return<Result> setChannelMask(AudioChannelBitfield mask) override;
    Return<AudioFormat> getFormat() override;
    Return<void> getSupportedFormats(getSupportedFormats_cb _hidl_cb) override;
    Return<Result> setFormat(AudioFormat format) override;
    Return<void> getAudioProperties(getAudioProperties_cb _hidl_cb) override;
    Return<Result> addEffect(uint64_t effectId) override;
    Return<Result> removeEffect(uint64_t effectId) override;
    Return<Result> standby() override;
    Return<void> getDevices(getDevices_cb _hidl_cb) override;
    Return<Result> setDevices(const hidl_vec<DeviceAddress>& devices) override;
    Return<void> getParameters(const hidl_vec<ParameterValue>& context,
                               const hidl_vec<hidl_string>& keys,
                               getParameters_cb _hidl_cb) override;
    Return<Result> setParameters(const hidl_vec<ParameterValue>& context,
                                 const hidl_vec<ParameterValue>& parameters) override;
    Return<Result> setHwAvSync(uint32_t hwAvSync) override;
    Return<Result> close() override;

    Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& options) override;

    // Methods from ::android::hardware::audio::AUDIO_HAL_VERSION::IStreamIn follow.
    Return<void> getAudioSource(getAudioSource_cb _hidl_cb) override;
    Return<Result> setGain(float gain) override;
    Return<void> prepareForReading(uint32_t frameSize, uint32_t framesCount,
                                   prepareForReading_cb _hidl_cb) override;
    Return<uint32_t> getInputFramesLost() override;
    Return<void> getCapturePosition(getCapturePosition_cb _hidl_cb) override;
    Return<Result> start() override;
    Return<Result> stop() override;
    Return<void> createMmapBuffer(int32_t minSizeFrames, createMmapBuffer_cb _hidl_cb) override;
    Return<void> getMmapPosition(getMmapPosition_cb _hidl_cb) override;
    Return<void> updateSinkMetadata(const SinkMetadata& sinkMetadata) override;
    Return<void> getActiveMicrophones(getActiveMicrophones_cb _hidl_cb) override;
    Return<Result> setMicrophoneDirection(MicrophoneDirection direction) override;
    Return<Result> setMicrophoneFieldDimension(float zoom) override;
    static Result getCapturePositionImpl(audio_stream_in_t* stream, uint64_t* frames,
                                         uint64_t* time);

   private:
    bool mIsClosed;
    const sp<Device> mDevice;
    audio_stream_in_t* mStream;
    const sp<Stream> mStreamCommon;
    const sp<StreamMmap<audio_stream_in_t>> mStreamMmap;
    std::unique_ptr<CommandMQ> mCommandMQ;
    std::unique_ptr<DataMQ> mDataMQ;
    std::unique_ptr<StatusMQ> mStatusMQ;
    EventFlag* mEfGroup;
    std::atomic<bool> mStopReadThread;
    sp<Thread> mReadThread;

    virtual ~StreamIn();
};

}  // namespace renesas
}  // namespace V5_0
}  // namespace audio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_AUDIO_V5_0_STREAMIN_H
