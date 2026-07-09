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

#include <cstdint>
#include <optional>
#include <vector>

#ifdef LOG_PRI
#undef LOG_PRI
#endif

#include <core/Portability.h>
#include <interfaces/IPowerManager.h>

#include "DeepSleep.h"
#include "UtilsLogging.h"

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <utils/StrongPointer.h>

#include <com/rdk/hal/deepsleep/IDeepSleep.h>
#include <com/rdk/hal/deepsleep/KeyCode.h>
#include <com/rdk/hal/deepsleep/WakeUpTrigger.h>

class DeepSleepAidlImpl : public hal::deepsleep::IPlatform {
    using WakeupReason = WPEFramework::Exchange::IPowerManager::WakeupReason;

public:
    DeepSleepAidlImpl()
        : _available(false)
        , _lastWakeupReason(WakeupReason::WAKEUP_REASON_UNKNOWN)
        , _lastWakeupKeyCode(0)
    {
        try {
            android::ProcessState::self()->startThreadPool();
            android::sp<android::IBinder> binderSvc = android::defaultServiceManager()->getService(
                android::String16(com::rdk::hal::deepsleep::IDeepSleep::serviceName().c_str()));

            if (binderSvc == nullptr) {
                LOGERR("Unable to get AIDL DeepSleep service");
                return;
            }

            _deepsleep = android::interface_cast<com::rdk::hal::deepsleep::IDeepSleep>(binderSvc);
            _available = (_deepsleep != nullptr);

            if (_available) {
                LOGINFO("AIDL DeepSleep service acquired");
            } else {
                LOGERR("Unable to cast DeepSleep service binder");
            }
        } catch (...) {
            LOGERR("Exception caught while initializing AIDL DeepSleep service");
            _available = false;
        }
    }

    bool IsAvailable() const
    {
        return _available;
    }

