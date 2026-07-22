"""
/**
 * @file TCID051_ExternallyTriggeredReboot.py
 * @brief L2 PowerManager combination testcase.
 *
 * @testcase TCID051_ExternallyTriggeredReboot
 * @details Validates network wake preconditions and records the current partial
 *          limitation around missing external wake stimulus in this framework.
 */
"""

import os
import time

from utils import POWERMANAGER_CMD_BASE, send_curl_command, send_vcomponent_command, is_ok, log_success, log_error, log_warning
import PowerManager_Curl as PowerManagerApis
from PowerManager_CombinationHelpers import parse_last_wakeup_reason, parse_power_state


def _post_reboot(yaml_file):
    http_code, body = send_vcomponent_command(f"{POWERMANAGER_CMD_BASE}/{yaml_file}", False)
    log_warning(f"vComponent POST {yaml_file}: HTTP {http_code}  {body}")
    return http_code == 200


def _wait_for_boot_reason(expected_reason, timeout_seconds=20):
    deadline = time.time() + timeout_seconds
    last_reason = None
    while time.time() < deadline:
        reason_resp = send_curl_command(PowerManagerApis.get_last_wakeup_reason)
        log_warning(f"Boot reason response: {reason_resp}")
        last_reason = parse_last_wakeup_reason(reason_resp)
        if last_reason == expected_reason:
            return last_reason
        time.sleep(1)
    return last_reason


def _wait_for_awake_state(timeout_seconds=20):
    deadline = time.time() + timeout_seconds
    last_state = None
    while time.time() < deadline:
        state_resp = send_curl_command(PowerManagerApis.get_power_state)
        log_warning(f"Power state response: {state_resp}")
        last_state = parse_power_state(state_resp)
        if isinstance(last_state, dict) and last_state.get("currentState") != "DEEP_SLEEP":
            return last_state
        time.sleep(1)
    return last_state


def run_test():
    start_time = time.perf_counter()

    if not _post_reboot("Boot_BootReason_COLD_BOOTeup_LAN.yaml"):
        log_error("TCID051_ExternallyTriggeredReboot Failed ❌ (failed to post COLD BOOT simulation)")
        return False

    state = _wait_for_awake_state()
    if not isinstance(state, dict):
        log_error("TCID051_ExternallyTriggeredReboot Failed ❌ (device did not report a post-wake power state)")
        return False

    reason = _wait_for_boot_reason("COLD_BOOT")
    if reason != "LAN":
        log_error("TCID051_ExternallyTriggeredReboot Failed ❌ (last boot reason was not LAN)")
        return False

    elapsed_time = time.perf_counter() - start_time
    msg = "TCID051_ExternallyTriggeredReboot Passed âœ…"
    if os.environ.get("POWERMANAGER_TIMING_ENABLED"):
        log_success(f"{msg} time consumed: {elapsed_time:.3f}s")
    else:
        log_success(msg)
    return True

