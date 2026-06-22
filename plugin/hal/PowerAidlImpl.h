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

#include <map>

#include <core/Portability.h>
#include <interfaces/IPowerManager.h>

#include "Power.h"
#include "UtilsLogging.h"

class PowerAidlImpl : public hal::power::IPlatform {
    using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;
    using WakeupSrcType = WPEFramework::Exchange::IPowerManager::WakeupSrcType;

public:
    PowerAidlImpl()
        : _available(false)
        , _powerState(PowerState::POWER_STATE_ON)
    {
#ifdef POWERMANAGER_ENABLE_AIDL_HAL
        // AIDL backend is enabled at build time.
        // Concrete service acquisition and API mapping is handled in backend-specific
        // method integrations as those interfaces become available on target devices.
        _available = true;
#endif
    }

    bool IsAvailable() const
    {
        return _available;
    }

    uint32_t SetPowerState(PowerState newState) override
    {
        _powerState = newState;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetPowerState(PowerState& state) override
    {
        state = _powerState;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetWakeupSrc(WakeupSrcType wakeSrcType, bool enabled, bool& supported) override
    {
        supported = true;
        _wakeupSources[wakeSrcType] = enabled;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetWakeupSrc(WakeupSrcType wakeSrcType, bool& enabled, bool& supported) const override
    {
        supported = true;
        auto it = _wakeupSources.find(wakeSrcType);
        enabled = (it != _wakeupSources.end()) ? it->second : false;
        return WPEFramework::Core::ERROR_NONE;
    }

private:
    bool _available;
    PowerState _powerState;
    std::map<WakeupSrcType, bool> _wakeupSources;
};