    uint32_t SetDeepSleep(uint32_t deepSleepTime, bool& isGPIOWakeup, bool networkStandby) override
    {
        if (!_available || _deepsleep == nullptr) {
            return WPEFramework::Core::ERROR_UNAVAILABLE;
        }

        bool result = false;
        android::binder::Status st = _deepsleep->setWakeUpTimer(static_cast<int32_t>(deepSleepTime), &result);
        if (!st.isOk() || !result) {
            LOGERR("IDeepSleep::setWakeUpTimer failed");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        std::vector<com::rdk::hal::deepsleep::WakeUpTrigger> triggers(_wakeupTriggers);
        triggers.push_back(com::rdk::hal::deepsleep::WakeUpTrigger::TIMER);

        if (networkStandby) {
            if (std::find(triggers.begin(), triggers.end(), com::rdk::hal::deepsleep::WakeUpTrigger::LAN) == triggers.end()) {
                triggers.push_back(com::rdk::hal::deepsleep::WakeUpTrigger::LAN);
            }
            if (std::find(triggers.begin(), triggers.end(), com::rdk::hal::deepsleep::WakeUpTrigger::WLAN) == triggers.end()) {
                triggers.push_back(com::rdk::hal::deepsleep::WakeUpTrigger::WLAN);
            }
        }

        std::vector<com::rdk::hal::deepsleep::WakeUpTrigger> wokeUpByTriggers;
        std::optional<com::rdk::hal::deepsleep::KeyCode> keyCode;
        bool success = false;

        st = _deepsleep->enterDeepSleep(triggers, &wokeUpByTriggers, &keyCode, &success);
        if (!st.isOk()) {
            LOGERR("IDeepSleep::enterDeepSleep failed");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        if (!success) {
            LOGERR("IDeepSleep::enterDeepSleep reported failure");
            return WPEFramework::Core::ERROR_ABORTED;
        }

        _lastWakeupReason = ConvertWakeupReason(wokeUpByTriggers);
        _lastWakeupKeyCode = keyCode.has_value() ? keyCode.value().keyCode : 0;

        // GPIO is not an explicit trigger in the current DeepSleep AIDL contract.
        isGPIOWakeup = (_lastWakeupReason == WakeupReason::WAKEUP_REASON_GPIO);

        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t DeepSleepWakeup(void) override
    {
        // AIDL enterDeepSleep call is blocking and returns on wakeup.
        // There is no explicit wakeup API, so this is a no-op.
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetLastWakeupReason(WakeupReason& wakeupReason) const override
    {
        wakeupReason = _lastWakeupReason;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetLastWakeupKeyCode(int& wakeupKeyCode) const override
    {
        wakeupKeyCode = _lastWakeupKeyCode;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetWakeupSrc(WakeupSrcType wakeSrcType, bool enabled) override
    {
        if (!_available || _deepsleep == nullptr) {
            return WPEFramework::Core::ERROR_UNAVAILABLE;
        }

        com::rdk::hal::deepsleep::WakeUpTrigger trigger;

        switch (wakeSrcType) {
        case WakeupSrcType::WAKEUPSRC_IR:
            trigger = com::rdk::hal::deepsleep::WakeUpTrigger::RCU_IR;
            break;
        case WakeupSrcType::WAKEUPSRC_RCU_BT:
            trigger = com::rdk::hal::deepsleep::WakeUpTrigger::RCU_BT;
            break;
        case WakeupSrcType::WAKEUPSRC_RCU_RF4CE:
            trigger = com::rdk::hal::deepsleep::WakeUpTrigger::RCU_RF4CE;
            break;
        case WakeupSrcType::WAKEUPSRC_GPIO:
            trigger = com::rdk::hal::deepsleep::WakeUpTrigger::GPIO;
            break;
        case WakeupSrcType::WAKEUPSRC_LAN:
            trigger = com::rdk::hal::deepsleep::WakeUpTrigger::LAN;
            break;
        case WakeupSrcType::WAKEUPSRC_WLAN:
            trigger = com::rdk::hal::deepsleep::WakeUpTrigger::WLAN;
            break;
        default:
            LOGERR("Unknown wakeup source type: %d", wakeSrcType);
            return WPEFramework::Core::ERROR_INVALID_PARAMETER;
        }

        if (enabled) {
            if (std::find(_wakeupTriggers.begin(), _wakeupTriggers.end(), trigger) == _wakeupTriggers.end()) {
                _wakeupTriggers.push_back(trigger);
            }
        } else {
            _wakeupTriggers.erase(std::remove(_wakeupTriggers.begin(), _wakeupTriggers.end(), trigger), _wakeupTriggers.end());
        }
        return WPEFramework::Core::ERROR_NONE;
    }

private:
    static WakeupReason ConvertTrigger(com::rdk::hal::deepsleep::WakeUpTrigger trigger)
    {
        using Trigger = com::rdk::hal::deepsleep::WakeUpTrigger;

        switch (trigger) {
        case Trigger::RCU_IR:
            return WakeupReason::WAKEUP_REASON_IR;
        case Trigger::RCU_BT:
            return WakeupReason::WAKEUP_REASON_BLUETOOTH;
        case Trigger::RCU_RF4CE:
            return WakeupReason::WAKEUP_REASON_RF4CE;
        case Trigger::LAN:
            return WakeupReason::WAKEUP_REASON_LAN;
        case Trigger::WLAN:
            return WakeupReason::WAKEUP_REASON_WIFI;
        case Trigger::TIMER:
            return WakeupReason::WAKEUP_REASON_TIMER;
        case Trigger::FRONT_PANEL:
            return WakeupReason::WAKEUP_REASON_FRONTPANEL;
        case Trigger::CEC:
            return WakeupReason::WAKEUP_REASON_CEC;
        case Trigger::PRESENCE:
            return WakeupReason::WAKEUP_REASON_PRESENCE;
        case Trigger::VOICE:
            return WakeupReason::WAKEUP_REASON_VOICE;
        case Trigger::ERROR_UNKNOWN:
        default:
            return WakeupReason::WAKEUP_REASON_UNKNOWN;
        }
    }

    static WakeupReason ConvertWakeupReason(const std::vector<com::rdk::hal::deepsleep::WakeUpTrigger>& wokeUpByTriggers)
    {
        if (wokeUpByTriggers.empty()) {
            return WakeupReason::WAKEUP_REASON_UNKNOWN;
        }

        return ConvertTrigger(wokeUpByTriggers.front());
    }

private:
    bool _available;
    WakeupReason _lastWakeupReason;
    int _lastWakeupKeyCode;
    std::vector<com::rdk::hal::deepsleep::WakeUpTrigger> _wakeupTriggers;

    android::sp<com::rdk::hal::deepsleep::IDeepSleep> _deepsleep;
};
