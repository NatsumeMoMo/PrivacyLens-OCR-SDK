# Engine SDK Architecture Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the obsolete OCR-only SDK path and leave Engine SDK as the only supported SDK ABI and main-project integration route.

**Architecture:** The SDK exposes only `include/pl_engine.h` and `src/c_api/pl_engine_api.cpp`. `EngineSession` dispatches providers, and providers can later own private backend interfaces where models or runtimes are needed. The main application owns user settings/capture/rendering and talks to the SDK only through runtime Engine policy and mask APIs.

**Tech Stack:** CMake, Visual Studio 18 2026 / MSVC, C++20, Qt 6.11.0 in the main project, Win32/DWM app-window provider, C ABI DLL boundary.

---

### Task 1: Document The Cleanup Contract

**Files:**
- Create: `docs/engine-sdk-architecture-cleanup-design.md`
- Create: `docs/superpowers/plans/2026-06-18-engine-sdk-architecture-cleanup.md`
- Modify later: `WORKLOG.md`

- [ ] **Step 1: Write the architecture cleanup design**

Create `docs/engine-sdk-architecture-cleanup-design.md` with:

```markdown
# Engine SDK Architecture Cleanup Design

## Target Boundary

PrivacyLens main application -> include/pl_engine.h -> src/c_api/pl_engine_api.cpp -> EngineSession -> Providers -> optional private Backends.

## Cleanup Scope

Remove the old OCR ABI, OCR engine, OCR backend implementations, OCR CLI, and main-project OCR runtime loader.
```

- [ ] **Step 2: Self-review the design**

Run:

```powershell
rg -n "T[B]D|TO[D]O|implem[e]nt later|fi[l]l in" docs/engine-sdk-architecture-cleanup-design.md docs/superpowers/plans/2026-06-18-engine-sdk-architecture-cleanup.md
```

Expected: no matches.

### Task 2: Prove SDK Cleanup With A Failing Contract Check

**Files:**
- Test: use `rg` and CMake configure as the contract checks
- Modify: `CMakeLists.txt`
- Delete: `include/pl_ocr.h`, `src/c_api/pl_ocr_api.cpp`, `src/core/ocr_*`, `src/backends/*`, `tools/pl-ocr-cli/main.cpp`, `tests/adapter_contract_tests.cpp`

- [ ] **Step 1: Verify the current SDK still contains old OCR symbols**

Run:

```powershell
rg -n "pl_ocr|ocr_backend|ocr_engine|PrivacyLensOcr|pl-ocr|rapidocr|paddleocr" CMakeLists.txt include src tools tests README.md docs
```

Expected: matches exist in old OCR files and historical docs. This is the RED state for cleanup.

- [ ] **Step 2: Delete obsolete SDK files**

Remove these tracked files:

```text
include/pl_ocr.h
src/c_api/pl_ocr_api.cpp
src/core/ocr_backend.hpp
src/core/ocr_engine.cpp
src/core/ocr_engine.hpp
src/core/ocr_error.hpp
src/core/ocr_types.hpp
src/backends/stub/stub_ocr_backend.cpp
src/backends/stub/stub_ocr_backend.hpp
src/backends/rapidocr_onnx/rapidocr_onnx_backend.cpp
src/backends/rapidocr_onnx/rapidocr_onnx_backend.hpp
src/backends/paddleocr_onnx/paddleocr_onnx_backend.cpp
src/backends/paddleocr_onnx/paddleocr_onnx_backend.hpp
tools/pl-ocr-cli/main.cpp
tests/adapter_contract_tests.cpp
```

- [ ] **Step 3: Update SDK documentation and ignore rules**

Update:

```text
README.md
docs/architecture.md
docs/backend-roadmap.md
docs/dynamic-loading.md
docs/artifact-policy.md
tests/README.md
.gitignore
```

The docs should describe old OCR code as removed, not merely inactive. Add `tests/ScreenTest/` to `.gitignore`.

- [ ] **Step 4: Verify SDK no longer exposes old OCR source**

Run:

```powershell
rg --files include src tools tests | rg "pl_ocr|ocr_|backends|pl-ocr|adapter_contract"
```

Expected: no output.

- [ ] **Step 5: Build and test SDK**

