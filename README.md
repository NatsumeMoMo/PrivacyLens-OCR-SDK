# PrivacyLens Engine SDK

PrivacyLens Engine SDK is the local redaction-decision boundary for
PrivacyLens. It builds `PrivacyLensEngine.dll`, exposes a direct-export C ABI in
`include/pl_engine.h`, and returns source-frame mask rectangles that the
PrivacyLens main application can render over its sanitized preview.

The SDK no longer treats full-screen OCR as the public product boundary. OCR
code from the earlier spike remains in the repository for future provider work,
but the current build surface is the Engine SDK:

```text
runtime policy + frame context
  -> provider dispatch
  -> latest mask list
```

## Current Status

- Spike Result: direct-export C ABI is present in `include/pl_engine.h`.
- Spike Result: `PrivacyLensEngine.dll` builds with CMake / MSVC / x64.
- Spike Result: `pl-engine-cli --self-test --mask-wechat` creates an
  engine/session, pushes a runtime policy, submits monitor-frame context, and
  prints privacy-safe mask counts.
- Spike Result: `AppWindowProvider` can detect visible WeChat / Weixin
  top-level window fragments through Win32/DWM geometry and z-order occlusion.
- Deferred: OCR provider, YOLO provider, manual-region provider, policy files,
  GPU frame input, and main-project rendering integration.

## Build

Preferred local baseline:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-engine -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-engine --config Debug
```

Expected Debug outputs:

```text
build-engine/Debug/PrivacyLensEngine.dll
build-engine/Debug/PrivacyLensEngine.lib
build-engine/Debug/pl-engine-cli.exe
```

## Smoke Test

```powershell
.\build-engine\Debug\pl-engine-cli.exe --self-test --mask-wechat
```

Expected behavior:

- Creates an engine.
- Creates a session.
- Pushes a runtime policy with WeChat masking enabled.
- Submits a monitor-frame context.
- Returns the latest mask list.
- Prints only SDK status, policy generation, option state, and mask count.

The mask count depends on the current desktop. If no visible WeChat / Weixin
window is present on the primary monitor, `masks=0` is valid.

## Public ABI

The public header is:

```text
include/pl_engine.h
```

The first direct-export functions are:

```c
pl_engine_create
pl_engine_destroy
pl_engine_session_create
pl_engine_session_destroy
pl_engine_session_update_policy
pl_engine_session_submit_frame
pl_engine_session_get_latest_masks
pl_engine_mask_list_destroy
pl_engine_get_last_error
pl_engine_status_to_string
```

The public ABI uses only C-compatible scalar types, pointers, plain structs,
opaque handles, and exported C functions. It does not expose Qt types, STL
containers, C++ classes, C++ exceptions, ONNX Runtime, PaddleOCR, or Win32 C++
wrappers.

## Runtime Policy Boundary

The PrivacyLens main application owns user settings and persistent
configuration. The SDK does not read user config files. Runtime truth is the
latest `pl_engine_policy` pushed through:

```c
pl_engine_session_update_policy(session, &policy);
```

The MVP policy contains one active option:

```text
mask_app_wechat
```

Future options can enable OCR, YOLO, manual-region, QR, face, or other provider
families without changing the main app's ownership of product settings.

## WeChat App-Window Provider

The current provider implements the geometry approach tested in
`D:\Atlas\Labs\WgcQtWechatBox`:

```text
EnumWindows
  -> filter visible, non-minimized, non-cloaked top-level windows
  -> match process names: wechat.exe / weixin.exe
  -> get visual bounds through DWM extended frame bounds
  -> subtract higher z-order occluders
  -> intersect with captured monitor
  -> map visible fragments into source-frame coordinates
  -> emit app_wechat masks
```

This provider does not inspect pixels and does not run OCR. It returns zero
masks when WeChat is closed, minimized, fully covered, outside the captured
monitor, or disabled by policy.

## Tests

```powershell
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-engine -C Debug --output-on-failure
```

Current tests cover:

- visible-region rectangle subtraction;
- monitor-to-frame coordinate mapping;
- policy/session C ABI behavior;
- SDK mask-list ownership;
- WeChat provider mask materialization from visible fragments;
- CLI self-test.

## Repository Boundary

This repository owns the Engine SDK and provider implementations. The
PrivacyLens main application remains responsible for Windows Graphics Capture,
preview rendering, UI settings, OBS-facing sanitized output, and persistent
user configuration.

Heavy models, runtime packages, generated binaries, real screenshots, and raw
OCR output must stay out of Git and under Atlas Artifacts when needed.
