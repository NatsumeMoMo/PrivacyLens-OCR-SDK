# PrivacyLens-OCR-SDK

PrivacyLens-OCR-SDK is an OCR-only Windows SDK boundary for PrivacyLens. It
builds a small DLL with a stable C ABI, a stub OCR backend, and a real-backend
spike behind the same ABI. It does not commit RapidOCR, ONNX Runtime, OpenCV,
real OCR models, real screenshots, or large runtime binaries to Git.

## Status

- Spike Result: CMake project skeleton, C ABI header, DLL target, stub backend,
  and CLI self-test are present.
- Spike Result: `rapidocr_onnx` backend selection, model-dir validation, dynamic
  RapidOcrOnnx loading, WIC image decode in the CLI, and unconfigured-state
  tests are present.
- Spike Result: RapidOcrOnnx 1.2.2 `windows-clib` loads as an artifact, but its
  prebuilt DLL does not export the bbox-capable `OcrDetectInput` /
  `OcrFreeResult` C API found in current source, so configured real OCR returns
  `PL_OCR_STATUS_BACKEND_UNAVAILABLE` instead of silently dropping bbox data.
- Proposed: PrivacyLens will load the SDK DLL at runtime instead of linking OCR
  inference code into the main application.
- Deferred: Build a bbox-capable RapidOcrOnnx C DLL from source or switch to a
  different OCR runtime adapter before claiming real OCR boxes are available.

## Build

Preferred local baseline:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
```

Known local CMake path if `cmake` is not on `PATH`:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build --config Debug
```

Expected Debug outputs:

```text
build/Debug/PrivacyLensOcr.dll
build/Debug/PrivacyLensOcr.lib
build/Debug/pl-ocr-cli.exe
```

## Smoke Test

```powershell
.\build\Debug\pl-ocr-cli.exe --self-test
.\build\Debug\pl-ocr-cli.exe --backend stub --self-test
```

Expected behavior:

- Gets the v1 function table through `pl_ocr_get_api`.
- Creates a stub context.
- Creates an in-memory BGRA8 test image.
- Runs OCR through the DLL boundary.
- Prints backend info, box count, latency, and fake OCR text.

Real-backend CLI shape:

```powershell
.\build\Debug\pl-ocr-cli.exe --backend rapidocr_onnx --model-dir <path> --image <path> [--print-text]
.\build\Debug\pl-ocr-cli.exe --backend rapidocr_onnx --model-dir <path> --self-test [--print-text]
```

`--image` uses Windows WIC and does not require OpenCV. OCR text from image
files is hidden by default; pass `--print-text` only for explicit local smoke
tests with safe synthetic inputs.

## Public ABI

The public header is `include/pl_ocr.h`.

The only exported entry point is:

```c
pl_ocr_status pl_ocr_get_api(uint32_t requested_version, pl_ocr_api_v1* out_api);
```

The v1 function table includes:

- `get_abi_version`
- `get_sdk_version`
- `create_context`
- `destroy_context`
- `get_backend_info`
- `recognize_image`
- `destroy_result`
- `get_last_error`
- `status_to_string`

The public ABI uses only C-compatible scalar types, pointers, plain structs, and
function pointers. It does not expose Qt types, STL containers, C++ classes, C++
exceptions, or memory that callers must release through their own CRT.

## Memory And Lifetime Rules

- Caller owns `pl_ocr_image.data`; it only needs to remain valid for the
  duration of `recognize_image`.
- SDK allocates `pl_ocr_context`; caller releases it with `destroy_context`.
- SDK allocates `pl_ocr_result`; caller releases it with `destroy_result`.
- Result strings and `boxes` are valid until `destroy_result`.
- Backend info strings are owned by the SDK and must not be freed by caller.
- Last-error strings are valid until the next SDK call on the same context, or
  until the context is destroyed.
- If `pl_ocr_get_api` receives an unsupported ABI version, it clears `out_api`
  and returns `PL_OCR_STATUS_UNSUPPORTED_ABI`.

## Stub Backend

The current backend name is `stub`, with model version `stub-v1`. For any valid
BGRA8 image it returns three fixed fake OCR boxes:

- `sk-demo-REDACTED-1234`
- `demo@example.com`
- `+1 555 0100`

These strings are synthetic test data. They are not real credentials or real
personal information.

## RapidOCR ONNX Spike

The optional backend name is `rapidocr_onnx` (alias: `rapidocr`). Select it via
`pl_ocr_context_options.requested_backend_utf8` and pass the model directory via
`pl_ocr_context_options.model_dir_utf8`.

Expected model directory:

```text
ch_PP-OCRv3_det_infer.onnx
ch_ppocr_mobile_v2.0_cls_infer.onnx
ch_PP-OCRv3_rec_infer.onnx
ppocr_keys_v1.txt
```

Runtime lookup order:

1. `PRIVACYLENS_OCR_RAPIDOCRONNX_DIR`
2. `<model_dir>\RapidOcrOnnx.dll` or `<model_dir>\bin\RapidOcrOnnx.dll`
3. Atlas artifact default path under `D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\...`

Current Spike Result: the SDK adapter requires the bbox-capable
`OcrDetectInput` API. The downloaded RapidOcrOnnx 1.2.2 prebuilt DLL exports
only the older file-path API, so it is treated as backend unavailable.

## Repository Boundary

This repository is for OCR SDK work only. It should not contain PrivacyLens UI,
capture, masking, policy, OBS integration, or general inference SDK code. The
PrivacyLens main project should later consume the SDK through runtime DLL
loading.

See:

- `docs/architecture.md`
- `docs/dynamic-loading.md`
- `docs/artifact-policy.md`
- `docs/backend-roadmap.md`
- `docs/real-backend-spike.md`
- `docs/artifact-manifest-rapidocr-onnx.md`