Run:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-engine -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-engine --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-engine -C Debug --output-on-failure
.\build-engine\Debug\pl-engine-cli.exe --self-test --mask-wechat
```

Expected: configure/build succeed, CTest reports 4/4 passing, CLI returns `status=OK`.

### Task 3: Prove Main Project Cleanup With A Failing Contract Check

**Files:**
- Modify: `D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens\CMakeLists.txt`
- Modify: `D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens\src\ui\MainWindow.h`
- Modify: `D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens\src\ui\MainWindow.cpp`
- Delete: `D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens\src\ocr_sdk\OcrSdkLoader.cpp`
- Delete: `D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens\src\ocr_sdk\OcrSdkLoader.h`
- Delete: `D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens\src\ocr_sdk\OcrSdkTypes.h`
- Delete: `D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens\src\ui\OcrSdkPanel.cpp`
- Delete: `D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens\src\ui\OcrSdkPanel.h`
- Delete: `D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens\tests\ocr_sdk_loader_smoke.cpp`

- [ ] **Step 1: Verify current main project still contains old OCR loader path**

Run:

```powershell
rg -n "ocr_sdk|OcrSdk|PrivacyLensOcr|pl_ocr|PL_OCR|OCR SDK Runtime Loader" CMakeLists.txt src tests docs .spec
```

Expected: matches exist in CMake, `src/ocr_sdk`, `OcrSdkPanel`, tests, and specs. This is the RED state for cleanup.

- [ ] **Step 2: Remove old OCR loader files and CMake target**

Remove source entries, include dirs, compile definitions, `PrivacyLensOcrSdkLoaderSmoke`, and `ocr_sdk_loader_missing`.

The remaining SDK-related CMake variables should be:

```cmake
set(PRIVACYLENS_ENGINE_SDK_ROOT ...)
set(PRIVACYLENS_ENGINE_SDK_RUNTIME_ROOT ...)
set(PRIVACYLENS_ENGINE_SDK_INCLUDE_DIR ...)
```

- [ ] **Step 3: Remove `OcrSdkPanel` from the Log page**

In `MainWindow.h`, remove:

```cpp
class OcrSdkPanel;
OcrSdkPanel* m_ocrSdkPanel = nullptr;
```

In `MainWindow.cpp`, remove:

```cpp
#include "ui/OcrSdkPanel.h"
m_ocrSdkPanel = new OcrSdkPanel(page);
```

Keep benchmark diagnostics and Engine SDK WeChat status intact.

- [ ] **Step 4: Update main project specs**

Update:

```text
.spec/00-overview/current-context.md
.spec/02-architecture/system-architecture.md
.spec/02-architecture/repo-structure.md
.spec/04-testing/acceptance-checklist.md
.spec/05-planning/project-log.md
docs/SDK/runtime-loader.md
```

These files should state that the old OCR runtime loader was removed after the
Engine SDK migration.

- [ ] **Step 5: Verify main project no longer references old OCR loader**

Run:

```powershell
rg -n "src/ocr_sdk|OcrSdk|PrivacyLensOcr|pl_ocr|PL_OCR|PrivacyLensOcrSdkLoaderSmoke|ocr_sdk_loader_missing" CMakeLists.txt src tests
```

Expected: no output.

- [ ] **Step 6: Build and test main project**

Run:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-engine-integration -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH="C:\Software\Qt\6.11.0\msvc2022_64"
& 'C:\Software\CMake\bin\cmake.exe' --build build-engine-integration --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-engine-integration -C Debug --output-on-failure
.\build-engine-integration\Debug\PrivacyLensEngineSdkLoaderSmoke.exe --expect-loaded
```

Expected: configure/build succeed, CTest reports Engine-only tests passing, and loaded Engine SDK smoke passes.

### Task 4: Final Verification, Commits, Merge, Push

**Files:**
- Modify: `WORKLOG.md`
- Modify: main project `.spec/05-planning/project-log.md`
- Git: SDK and main repository branches

- [ ] **Step 1: Update worklogs**

Append a dated SDK entry to `WORKLOG.md` with files changed, behavior changes,
commands run, excluded artifacts, and deferred items.

Append a dated main-project entry to `.spec/05-planning/project-log.md` with
the old OCR loader cleanup and Engine SDK-only verification result.

- [ ] **Step 2: Check repository state**

Run in both repositories:

```powershell
git status --short --branch
git diff --stat
```

Expected: only intended cleanup, docs, and worklog changes are present. Local
untracked IDE folders and `tests/ScreenTest/` are not staged.

- [ ] **Step 3: Commit SDK cleanup**

Run in SDK repository:

```powershell
git add -A
git add -f docs/engine-sdk-architecture-cleanup-design.md docs/superpowers/plans/2026-06-18-engine-sdk-architecture-cleanup.md WORKLOG.md tests/README.md docs/architecture.md docs/backend-roadmap.md docs/dynamic-loading.md docs/artifact-policy.md
git commit -m "refactor: remove legacy ocr sdk surface"
```

- [ ] **Step 4: Commit main cleanup**

Run in main repository:

```powershell
git add -A
git commit -m "refactor: remove legacy ocr sdk integration"
```

- [ ] **Step 5: Merge SDK feature branch into main**

Run in SDK repository:

```powershell
git checkout main
git merge codex/engine-sdk-redaction-pipeline
& 'C:\Software\CMake\bin\cmake.exe' --build build-engine --config Debug
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-engine -C Debug --output-on-failure
```

Expected: merge succeeds and SDK tests still pass on `main`.

- [ ] **Step 6: Push requested repositories**

Run:

```powershell
git -C D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens-OCR-SDK push origin main
git -C D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens push origin main
```

Expected: both pushes succeed.
