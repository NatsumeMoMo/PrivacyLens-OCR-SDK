# Tests

## Current Verification

Configure and build:

```powershell
& 'C:\Software\CMake\bin\cmake.exe' -S . -B build-engine -G "Visual Studio 18 2026" -A x64
& 'C:\Software\CMake\bin\cmake.exe' --build build-engine --config Debug
```

Run tests:

```powershell
& 'C:\Software\CMake\bin\ctest.exe' --test-dir build-engine -C Debug --output-on-failure
```

Run CLI smoke:

```powershell
.\build-engine\Debug\pl-engine-cli.exe --self-test --mask-wechat
```

## What It Covers

- `pl_engine_create` and `pl_engine_destroy`.
- `pl_engine_session_create` and `pl_engine_session_destroy`.
- Runtime policy copy through `pl_engine_session_update_policy`.
- Frame context submission through `pl_engine_session_submit_frame`.
- SDK-owned mask list materialization and destruction.
- Visible-region subtraction and coordinate mapping.
- WeChat provider mask creation from visible fragments.
- Privacy-safe CLI status output.

The old OCR adapter contract tests were removed with the OCR-only SDK ABI.
Future OCR coverage should be added through Engine provider tests.

## Privacy Rule

Tests and smoke commands must not log screenshots, frame pixels, OCR full text,
tokens, API keys, emails, phone numbers, or private repository content.

Real screenshots and raw OCR output remain in Atlas Artifacts or local ignored
test directories, not Git.
