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

#define LOG_TAG "audiohalservice"

#include "DevicesFactory.h"

#include <utils/StrongPointer.h>
#include <hidl/HidlTransportSupport.h>
#include <android-base/logging.h>

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

using android::hardware::audio::V5_0::IDevicesFactory;
using android::hardware::audio::V5_0::IDevice;
using namespace android::hardware::audio::V5_0::renesas;
using namespace android;

using android::OK;

int main() {
    android::sp<IDevicesFactory> service1 = new DevicesFactory();
    configureRpcThreadpool(16, true /*callerWillJoin*/);
    status_t status = service1->registerAsService();
    CHECK_EQ(status, OK) << "Could not register service DevicesFactory: " << status;

    ALOGD("%s is ready.", "DevicesFactory");
    joinRpcThreadpool();

    // return with error if we get here
    return 2;
}
