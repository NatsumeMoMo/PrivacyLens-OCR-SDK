# PrivacyLens-OCR-SDK Worklog

This file records local handoff notes for the SDK workstream. It is intentionally
kept out of the SDK git history unless the repository policy changes.

## 2026-06-14 - Handoff Baseline

### Scope

- Took over `D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens-OCR-SDK`.
- No new feature implementation was started.
- Goal for this stage: confirm current state, verification status, artifacts,
  and git boundary before continuing backend work.

### Current Project State

- SDK is an OCR-only Windows DLL boundary for PrivacyLens.
- Public ABI is `include/pl_ocr.h`.
- Exported entry remains `pl_ocr_get_api`.
- `stub` backend is available and returns three synthetic boxes.
- `rapidocr_onnx` backend adapter exists behind the SDK ABI.
- Real RapidOCR inference is not working yet.

### RapidOCR Status

- Model artifacts exist under:
  `D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PP-OCRv3-rapidocr-onnx`
- Runtime artifact exists under:
  `D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\RapidOcrOnnx-1.2.2`
- Required model files were present:
  - `ch_PP-OCRv3_det_infer.onnx`
  - `ch_ppocr_mobile_v2.0_cls_infer.onnx`
  - `ch_PP-OCRv3_rec_infer.onnx`
  - `ppocr_keys_v1.txt`
- Current prebuilt `RapidOcrOnnx.dll` lacks required bbox-capable exports:
  - `OcrDetectInput`
  - `OcrFreeResult`
- Configured `rapidocr_onnx` therefore correctly fails closed with:
  `PL_OCR_STATUS_BACKEND_UNAVAILABLE`

### Verification

