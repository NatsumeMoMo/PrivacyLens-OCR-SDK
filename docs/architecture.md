# Engine SDK Architecture

## Purpose

Define the current SDK boundary after the OCR-only spike was refactored into a
redaction Engine SDK.

## Current Goal

The SDK receives runtime policy and frame context from the PrivacyLens main
application, then returns source-frame mask rectangles. The first provider is a
Win32/DWM WeChat app-window provider.

```text
PrivacyLens main app
  owns UI, settings, capture, preview, render
  -> pl_engine_session_update_policy
  -> pl_engine_session_submit_frame
  -> pl_engine_session_get_latest_masks
PrivacyLensEngine.dll
  owns provider dispatch and mask output
```

## Non-Goals

- No Windows Graphics Capture ownership inside the SDK.
- No Qt UI or Qt data types in the SDK ABI.
- No OBS, virtual camera, or preview renderer inside the SDK.
- No direct reading of the user's settings file by the SDK.
- No real screenshots, models, generated binaries, or raw OCR text in Git.

## Public ABI

The current public header is `include/pl_engine.h`. The SDK exports direct C
functions instead of a single function-table getter.

The ABI keeps these rules:

- opaque handles for `pl_engine` and `pl_engine_session`;
- plain C structs with `struct_size` for extensibility;
- SDK-owned mask lists destroyed through `pl_engine_mask_list_destroy`;
- no STL, Qt, exceptions, ONNX Runtime, or C++ classes crossing the ABI.

## Runtime Policy

The main app turns user settings into a `pl_engine_policy` snapshot. The SDK
copies the snapshot and uses it on subsequent frame submissions.

The first active policy flag is:

```text
mask_app_wechat
```

The policy also carries `policy_generation`; returned mask lists copy this
generation so diagnostics can identify which settings produced the masks.

## WeChat Provider Flow

```text
submit_frame(frame, monitor context)
  -> AppWindowProvider
  -> EnumWindows
  -> match wechat.exe / weixin.exe
  -> DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS)
  -> collect higher z-order occluders
  -> subtract occluders
  -> map visible fragments to source-frame coordinates
  -> return app_wechat masks
```

The provider is geometry-derived. Its mask confidence is `1.0` because it is not
model inference. It emits reason code:

```text
app.wechat.visible_region
```

## Provider And Backend Layering

Providers represent product capabilities. Backends represent private model or
runtime implementations underneath providers that need them.

The current `app_window` provider has no backend layer because Win32/DWM
geometry is the implementation. Future OCR and YOLO work should use private
backend contracts under provider directories, for example:

```text
src/providers/ocr/
  ocr_provider.hpp
  ocr_backend.hpp
  backends/paddle_onnx/
  backends/rapidocr_onnx/

src/providers/object_detection/
  object_detection_provider.hpp
  object_detector_backend.hpp
  backends/yolo_onnx/
  backends/openvino/
```

The previous OCR-only ABI and OCR backend tree were removed during the Engine
SDK architecture cleanup. They should not be used as a compatibility layer.

## Deferred Providers

The architecture leaves room for:

- OCR provider for ROI text/secrets;
- YOLO or other region detector provider;
- manual-region provider;
- template provider;
- QR/face providers.

Those providers should produce candidates or masks behind the same session and
policy boundary. They should not move product setting ownership into the SDK.
