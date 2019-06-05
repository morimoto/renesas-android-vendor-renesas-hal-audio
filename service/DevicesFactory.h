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

#ifndef ANDROID_HARDWARE_AUDIO_V5_0_DEVICESFACTORY_H_
#define ANDROID_HARDWARE_AUDIO_V5_0_DEVICESFACTORY_H_

#include <android/hardware/audio/5.0/IDevicesFactory.h>
#include <hidl/Status.h>
#include <hidl/MQDescriptor.h>

#include <hardware/audio.h>

namespace android {
namespace hardware {
namespace audio {
namespace V5_0 {
namespace renesas {

using ::android::hardware::audio::V5_0::IDevice;
using ::android::hardware::audio::V5_0::IDevicesFactory;
using ::android::hardware::audio::V5_0::Result;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::sp;

//class Device;

struct DevicesFactory : public IDevicesFactory {
    Return<void> openDevice(const hidl_string& device, openDevice_cb _hidl_cb) override;
    Return<void> openPrimaryDevice(openPrimaryDevice_cb _hidl_cb) override;

   private:
    template <class DeviceShim, class Callback>
    Return<void> openDevice(const char* moduleName, Callback _hidl_cb);
    Return<void> openDevice(const char* moduleName, openDevice_cb _hidl_cb);

    static int loadAudioInterface(const char* if_name, audio_hw_device_t** dev);
};

extern "C" IDevicesFactory* HIDL_FETCH_IDevicesFactory(const char* name);

}  // namespace renesas
}  // namespace V5_0
}  // namespace audio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_AUDIO_V5_0_DEVICESFACTORY_H
