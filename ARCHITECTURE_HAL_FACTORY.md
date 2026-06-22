# PowerManager HAL Factory Architecture

## Overview

This document describes the dual-backend HAL architecture introduced for PowerManager, where a factory pattern selects and instantiates either:

- RDKV HAL backend
- AIDL HAL backend

The design keeps controller logic backend-agnostic and centralizes backend selection, fallback, and creation in one place.

## Goals

- Support multiple HAL families without changing controller behavior
- Keep backend-specific code isolated in hal classes
- Allow runtime backend selection with safe fallback
- Preserve current RDKV behavior as default

## Key Components

- PowerManagerImplementation
  - Owns the controller layer and plugin lifecycle
- DeepSleepController
  - Uses hal::deepsleep::IPlatform only
- PowerController
  - Uses hal::power::IPlatform only
- PowerManagerFactory
  - Creates backend-specific platform implementations
  - Handles backend selection and fallback
- Backend implementations
  - RDKV: DeepSleepImpl, PowerImpl
  - AIDL: DeepSleepAidlImpl, PowerAidlImpl

## Class Diagram

```mermaid
classDiagram
    direction LR

    class PowerManagerImplementation {
        -DeepSleepController _deepSleepController
        -PowerController _powerController
    }

    class DeepSleepController {
      -shared_ptr~IPlatformDeepSleep~ _platform
        +Create(parent) DeepSleepController
        +Activate(timeout, nwStandbyMode) uint32_t
        +Deactivate() uint32_t
        +GetLastWakeupReason(reason) uint32_t
    }

    class PowerController {
      -unique_ptr~IPlatformPower~ _platform
        +Create(deepSleep) PowerController
        +SetPowerState(keyCode, state, reason) uint32_t
        +GetPowerState(current, prev) uint32_t
        +SetWakeupSourceConfig(configs) uint32_t
    }

    class PowerManagerFactory {
      +CreateDeepSleepPlatform() shared_ptr~IPlatformDeepSleep~
      +CreatePowerPlatform() unique_ptr~IPlatformPower~
    }

    class IPlatformDeepSleep["hal::deepsleep::IPlatform"] {
        <<interface>>
        +SetDeepSleep(timeout, isGPIOWakeup, networkStandby) uint32_t
        +DeepSleepWakeup() uint32_t
        +GetLastWakeupReason(reason) uint32_t
        +GetLastWakeupKeyCode(keyCode) uint32_t
    }

    class IPlatformPower["hal::power::IPlatform"] {
        <<interface>>
        +SetPowerState(state) uint32_t
        +GetPowerState(state) uint32_t
        +SetWakeupSrc(type, enabled, supported) uint32_t
        +GetWakeupSrc(type, enabled, supported) uint32_t
    }

    class DeepSleepImpl
    class PowerImpl
    class DeepSleepAidlImpl
    class PowerAidlImpl

    PowerManagerImplementation --> DeepSleepController
    PowerManagerImplementation --> PowerController

    DeepSleepController --> PowerManagerFactory : Create()
    PowerController --> PowerManagerFactory : Create()

    DeepSleepController --> IPlatformDeepSleep
    PowerController --> IPlatformPower

    DeepSleepImpl ..|> IPlatformDeepSleep
    PowerImpl ..|> IPlatformPower
    DeepSleepAidlImpl ..|> IPlatformDeepSleep
    PowerAidlImpl ..|> IPlatformPower

    PowerManagerFactory --> DeepSleepImpl : RDKV path
    PowerManagerFactory --> PowerImpl : RDKV path
    PowerManagerFactory --> DeepSleepAidlImpl : AIDL path
    PowerManagerFactory --> PowerAidlImpl : AIDL path
```

## Backend Selection

Backend is selected using the following logic:

Checks the AIDL HAL's availability during runtime.
   - If AIDL HAL available -> creates AIDL backend
   - If AIDL HAL not available -> creates RDKV backend

## Runtime Flow Diagram

```mermaid
flowchart TD
    A[PowerManagerImplementation startup] --> B[Create DeepSleepController]
    B --> C[DeepSleepController Create]
    C --> D[PowerManagerFactory CreateDeepSleepPlatform]

    A --> E[Create PowerController]
    E --> F[PowerController Create]
    F --> G[PowerManagerFactory CreatePowerPlatform]

    D --> H{Selected backend}
    G --> I{Selected backend}

    H -->|AIDL requested| J[Instantiate DeepSleepAidlImpl]
    H -->|RDKV requested| K[Instantiate DeepSleepImpl]

    I -->|AIDL requested| L[Instantiate PowerAidlImpl]
    I -->|RDKV requested| M[Instantiate PowerImpl]

    J --> N{AIDL available?}
    L --> O{AIDL available?}

    N -->|Yes| P[Use AIDL DeepSleep platform]
    N -->|No| Q[Fallback to RDKV DeepSleep]

    O -->|Yes| R[Use AIDL Power platform]
    O -->|No| S[Fallback to RDKV Power]

    P --> T[Controller uses IPlatform API]
    Q --> T
    R --> U[Controller uses IPlatform API]
    S --> U

    T --> V[Deep sleep operations]
    U --> W[Power and wake source operations]
```

## Design Benefits

- Single point of backend selection logic
- Controller code remains stable and backend-agnostic
- Reduced regression risk when adding new backends
- Incremental migration path from RDKV HAL to AIDL HAL
