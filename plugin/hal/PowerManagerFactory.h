/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <string>

#include <core/Portability.h>

#include "DeepSleep.h"
#include "Power.h"
#include "UtilsLogging.h"

#ifdef POWERMANAGER_ENABLE_RDKV_HAL
#include "DeepSleepImpl.h"
#include "PowerImpl.h"
#endif

#ifdef POWERMANAGER_ENABLE_AIDL_HAL
#include "DeepSleepAidlImpl.h"
#include "PowerAidlImpl.h"
#endif

class PowerManagerFactory {
public:
    static std::shared_ptr<hal::deepsleep::IPlatform> CreateDeepSleepPlatform()
    {
        if (isAidlServiceAvailable()) {
            return std::make_shared<DeepSleepAidlImpl>();
        }

        LOGINFO("Using RDKV backend for DeepSleep HAL");
        return std::make_shared<DeepSleepImpl>();
    }

    static std::unique_ptr<hal::power::IPlatform> CreatePowerPlatform()
    {
        if (isAidlServiceAvailable()) {
            return std::unique_ptr<PowerAidlImpl>(new PowerAidlImpl());
        }
        LOGINFO("Using RDKV backend for Power HAL");
        return std::unique_ptr<hal::power::IPlatform>(new PowerImpl());
    }

private:
    static bool isAidlServiceAvailable();
    {
        const char* value = std::getenv("POWERMANAGER_HAL_BACKEND");
        if (value != nullptr) {
            return true;
        }
        return false;
    }
};
