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

#ifndef ANDROID_HARDWARE_AUDIO_V5_0_DEVICE_H
#define ANDROID_HARDWARE_AUDIO_V5_0_DEVICE_H

#include "ParametersUtil.h"
#include "HidlUtils.h"
#include "Bitfields.h"
#include "HidlSupport.h"

#include <media/AudioParameter.h>
#include <hardware/audio.h>

#include <android/hardware/audio/5.0/IDevice.h>
#include <hidl/Status.h>
#include <hidl/MQDescriptor.h>

#include <memory>

namespace android {
namespace hardware {
namespace audio {
namespace V5_0 {
namespace renesas {

using ::android::hardware::audio::common::V5_0::AudioConfig;
using ::android::hardware::audio::common::V5_0::AudioHwSync;
using ::android::hardware::audio::common::V5_0::AudioInputFlag;
using ::android::hardware::audio::common::V5_0::AudioOutputFlag;
using ::android::hardware::audio::common::V5_0::AudioPatchHandle;
using ::android::hardware::audio::common::V5_0::AudioPort;
using ::android::hardware::audio::common::V5_0::AudioPortConfig;
using ::android::hardware::audio::common::V5_0::AudioSource;
using ::android::hardware::audio::common::V5_0::DeviceAddress;
using ::android::hardware::audio::common::V5_0::SourceMetadata;
using ::android::hardware::audio::common::V5_0::SinkMetadata;
using ::android::hardware::audio::common::V5_0::implementation::HidlUtils;
using ::android::hardware::audio::common::V5_0::implementation::AudioInputFlagBitfield;
using ::android::hardware::audio::common::V5_0::implementation::AudioOutputFlagBitfield;
using ::android::hardware::audio::V5_0::IDevice;
using ::android::hardware::audio::V5_0::IStreamIn;
using ::android::hardware::audio::V5_0::IStreamOut;
using ::android::hardware::audio::V5_0::ParameterValue;
using ::android::hardware::audio::V5_0::Result;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::sp;

struct Device : public IDevice, public ParametersUtil {
    explicit Device(audio_hw_device_t* device);

    // Methods from ::android::hardware::audio::AUDIO_HAL_VERSION::IDevice follow.
    Return<Result> initCheck() override;
    Return<Result> setMasterVolume(float volume) override;
    Return<void> getMasterVolume(getMasterVolume_cb _hidl_cb) override;
    Return<Result> setMicMute(bool mute) override;
    Return<void> getMicMute(getMicMute_cb _hidl_cb) override;
    Return<Result> setMasterMute(bool mute) override;
    Return<void> getMasterMute(getMasterMute_cb _hidl_cb) override;
    Return<void> getInputBufferSize(const AudioConfig& config,
                                    getInputBufferSize_cb _hidl_cb) override;

    // V2 openInputStream is called by V4 input stream thus present in both versions
    Return<void> openInputStream(int32_t ioHandle, const DeviceAddress& device,
                                 const AudioConfig& config, AudioInputFlagBitfield flags,
                                 AudioSource source, openInputStream_cb _hidl_cb);
    Return<void> openOutputStream(int32_t ioHandle, const DeviceAddress& device,
                                  const AudioConfig& config, AudioOutputFlagBitfield flags,
                                  const SourceMetadata& sourceMetadata,
                                  openOutputStream_cb _hidl_cb) override;
    Return<void> openInputStream(int32_t ioHandle, const DeviceAddress& device,
                                 const AudioConfig& config, AudioInputFlagBitfield flags,
                                 const SinkMetadata& sinkMetadata,
                                 openInputStream_cb _hidl_cb) override;

    Return<bool> supportsAudioPatches() override;
    Return<void> createAudioPatch(const hidl_vec<AudioPortConfig>& sources,
                                  const hidl_vec<AudioPortConfig>& sinks,
                                  createAudioPatch_cb _hidl_cb) override;
    Return<Result> releaseAudioPatch(int32_t patch) override;
    Return<void> getAudioPort(const AudioPort& port, getAudioPort_cb _hidl_cb) override;
    Return<Result> setAudioPortConfig(const AudioPortConfig& config) override;

    Return<Result> setScreenState(bool turnedOn) override;

    Return<void> getHwAvSync(getHwAvSync_cb _hidl_cb) override;
    Return<void> getParameters(const hidl_vec<ParameterValue>& context,
                               const hidl_vec<hidl_string>& keys,
                               getParameters_cb _hidl_cb) override;
    Return<Result> setParameters(const hidl_vec<ParameterValue>& context,
                                 const hidl_vec<ParameterValue>& parameters) override;
    Return<void> getMicrophones(getMicrophones_cb _hidl_cb) override;
    Return<Result> setConnectedState(const DeviceAddress& address, bool connected) override;

    Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& options) override;

    // Utility methods for extending interfaces.
    Result analyzeStatus(const char* funcName, int status);
    void closeInputStream(audio_stream_in_t* stream);
    void closeOutputStream(audio_stream_out_t* stream);
    audio_hw_device_t* device() const { return mDevice; }

   private:
    audio_hw_device_t* mDevice;

    virtual ~Device();

    // Methods from ParametersUtil.
    char* halGetParameters(const char* keys) override;
    int halSetParameters(const char* keysAndValues) override;

    uint32_t version() const { return mDevice->common.version; }
};

}  // namespace renesas
}  // namespace V5_0
}  // namespace audio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_AUDIO_V5_0_DEVICE_H