Commands run:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-real-ocr -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-real-ocr --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-real-ocr -C Debug --output-on-failure
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend rapidocr_onnx --model-dir D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PP-OCRv3-rapidocr-onnx --self-test --print-text --expect-status backend-unavailable
```

Results:

- CMake configure succeeded.
- Debug build succeeded.
- CTest passed: `3/3`.
- Artifact-gated rapidocr check returned:
  `RapidOcrOnnx.dll is missing required C API exports`.

### Git Boundary

- Before SDK git initialization, `git rev-parse --show-toplevel` resolved to
  `D:\Atlas`.
- The SDK directory was seen by the parent repo as an untracked directory.
- The SDK should now become an independent git repository.
- Repository policy requested by user:
  - include necessary code only;
  - include top-level `README.md`;
  - do not include other documentation in the SDK git.

### Next Technical Decision

- Route A: build a bbox-capable RapidOcrOnnx CLIB that exports
  `OcrDetectInput` and `OcrFreeResult`.
- Route B: implement a direct ONNX Runtime OCR pipeline inside this SDK.
- Do not change the public ABI unless there is a separate explicit decision.

## 2026-06-14 - Internal Adapter Pipeline Refactor

### Scope

- Wrote local design note:
  `docs/internal-adapter-pipeline-design.md`
- Refactored SDK internal adapter contract only.
- Did not integrate a real OCR model.
- Did not change `include/pl_ocr.h` or the public C ABI.
- Stub remains the verification backend for this stage.

### Architecture Decision

- Keep the top-level SDK-owned OCR contract as whole-image OCR:
  `ImageView -> OcrResult`.
- Add `OcrRequest` as the internal request envelope.
- Add `BackendCapabilities` so backend fit is explicit.
- Keep detector / recognizer / postprocessor decomposition as an optional
  internal implementation detail for future adapters, not a mandatory interface
  every backend must expose.

### Files Changed

- `CMakeLists.txt`
  - Added `pl-ocr-adapter-contract-tests`.
- `src/core/ocr_types.hpp`
  - Added `OcrRequest`.
  - Added `BackendCapabilities`.
- `src/core/ocr_backend.hpp`
  - Added `capabilities()`.
  - Changed backend recognition input from `ImageView` to `OcrRequest`.
- `src/core/ocr_engine.hpp`
  - Added `backend_capabilities()`.
- `src/core/ocr_engine.cpp`
  - Wraps `ImageView` into `OcrRequest` before dispatching to backend.
- `src/backends/stub/*`
  - Implements `BackendCapabilities`.
  - Consumes `OcrRequest`.
- `src/backends/rapidocr_onnx/*`
  - Implements intended backend capabilities.
  - Consumes `OcrRequest`.
- `tests/adapter_contract_tests.cpp`
  - Verifies stub capabilities, request path, and engine capability forwarding.

### TDD Evidence

RED command:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' --build build-real-ocr --config Debug --target pl-ocr-adapter-contract-tests
```

Expected failure observed:

```text
"capabilities": is not a member of "plocr::StubOcrBackend"
"OcrRequest": is not a member of "plocr"
"backend_capabilities": is not a member of "plocr::OcrEngine"
```

GREEN / verification commands:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' --build build-real-ocr --config Debug --target pl-ocr-adapter-contract-tests
.\build-real-ocr\Debug\pl-ocr-adapter-contract-tests.exe
& 'C:\Software\CMake\bin\cmake.exe' --build build-real-ocr --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-real-ocr -C Debug --output-on-failure
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend stub --self-test
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend rapidocr_onnx --model-dir D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PP-OCRv3-rapidocr-onnx --self-test --print-text --expect-status backend-unavailable
```

Results:

- Adapter contract test target built successfully.
- Adapter contract executable exited successfully.
- Debug build succeeded.
- CTest passed: `4/4`.
- Stub CLI still returns `status=ok boxes=3`.
- Configured RapidOcrOnnx path still returns expected backend-unavailable error:
  `RapidOcrOnnx.dll is missing required C API exports`.

### Deferred

- Real OCR runtime selection.
- Windows.Media.Ocr, PaddleOCR, RapidOCR, or direct ONNX Runtime backend
  integration.
- Public exposure of backend capabilities, if PrivacyLens later needs it.

## 2026-06-14 - PaddleOCR ONNX Backend Spike

### Scope

- Created feature branch: `codex/paddleocr-onnx-backend`.
- Followed route A: direct ONNX Runtime backend inside the SDK.
- Downloaded and side-validated official PP-OCRv6 small ONNX models.
- Integrated a first `paddleocr_onnx` SDK backend.
- Did not change `include/pl_ocr.h`.

### Artifacts

Model root:

```text
D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx
```

Files:

- `det/model.onnx`
- `det/inference.yml`
- `rec/model.onnx`
- `rec/inference.yml`
- `manifest.json`
- `tools/side_validate_ppocrv6.py`

Runtime root:

```text
D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\ONNXRuntime\onnxruntime-win-x64-1.26.0\package\onnxruntime-win-x64-1.26.0
```

### Side Validation

Command:

```powershell
& 'D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\python-venv-paddleocr-onnx\Scripts\python.exe' `
  'D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx\tools\side_validate_ppocrv6.py'
```

Result summary:

- Detector found 5 boxes on synthetic safe image.
- Recognizer returned 5 non-empty lines.
- Recognized sensitive-pattern test strings included:
  - `sk-demo-REDACTED-1234`
  - `demo@example.com`
  - `+8613800000000`
  - `postgres://user:pass@localhost/db`
- Initial confidence calculation was wrong because the recognizer output was
  already a probability distribution; fixed side script to avoid double softmax.

### SDK Integration

Files changed:

- `CMakeLists.txt`
- `README.md`
- `src/core/ocr_engine.cpp`
- `src/backends/paddleocr_onnx/paddleocr_onnx_backend.hpp`
- `src/backends/paddleocr_onnx/paddleocr_onnx_backend.cpp`

Backend names:

- `paddleocr_onnx`
- `paddleocr`

Current implementation:

- Loads ONNX Runtime through linked import library and copied runtime DLL.
- Validates `det/model.onnx`, `det/inference.yml`, `rec/model.onnx`, and
  `rec/inference.yml`.
- Runs PP-OCRv6 small detector and recognizer models on CPU.
- Uses SDK-owned connected-component detector postprocess.
- Uses axis-aligned crops for recognition.
- Uses CTC decode from `rec/inference.yml` character dictionary.

### Verification

RED test added:

```powershell
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-real-ocr -C Debug -R pl_ocr_cli_paddleocr_unconfigured --output-on-failure
```

Initial failure:

```text
status=backend-unavailable ... unknown OCR backend requested
expected status model-not-configured but got backend-unavailable
```

GREEN / final commands:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-real-ocr -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-real-ocr --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-real-ocr -C Debug --output-on-failure
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend paddleocr_onnx --model-dir D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx --self-test --print-text
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend stub --self-test
```

Results:

- Debug build succeeded.
- CTest passed: `5/5`.
- `paddleocr_onnx` self-test returned `status=ok boxes=3`.
- Example OCR output:
  - `PrivacvLensOCRTEST` (title has one character error)
  - `demo@example.com`
  - `+15550100`
- Stub backend remained unchanged and returned 3 synthetic boxes.

### Known Limitations

- Detector postprocess is a first SDK-owned implementation, not full PaddleOCR
  DBPostProcess parity.
- Cropping is axis-aligned; perspective rectification is deferred.
- Confidence currently combines detector score and recognizer probability.
- Phone-number spacing is lost by recognizer output, which is acceptable for
  regex-style detection but should be tracked.
- The CMake default ONNX Runtime path is local Artifact-specific and may need a
  cleaner packaging option before broader reuse.

## 2026-06-14 - ScreenTest Screenshot Validation

### Scope

- Tested the current `paddleocr_onnx` SDK backend on five real PC screenshots:

```text
D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens-OCR-SDK\tests\ScreenTest\pics
```

- Used the SDK CLI as the execution path, not the Python side-validation path.
- Avoided pasting full real-screen OCR text into logs or handoff notes.

### Clarification

Previous SDK self-test image was a synthetic in-memory GDI image generated by
`pl-ocr-cli`, with safe test text such as:

- `PrivacyLens OCR TEST`
- `demo@example.com`
- `+1 555 0100`

The earlier Python side-validation image was a separate synthetic artifact under:

```text
D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx\side-validation
```

### PaddleOCR Pipeline Shape

Current `paddleocr_onnx` uses two separate ONNX models and two internal stages:

- detector: `det/model.onnx`
- recognizer: `rec/model.onnx`

They are exposed to PrivacyLens as one backend pipeline. The public SDK ABI still
sees only `recognize_image -> pl_ocr_result`; detector/recognizer details remain
inside the SDK adapter.

### SDK CLI Command

```powershell
$repo='D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens-OCR-SDK'
$model='D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx'
$cli=Join-Path $repo 'build-real-ocr\Debug\pl-ocr-cli.exe'
Get-ChildItem (Join-Path $repo 'tests\ScreenTest\pics') -Filter *.png |
  Sort-Object Name |
  ForEach-Object {
    & $cli --backend paddleocr_onnx --model-dir $model --image $_.FullName
  }
```

### Results

| Screenshot                | Status | Boxes | Total latency |
| ------------------------- | ------ | ----: | ------------: |
| `QQ20260614-163422.png` | ok     |   175 |       ~1.80 s |
| `QQ20260614-163454.png` | ok     |     4 |       ~0.25 s |
| `QQ20260614-163542.png` | ok     |   228 |       ~2.19 s |
| `QQ20260614-163600.png` | ok     |   277 |       ~2.82 s |
| `QQ20260614-163607.png` | ok     |   287 |       ~2.85 s |

Generated local artifacts:

```text
D:\Atlas\Artifacts\TestData\PrivacyLens-OCR-SDK\ScreenTest\paddleocr-onnx-sdk-cli
```

Important files:

- `summary_redacted.json`
- `contact_sheet_boxes.png`
- `annotated-boxes\*_boxes.png`
- `raw_results_private.json`
- `raw-cli-output-private\*.txt`

### Observations

- Dense app/window screenshots are usable as an end-to-end flow smoke test.
- The backend recognizes many visible project/tool terms in dense windows, such
  as `PrivacyLens`, `PaddleOCR`, `PP-OCRv6`, `pl_ocr_get_api`, and
  `stub_ocr_backend`.
- The pure desktop screenshot is weak: only 4 boxes were detected, so desktop
  icon labels are mostly missed.
- Current detector postprocess favors stronger text lines in application
  windows and misses or fragments small/low-contrast UI text.
- Latency on dense 1440p screenshots is currently around 1.8-2.9 seconds on CPU,
  which is acceptable for integration validation but not yet good enough for a
  polished real-time screen OCR path.

### Current Conclusion

The SDK-level PaddleOCR adapter path is validated on real screenshots, but the
current implementation should still be treated as an MVP pipeline:

- good for proving SDK integration and result materialization;
- not yet production-quality for desktop icon labels or dense small UI text;
- next quality work should focus on detector postprocess parity, crop
  rectification, thresholds, and possibly tiling/upscaling for desktop UI text.

## 2026-06-16 - Handoff Recheck

### Scope

- Took over current `PrivacyLens-OCR-SDK` state without starting new feature
  implementation.
- Reconfirmed git state, artifact paths, core architecture files, local build,
  tests, and backend smoke behavior.
- Did not change `include/pl_ocr.h` or any SDK source file.
- Did not read or paste private raw OCR text from real screenshot artifacts.

### Git Boundary

Commands run:

```powershell
git status --short --branch
git log -1 --oneline --decorate
git remote -v
git status --ignored --short tests/ScreenTest WORKLOG.md
```

Results:

- Current branch is `codex/paddleocr-onnx-backend`, tracking
  `origin/codex/paddleocr-onnx-backend`.
- HEAD is:
  `2a74dfc (HEAD -> codex/paddleocr-onnx-backend, origin/codex/paddleocr-onnx-backend) Add PaddleOCR ONNX backend`
- Remote is:
  `https://github.com/NatsumeMoMo/PrivacyLens-OCR-SDK.git`
- `tests/ScreenTest/` remains untracked.
- `WORKLOG.md` is ignored and remains a local handoff file.

### Architecture Recheck

- `include/pl_ocr.h` remains the public C ABI boundary.
- `pl_ocr_get_api` remains the single exported API entry.
- `src/core/ocr_backend.hpp` exposes the internal backend contract as
  `backend_info()`, `capabilities()`, and `recognize(const OcrRequest&)`.
- `src/core/ocr_engine.cpp` selects `stub`, `rapidocr_onnx` / `rapidocr`, and
  `paddleocr_onnx` / `paddleocr`.
- `src/c_api/pl_ocr_api.cpp` owns the C ABI bridge, validates public structs,
  catches C++ exceptions, maps internal errors to `pl_ocr_status`, and
  materializes `pl_ocr_result` with SDK-owned storage.
- `paddleocr_onnx` remains an SDK-internal adapter: ONNX Runtime sessions,
  detector preprocessing, connected-component detector postprocess,
  axis-aligned crop, recognizer inference, and CTC decode are hidden behind
  `IOcrBackend`.

### Artifact Recheck

Confirmed model root:

```text
D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx
```

Required PaddleOCR files exist:

- `det/model.onnx`
- `det/inference.yml`
- `rec/model.onnx`
- `rec/inference.yml`
- `manifest.json`
- `tools/side_validate_ppocrv6.py`

Confirmed ONNX Runtime root:

```text
D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\ONNXRuntime\onnxruntime-win-x64-1.26.0\package\onnxruntime-win-x64-1.26.0
```

Required ONNX Runtime files exist:

- `include/onnxruntime_cxx_api.h`
- `include/onnxruntime_c_api.h`
- `lib/onnxruntime.lib`
- `lib/onnxruntime.dll`

Confirmed screenshot test report root:

```text
D:\Atlas\Artifacts\TestData\PrivacyLens-OCR-SDK\ScreenTest\paddleocr-onnx-sdk-cli
```

Report files/directories exist:

- `summary_redacted.json`
- `contact_sheet_boxes.png`
- `annotated-boxes\` with 5 files
- `raw_results_private.json`
- `raw-cli-output-private\` with 5 files

The redacted summary still reports five successful screenshot runs with box
counts `175`, `4`, `228`, `277`, and `287`, and total latency around
`0.25` to `2.85` seconds depending on screenshot density.

### Verification

Commands run:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-real-ocr -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-real-ocr --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-real-ocr -C Debug --output-on-failure
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend paddleocr_onnx --model-dir D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx --self-test --print-text
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend rapidocr_onnx --model-dir D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PP-OCRv3-rapidocr-onnx --self-test --print-text --expect-status backend-unavailable
```

Results:

- CMake configure succeeded.
- Debug build succeeded.
- CTest passed: `5/5`.
- `paddleocr_onnx` self-test returned `status=ok boxes=3`.
- Synthetic self-test still recognizes:
  - `PrivacvLensOCRTEST`
  - `demo@example.com`
  - `+15550100`
- The title still has the known `y` -> `v` recognition error.
- Phone-number spaces are still lost.
- Configured `rapidocr_onnx` still fails closed with:
  `RapidOcrOnnx.dll is missing required C API exports`

### Notes For Next Step

- No code layer is currently selected for modification.
- If continuing quality work, likely layers are:
  - detector postprocess;
  - recognizer/crop/decoder;
  - packaging/build for cleaner ONNX Runtime configuration.
- Public ABI should remain untouched unless there is a separate explicit
  decision.
- A small non-blocking documentation issue was noticed: `pl-ocr-cli --help`
  usage text still lists RapidOCR examples but not PaddleOCR examples.

## 2026-06-17 - Handoff Recheck

### Scope

- Took over current `PrivacyLens-OCR-SDK` state without starting feature
  implementation.
- Reconfirmed git state, artifact paths, core architecture files, local build,
  tests, PaddleOCR synthetic self-test, and RapidOCR fail-closed behavior.
- Did not change `include/pl_ocr.h` or any SDK source file.
- Did not read or paste private raw OCR text from real screenshot artifacts.

### Git Boundary

Commands run:

```powershell
git status --short --branch
git log -1 --oneline --decorate
git remote -v
git status --ignored --short tests\ScreenTest WORKLOG.md
```

Results:

- Current branch is `codex/paddleocr-onnx-backend`, tracking
  `origin/codex/paddleocr-onnx-backend`.
- HEAD is:
  `2a74dfc (HEAD -> codex/paddleocr-onnx-backend, origin/codex/paddleocr-onnx-backend) Add PaddleOCR ONNX backend`
- Remote is:
  `https://github.com/NatsumeMoMo/PrivacyLens-OCR-SDK.git`
- `tests/ScreenTest/` remains untracked.
- `WORKLOG.md` is ignored and remains a local handoff file.

### Architecture Recheck

- `include/pl_ocr.h` remains the public C ABI boundary.
- `pl_ocr_get_api` remains the single exported API entry.
- `src/core/ocr_backend.hpp` exposes the internal backend contract as
  `backend_info()`, `capabilities()`, and `recognize(const OcrRequest&)`.
- `src/core/ocr_engine.cpp` selects `stub`, `rapidocr_onnx` / `rapidocr`, and
  `paddleocr_onnx` / `paddleocr`.
- `src/c_api/pl_ocr_api.cpp` owns the C ABI bridge, validates public structs,
  catches C++ exceptions, maps internal errors to `pl_ocr_status`, and
  materializes `pl_ocr_result` with SDK-owned storage.
- `paddleocr_onnx` remains an SDK-internal adapter: ONNX Runtime sessions,
  detector preprocessing, connected-component detector postprocess,
  axis-aligned crop, recognizer inference, and CTC decode are hidden behind
  `IOcrBackend`.
- `rapidocr_onnx` still requires `OcrInit`, `OcrDetectInput`,
  `OcrFreeResult`, and `OcrDestroy`; missing required exports still map to
  `PL_OCR_STATUS_BACKEND_UNAVAILABLE`.

### Artifact Recheck

Confirmed model root:

```text
D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx
```

Required PaddleOCR files exist:

- `det/model.onnx`
- `det/inference.yml`
- `rec/model.onnx`
- `rec/inference.yml`
- `manifest.json`
- `tools/side_validate_ppocrv6.py`

Confirmed ONNX Runtime root:

```text
D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\ONNXRuntime\onnxruntime-win-x64-1.26.0\package\onnxruntime-win-x64-1.26.0
```

Required ONNX Runtime files exist:

- `include/onnxruntime_cxx_api.h`
- `include/onnxruntime_c_api.h`
- `lib/onnxruntime.lib`
- `lib/onnxruntime.dll`

Confirmed screenshot test report root:

```text
D:\Atlas\Artifacts\TestData\PrivacyLens-OCR-SDK\ScreenTest\paddleocr-onnx-sdk-cli
```

Report files/directories exist:

- `summary_redacted.json`
- `contact_sheet_boxes.png`
- `annotated-boxes\` with 5 files
- `raw_results_private.json`
- `raw-cli-output-private\` with 5 files

The redacted summary still reports five successful screenshot runs with box
counts `175`, `4`, `228`, `277`, and `287`, and total latency around
`0.25` to `2.85` seconds depending on screenshot density.

### Verification

Commands run:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-real-ocr -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-real-ocr --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-real-ocr -C Debug --output-on-failure
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend paddleocr_onnx --model-dir D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx --self-test --print-text
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend rapidocr_onnx --model-dir D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PP-OCRv3-rapidocr-onnx --self-test --print-text --expect-status backend-unavailable
```

Results:

- CMake configure succeeded.
- Debug build succeeded.
- CTest passed: `5/5`.
- `paddleocr_onnx` self-test returned `status=ok boxes=3`.
- Synthetic self-test still recognizes:
  - `PrivacvLensOCRTEST`
  - `demo@example.com`
  - `+15550100`
- The title still has the known `y` -> `v` recognition error.
- Phone-number spaces are still lost.
- Configured `rapidocr_onnx` still fails closed with:
  `RapidOcrOnnx.dll is missing required C API exports`

### Notes For Next Step

- No code layer is currently selected for modification.
- If continuing quality work, likely layers are:
  - detector postprocess;
  - recognizer/crop/decoder;
  - packaging/build for cleaner ONNX Runtime configuration;
  - CLI documentation/help text for PaddleOCR examples.
- Public ABI should remain untouched unless there is a separate explicit
  decision.

## 2026-06-17 - Current Session Handoff Recheck

### Scope

- Took over current `PrivacyLens-OCR-SDK` state from the pasted handoff note.
- Read `AGENTS.md`, `README.md`, `WORKLOG.md`, the public ABI header, core
  engine/backend/C API bridge files, the PaddleOCR ONNX backend, CLI entrypoint,
  and the main architecture/artifact/test docs.
- Reconfirmed current git state, artifact paths, local build, CTest, PaddleOCR
  synthetic self-test, RapidOCR fail-closed behavior, and RapidOcrOnnx exports.
- Did not start feature implementation.
- Did not change `include/pl_ocr.h` or any SDK source file.
- Did not read or paste private raw OCR text from real screenshot artifacts.

### Git Boundary

Commands run:

```powershell
git status --short --branch
git log -1 --oneline --decorate
git remote -v
git status --ignored --short tests\ScreenTest WORKLOG.md
```

Results:

- Current branch is `codex/paddleocr-onnx-backend`, tracking
  `origin/codex/paddleocr-onnx-backend`.
- HEAD is:
  `2a74dfc (HEAD -> codex/paddleocr-onnx-backend, origin/codex/paddleocr-onnx-backend) Add PaddleOCR ONNX backend`
- Remote is:
  `https://github.com/NatsumeMoMo/PrivacyLens-OCR-SDK.git`
- `tests/ScreenTest/` remains untracked.
- `WORKLOG.md` is ignored and remains the local handoff file.
- The pasted handoff note mentioned `codex/paddleocr-cuda-baseline` as the last
  observed local branch, but the current checkout is
  `codex/paddleocr-onnx-backend`.

### Architecture Recheck

- `include/pl_ocr.h` remains the stable public C ABI boundary.
- `pl_ocr_get_api` remains the single exported API entry.
- `src/c_api/pl_ocr_api.cpp` owns the C ABI bridge, validates public structs,
  catches C++ exceptions, maps internal errors to `pl_ocr_status`, and
  materializes `pl_ocr_result` through SDK-owned storage.
- `src/core/ocr_engine.cpp` selects `stub`, `rapidocr_onnx` / `rapidocr`, and
  `paddleocr_onnx` / `paddleocr`.
- `src/core/ocr_backend.hpp` keeps backend implementation behind
  `IOcrBackend`.
- `paddleocr_onnx` remains an SDK-internal ONNX Runtime CPU adapter with
  detector preprocessing, connected-component postprocess, axis-aligned crop,
  recognizer inference, and CTC decode hidden behind the backend contract.
- CLI output for real image files remains privacy-aware: OCR text is hidden by
  default unless `--print-text` is explicitly passed.

### Artifact Recheck

Confirmed PaddleOCR model root:

```text
D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx
```

Required files exist:

- `det/model.onnx`
- `det/inference.yml`
- `rec/model.onnx`
- `rec/inference.yml`
- `manifest.json`
- `tools/side_validate_ppocrv6.py`

Confirmed ONNX Runtime root:

```text
D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\ONNXRuntime\onnxruntime-win-x64-1.26.0\package\onnxruntime-win-x64-1.26.0
```

Required files exist:

- `include/onnxruntime_cxx_api.h`
- `include/onnxruntime_c_api.h`
- `lib/onnxruntime.lib`
- `lib/onnxruntime.dll`

Confirmed Python side-validation venv:

```text
D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\python-venv-paddleocr-onnx\Scripts\python.exe
```

Confirmed RapidOCR legacy artifacts still exist:

- `D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PP-OCRv3-rapidocr-onnx`
- `D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\RapidOcrOnnx-1.2.2\windows-clib\windows-clib\win-CLIB-CPU-x64\bin\RapidOcrOnnx.dll`

Confirmed screenshot test input directory:

```text
D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens-OCR-SDK\tests\ScreenTest\pics
```

It contains five PNG screenshots and remains untracked.

Confirmed screenshot test report root:

```text
D:\Atlas\Artifacts\TestData\PrivacyLens-OCR-SDK\ScreenTest\paddleocr-onnx-sdk-cli
```

Report files/directories exist:

- `summary_redacted.json`
- `contact_sheet_boxes.png`
- `annotated-boxes\` with 5 files
- `raw_results_private.json`
- `raw-cli-output-private\` with 5 files

The redacted summary still reports five successful screenshot runs with box
counts `175`, `4`, `228`, `277`, `287`; total latency is roughly `0.25` to
`2.85` seconds depending on screenshot density.

### Verification

Commands run:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-real-ocr -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-real-ocr --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-real-ocr -C Debug --output-on-failure
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend paddleocr_onnx --model-dir D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx --self-test --print-text
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend rapidocr_onnx --model-dir D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PP-OCRv3-rapidocr-onnx --self-test --print-text --expect-status backend-unavailable
& 'C:\Software\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\HostX64\x64\dumpbin.exe' /exports D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\RapidOcrOnnx-1.2.2\windows-clib\windows-clib\win-CLIB-CPU-x64\bin\RapidOcrOnnx.dll
```

Results:

- CMake configure succeeded.
- Debug build succeeded.
- CTest passed: `5/5`.
- `paddleocr_onnx` self-test returned `status=ok boxes=3`.
- Synthetic self-test output remains:
  - `PrivacvLensOCRTEST`
  - `demo@example.com`
  - `+15550100`
- The title still has the known `y` -> `v` recognition error.
- Phone-number spaces are still lost.
- Configured `rapidocr_onnx` still fails closed with:
  `RapidOcrOnnx.dll is missing required C API exports`
- `dumpbin /exports` confirms the current `RapidOcrOnnx.dll` exports only:
  - `OcrDestroy`
  - `OcrDetect`
  - `OcrGetLen`
  - `OcrGetResult`
  - `OcrInit`
- It still does not export `OcrDetectInput` or `OcrFreeResult`.

### Notes For Next Step

- No code layer is currently selected for modification.
- If the next request targets quality, the likely layers are detector
  postprocess and recognizer/crop/decoder.
- If the next request targets deployability, the likely layer is
  packaging/build for cleaner ONNX Runtime configuration.
- If the next request targets CLI polish, `pl-ocr-cli --help` still needs
  PaddleOCR examples.
- Public ABI should remain untouched unless there is a separate explicit
  decision.

## 2026-06-17 - Codex Handoff Recheck Before New Work

### Scope

- Re-read the pasted handoff request, `AGENTS.md`, memory guidance, current
  `WORKLOG.md`, required repo docs, public ABI/header bridge files, the
  `OcrEngine` backend selector, PaddleOCR ONNX backend, and CLI entrypoint.
- Stayed in reconnaissance mode only: no new feature implementation and no SDK
  source edits.
- Confirmed the current checkout, artifact paths, build state, CTest state,
  PaddleOCR synthetic self-test, and RapidOCR fail-closed blocker.
- Did not read or paste raw private screenshot OCR output.

### Git Boundary

Commands run:

```powershell
git status --short --branch
git log -1 --oneline --decorate
git remote -v
git status --ignored --short tests\ScreenTest WORKLOG.md
git status --porcelain=v1 --untracked-files=all
```

Results:

- Current branch is `codex/paddleocr-onnx-backend`, tracking
  `origin/codex/paddleocr-onnx-backend`.
- HEAD is:
  `2a74dfc (HEAD -> codex/paddleocr-onnx-backend, origin/codex/paddleocr-onnx-backend) Add PaddleOCR ONNX backend`
- Remote is:
  `https://github.com/NatsumeMoMo/PrivacyLens-OCR-SDK.git`
- `tests/ScreenTest/pics` remains untracked and contains five PNG screenshots.
- `WORKLOG.md` is ignored and remains a local handoff file.
- The pasted handoff mentioned `codex/paddleocr-cuda-baseline` as a previous
  observed branch, but the current checkout is `codex/paddleocr-onnx-backend`.

### Artifact Recheck

Confirmed PaddleOCR model root:

```text
D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx
```

Required files exist:

- `det/model.onnx`
- `det/inference.yml`
- `rec/model.onnx`
- `rec/inference.yml`
- `manifest.json`
- `tools/side_validate_ppocrv6.py`

Confirmed ONNX Runtime root:

```text
D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\ONNXRuntime\onnxruntime-win-x64-1.26.0\package\onnxruntime-win-x64-1.26.0
```

Required files exist:

- `include/onnxruntime_cxx_api.h`
- `include/onnxruntime_c_api.h`
- `lib/onnxruntime.lib`
- `lib/onnxruntime.dll`

Also confirmed:

- PaddleOCR side-validation Python:
  `D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\python-venv-paddleocr-onnx\Scripts\python.exe`
- RapidOCR legacy model root:
  `D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PP-OCRv3-rapidocr-onnx`
- RapidOCR legacy runtime DLL:
  `D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\RapidOcrOnnx-1.2.2\windows-clib\windows-clib\win-CLIB-CPU-x64\bin\RapidOcrOnnx.dll`
- Screenshot report root:
  `D:\Atlas\Artifacts\TestData\PrivacyLens-OCR-SDK\ScreenTest\paddleocr-onnx-sdk-cli`

The redacted screenshot summary still reports five successful runs with box
counts `175`, `4`, `228`, `277`, and `287`; total latency ranges from roughly
`0.25` to `2.85` seconds depending on screenshot density.

### Verification

Commands run:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-real-ocr -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-real-ocr --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-real-ocr -C Debug --output-on-failure
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend paddleocr_onnx --model-dir D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PaddleOCR\PP-OCRv6-small-onnx --self-test --print-text
.\build-real-ocr\Debug\pl-ocr-cli.exe --backend rapidocr_onnx --model-dir D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\PP-OCRv3-rapidocr-onnx --self-test --print-text --expect-status backend-unavailable
& 'C:\Software\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\HostX64\x64\dumpbin.exe' /exports D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\RapidOcrOnnx-1.2.2\windows-clib\windows-clib\win-CLIB-CPU-x64\bin\RapidOcrOnnx.dll
```

Results:

- CMake configure succeeded.
- Debug build succeeded.
- CTest passed: `5/5`.
- `paddleocr_onnx` synthetic self-test returned `status=ok boxes=3`.
- Synthetic self-test still recognizes the title with the known `y` -> `v`
  error and drops phone-number spaces.
- Configured `rapidocr_onnx` still fails closed with:
  `RapidOcrOnnx.dll is missing required C API exports`
- `dumpbin /exports` confirms the current `RapidOcrOnnx.dll` exports only:
  - `OcrDestroy`
  - `OcrDetect`
  - `OcrGetLen`
  - `OcrGetResult`
  - `OcrInit`
- It still does not export `OcrDetectInput` or `OcrFreeResult`.

### Notes For Next Step

- No code layer is currently selected for modification.
- If the next request targets quality, likely layers are detector postprocess
  and recognizer/crop/decoder.
- If the next request targets deployability, likely layer is packaging/build for
  cleaner ONNX Runtime configuration.
- If the next request targets CUDA baseline, first confirm the ONNX Runtime GPU
  package, CUDA/cuDNN dependencies, and provider initialization strategy.
- Public ABI should remain untouched unless there is a separate explicit
  decision.

## 2026-06-18 - Engine SDK Redaction Pipeline Refactor

### Scope

- Created branch `codex/engine-sdk-redaction-pipeline` from the current SDK
  checkout.
- Wrote local design note:
  `docs/engine-sdk-redaction-pipeline-design.md`
- Wrote local implementation plan:
  `docs/superpowers/plans/2026-06-18-engine-sdk-redaction-pipeline.md`
- Refactored the active build surface from OCR SDK to Engine SDK.
- Did not push any branch.
- Did not add real screenshots, models, generated binaries, or raw OCR output
  to Git.

### Architecture Decision

- The PrivacyLens main application owns user-facing settings and persistent
  configuration.
- The SDK does not read the main app's settings file directly.
- Runtime truth is the latest `pl_engine_policy` snapshot pushed through
  `pl_engine_session_update_policy`.
- The public SDK style is now direct exported C functions, similar in spirit to
  FFmpeg / whisper.cpp, rather than the previous `pl_ocr_get_api` function
  table.
- The first active provider is app-window geometry masking for WeChat / Weixin.
- OCR, YOLO, manual-region, QR, face, and template providers are deferred
  extension points.

### Files Changed

- `CMakeLists.txt`
  - Replaced active build target with `PrivacyLensEngine`.
  - Added `pl-engine-cli` and Engine SDK tests.
- `README.md`
  - Rewritten for Engine SDK current state, build, ABI, and WeChat provider.
- `include/pl_engine.h`
  - Added direct-export C ABI and public policy/frame/mask structs.
- `src/c_api/pl_engine_api.cpp`
  - Added C ABI bridge, error mapping, and SDK-owned mask-list materialization.
- `src/core/engine_*`
  - Added engine/session state, policy copy, provider dispatch, and latest-mask
    cache.
- `src/providers/app_window/*`
  - Added visible-region geometry helpers and Win32/DWM WeChat provider.
- `tests/*engine*` and `tests/visible_regions_tests.cpp`
  - Added geometry, policy/session, and provider tests.
- `tools/pl-engine-cli/main.cpp`
  - Added privacy-safe self-test CLI.

### TDD Evidence

- Geometry tests were added before `visible_regions.hpp`; the first build failed
  because the include was missing.
- Policy/session tests were added before `pl_engine.h` and
  `pl_engine_api.cpp`; CMake/build failed because those files did not exist.
- Provider tests were added before `app_window_provider.cpp`; CMake/build failed
  because the provider source did not exist.
- CLI self-test target was added before `tools/pl-engine-cli/main.cpp`; build
  failed because the CLI source did not exist.

### Verification

Commands run:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-engine -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-engine --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-engine -C Debug --output-on-failure
.\build-engine\Debug\pl-engine-cli.exe --self-test --mask-wechat
```

Results:

- CMake configure succeeded.
- Debug build succeeded.
- CTest passed: `4/4`.
- CLI exited successfully and printed privacy-safe status:
  `status=OK policy_generation=1 mask_wechat=1 masks=1`
- The nonzero mask count depended on the current desktop having a visible
  WeChat / Weixin-related window on the primary monitor.

### Git Boundary

- `tests/ScreenTest/` remains untracked and must stay out of Git.
- `WORKLOG.md`, `docs/`, and `tests/README.md` remain ignored local handoff
  material under the current repository policy.
- Intended SDK commit scope is source, tests, CMake, and top-level `README.md`
  only.

### Deferred

- Manual full-pipeline visual verification in the running PrivacyLens UI.
- Future QQ, YOLO, OCR, manual-region, and template providers.

### Follow-Up: Main Project Integration

- SDK local commit after amend:
  `7e08fe0 refactor: introduce engine SDK redaction pipeline`
- Main PrivacyLens integration added:
  - runtime `PrivacyLensEngine.dll` loader;
  - `Mask WeChat windows` settings toggle;
  - monitor capture entry for app-window coordinate alignment;
  - SDK mask injection into `PreviewWidget`.
- Main verification passed:
  - Debug build;
  - CTest `4/4`;
  - loaded Engine SDK smoke returned one privacy-safe mask count;
  - fake `wechat.exe` pipeline smoke returned app-window masks and injected
    them into the preview widget;
  - app launch smoke stayed running for 3 seconds.
- Optional remaining product check: a human visual pass can still confirm masks
  follow a real WeChat / Weixin window in the running PrivacyLens UI, but the
  automated smoke no longer depends on private user windows or screenshots.

## 2026-06-18: Engine SDK Architecture Cleanup

### Scope

- Wrote `docs/engine-sdk-architecture-cleanup-design.md` and
  `docs/superpowers/plans/2026-06-18-engine-sdk-architecture-cleanup.md`.
- Removed the obsolete OCR-only SDK surface:
  - `include/pl_ocr.h`
  - `src/c_api/pl_ocr_api.cpp`
  - `src/core/ocr_*`
  - `src/backends/stub/`
  - `src/backends/rapidocr_onnx/`
  - `src/backends/paddleocr_onnx/`
  - `tools/pl-ocr-cli/`
  - `tests/adapter_contract_tests.cpp`
- Updated `README.md` to state that `pl_engine.h` is the only supported public
  SDK header.
- Updated local SDK docs and `tests/README.md` to describe future OCR/YOLO work
  as Engine providers/backends, not as a restored OCR-only ABI.
- Added `tests/ScreenTest/` to `.gitignore` so local screenshot experiments stay
  out of Git.

### ABI / Behavior Changes

- `PrivacyLensEngine.dll` and direct `pl_engine_*` exports remain the supported
  DLL boundary.
- The old `pl_ocr_get_api` function-table ABI has been removed.
- The active SDK behavior is unchanged for the WeChat app-window provider:
  policy is pushed through `pl_engine_session_update_policy`, monitor-frame
  context is submitted through `pl_engine_session_submit_frame`, and masks are
  retrieved through `pl_engine_session_get_latest_masks`.

### Verification

Commands run:

```powershell
rg -n "pl_ocr|ocr_backend|ocr_engine|PrivacyLensOcr|pl-ocr|rapidocr|paddleocr" CMakeLists.txt include src tools tests
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-engine -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-engine --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-engine -C Debug --output-on-failure
.\build-engine\Debug\pl-engine-cli.exe --self-test --mask-wechat
```

Results:

- Source-level OCR contract check produced no output after cleanup.
- CMake configure succeeded.
- Debug build succeeded.
- CTest passed: `4/4`.
- CLI exited successfully and printed privacy-safe status:
  `status=OK policy_generation=1 mask_wechat=1 masks=0`.
- `masks=0` was valid for the current desktop because no visible matching
  WeChat / Weixin window was present on the primary monitor during that run.

### Main Project Cleanup

- Removed main-project `src/ocr_sdk`, `OcrSdkPanel`, and
  `PrivacyLensOcrSdkLoaderSmoke`.
- Main project now has one SDK route: runtime loading `PrivacyLensEngine.dll`
  through `src/engine_sdk`.
- Main verification after cleanup:
  - source-level old OCR loader check produced no output;
  - Debug build succeeded;
  - CTest passed Engine-only tests: `3/3`;
  - loaded Engine SDK smoke printed `Engine SDK: 0 mask(s), policy generation 1`.

### Artifacts Excluded

- `tests/ScreenTest/` remains local and ignored.
- Build outputs under `build-engine/` remain out of Git.
- No model files, runtime packages, real screenshots, raw OCR text, or generated
  binaries were added.

### Deferred

- Future OCR must be reintroduced under an Engine provider/backend contract.
- Future YOLO/object detection must use the same provider/backend layering.
- Product visual verification with a real WeChat / Weixin window remains an
  optional manual acceptance pass; automated smoke uses the fake `wechat.exe`
  pipeline.
