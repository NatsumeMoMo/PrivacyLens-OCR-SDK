# Provider Roadmap

## Current Provider

Spike Result: `app_window`

- No model files.
- No third-party runtime dependency.
- Uses Win32/DWM geometry and z-order.
- Matches WeChat / Weixin process names.
- Returns source-frame mask rectangles with category `app_wechat`.
- Emits zero masks when the policy is disabled or no visible matching window is
  present.

## Current Public Flow

The public Engine SDK flow is:

```text
pl_engine_session_update_policy
pl_engine_session_submit_frame
pl_engine_session_get_latest_masks
```

The SDK does not decide product-level categories by reading a config file. The
PrivacyLens main app owns settings and pushes a runtime policy snapshot.

## Next Provider Questions

Recommended next provider families:

1. `app_window`: QQ and other known app classes using the same geometry path.
2. `manual_region`: user-defined rectangles from the main app.
3. `ocr_text`: OCR-backed secret or PII text regions.
4. `object_detector`: YOLO or another detector for categories that cannot be
   represented as known app windows.
5. `template`: stable UI patterns for specific workflows.

Each provider should produce masks behind the same session/policy boundary.
Provider-specific dependencies must stay private to the SDK implementation.

## OCR Spike History

The earlier OCR work remains useful as research history:

- `stub` validated the first SDK ABI and CLI path.
- `rapidocr_onnx` showed that the available prebuilt DLL lacked the required
  memory-input bbox exports.
- `paddleocr_onnx` validated a direct ONNX Runtime path on synthetic and local
  screenshot tests.

Those components were removed from the active SDK source tree during the Engine
SDK architecture cleanup. They should be reintroduced only through an Engine
provider/backend interface when OCR masks become an active product option again.

Do not restore the old `pl_ocr` ABI as a compatibility layer.

## Deferred Capabilities

- Provider scoring and priority arbitration.
- Multiple mask styles.
- Standalone benchmark runner.
- GPU frame input.
- Artifact-gated provider tests.
- Model package signing or checksum enforcement.
- Main-project async worker integration.

## Boundary Rules

Providers may use C++ internally, but the DLL boundary must remain C ABI. Do not
expose provider classes, ONNX Runtime objects, detector objects, STL containers,
Qt objects, Win32 wrapper classes, or exceptions to callers.
