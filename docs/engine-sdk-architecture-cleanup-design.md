# Engine SDK Architecture Cleanup Design

Date: 2026-06-18

## Purpose

PrivacyLens-OCR-SDK has already shifted from an OCR-only spike to the
PrivacyLens Engine SDK. The current repository still contains the old OCR ABI,
OCR engine, OCR backends, OCR CLI, and main-project OCR diagnostic loader. Those
pieces no longer describe the active product boundary and create a false second
integration path.

This cleanup makes the Engine SDK the only supported public SDK surface.

## Target Boundary

```text
PrivacyLens main application
  owns UI, settings, capture, preview, rendering, OBS-facing output
  pushes runtime policy and frame context
  pulls latest masks

include/pl_engine.h
  is the only public C ABI header

src/c_api/pl_engine_api.cpp
  is a thin ABI bridge

EngineSession
  owns policy snapshots, provider dispatch, and mask-list materialization

Providers
  implement redaction capabilities such as app-window, OCR ROI, object
  detection, manual regions, templates, QR, and faces

Backends
  are private implementation choices underneath providers when a provider needs
  a model/runtime family
```

The active MVP provider remains `AppWindowProvider`, currently configured only
for WeChat / Weixin geometry masking.

## Provider And Backend Layers

Provider means product capability. It answers: "What privacy signal can this
part of the engine produce?"

Backend means implementation strategy under a provider. It answers: "Which
model, runtime, or adapter executes this provider?"

Current and future layering:

```text
src/providers/app_window/
  AppWindowProvider
  no backend layer; Win32/DWM geometry is the implementation

src/providers/ocr/
  OcrProvider
  IOcrBackend
  backends/paddle_onnx/
  backends/rapidocr_onnx/
  backends/windows_media_ocr/

src/providers/object_detection/
  ObjectDetectionProvider
  IObjectDetectorBackend
  backends/yolo_onnx/
  backends/openvino/
  backends/directml/
```

This pass does not implement OCR or YOLO providers. It removes the old OCR SDK
shape so those providers can be added later without inheriting obsolete ABI and
directory names.

## Files To Remove From SDK

Remove the old OCR public and implementation surface:

```text
include/pl_ocr.h
src/c_api/pl_ocr_api.cpp
src/core/ocr_backend.hpp
src/core/ocr_engine.cpp
src/core/ocr_engine.hpp
src/core/ocr_error.hpp
src/core/ocr_types.hpp
src/backends/stub/
src/backends/rapidocr_onnx/
src/backends/paddleocr_onnx/
tools/pl-ocr-cli/
tests/adapter_contract_tests.cpp
```

The old PaddleOCR and RapidOCR code should be treated as research history, not
as active Engine SDK code. If it is needed later, reintroduce it under provider
directories with a fresh provider/backend contract.

## Files To Keep In SDK

Keep the Engine SDK surface and WeChat app-window pipeline:

```text
include/pl_engine.h
src/c_api/pl_engine_api.cpp
src/core/engine_error.hpp
src/core/engine_session.cpp
src/core/engine_session.hpp
src/core/engine_types.hpp
src/providers/app_window/
tools/pl-engine-cli/
tests/visible_regions_tests.cpp
tests/engine_policy_tests.cpp
tests/app_window_provider_tests.cpp
CMakeLists.txt
README.md
```

## Main Project Cleanup

Remove the old PrivacyLens OCR SDK loader and UI diagnostic panel:

```text
src/ocr_sdk/
src/ui/OcrSdkPanel.cpp
src/ui/OcrSdkPanel.h
tests/ocr_sdk_loader_smoke.cpp
PrivacyLensOcrSdkLoaderSmoke target
ocr_sdk_loader_missing CTest
PRIVACYLENS_OCR_SDK_* CMake cache variables and compile definitions
PL_OCR_NO_IMPORT compile definition
```

Keep the Engine SDK loader and WeChat pipeline:

```text
src/engine_sdk/
tests/engine_sdk_loader_smoke.cpp
tests/engine_wechat_pipeline_smoke.cpp
tests/fake_wechat_window.cpp
tests/preview_widget_mask_smoke.cpp
```

Update `.spec` documents so the project control surface no longer describes the
old OCR loader as an active spike.

## Runtime Configuration Boundary

The main application owns persistent user settings and UI choices. The SDK
receives runtime snapshots through `pl_engine_session_update_policy`.

The SDK should not read the main app's settings file. For future provider or
model configuration, prefer explicit Engine/session/provider config structs or
manifest paths supplied by the main app at startup.

This pass keeps only:

```text
pl_engine_policy.mask_app_wechat
```

Additional policy flags can be added later for QQ, OCR, YOLO, manual regions,
templates, QR, or faces.

## Verification

SDK verification after cleanup:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-engine -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-engine --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-engine -C Debug --output-on-failure
.\build-engine\Debug\pl-engine-cli.exe --self-test --mask-wechat
```

Main-project verification after cleanup:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-engine-integration -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH="C:\Software\Qt\6.11.0\msvc2022_64"
& 'C:\Software\CMake\bin\cmake.exe' --build build-engine-integration --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-engine-integration -C Debug --output-on-failure
.\build-engine-integration\Debug\PrivacyLensEngineSdkLoaderSmoke.exe --expect-loaded
```

The expected main CTest set after cleanup is Engine-only:

```text
engine_sdk_loader_missing
preview_widget_external_masks
engine_wechat_pipeline_smoke
```

## Commit And Merge Policy

The SDK cleanup should be committed on `codex/engine-sdk-redaction-pipeline`.
After verification, merge that branch into SDK `main` locally and push both the
updated SDK `main` and the main PrivacyLens repository `main`.

The untracked local screenshot folder `tests/ScreenTest/` remains out of Git.
It should be ignored or moved to Atlas Artifacts if it is still needed.
