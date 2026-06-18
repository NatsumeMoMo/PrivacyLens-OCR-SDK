# Artifact Policy

## Purpose

Keep the Engine SDK repository lightweight, auditable, and safe to share without
leaking runtime packages, model assets, private screen content, or generated
binaries.

## Do Not Commit

- OCR, YOLO, or other ML model files.
- ONNX Runtime, RapidOCR, OpenCV, CUDA, or other binary runtime packages.
- Large DLLs, import libraries, PDBs, EXEs, generated build trees, or package
  outputs.
- Real screenshots.
- Real screen recordings.
- Raw OCR output or full text from user screens.
- Tokens, API keys, emails, phone numbers, or private repository content.

## Git-Allowed Content

- Source code.
- Public C ABI headers.
- CMake files.
- Top-level README and intentionally tracked small tests.
- Small synthetic fixtures, if added later.
- Privacy-safe CLI/status output.

The current repository `.gitignore` intentionally keeps `docs/`, `WORKLOG.md`,
and `tests/README.md` as local handoff material rather than tracked SDK source.

## Recommended Atlas Artifact Paths

```text
D:\Atlas\Artifacts\Models\PrivacyLens-OCR-SDK\...
D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\...
D:\Atlas\Artifacts\TestData\PrivacyLens-OCR-SDK\...
```

These paths are still valid for future OCR/YOLO providers even though the first
Engine SDK pipeline does not require model artifacts.

## Manifest Requirement

Every real provider artifact set must record:

- Artifact name.
- Source URL or internal source.
- Version.
- Checksum.
- License.
- Restore command or manual restore steps.
- Expected local path.
- Whether redistribution is allowed.

Historical OCR spike manifests can remain as local reference material, but they
do not define the current Engine SDK packaging surface. The old OCR source tree
and `pl_ocr` ABI are no longer part of the active SDK.

## Current Engine SDK Artifacts

The WeChat app-window provider has no model or third-party runtime artifacts.
It depends on Windows user32 and DWM APIs.

Current generated build outputs stay out of Git:

```text
build-engine\Debug\PrivacyLensEngine.dll
build-engine\Debug\PrivacyLensEngine.lib
build-engine\Debug\pl-engine-cli.exe
```

## Future Runtime Packaging

Proposed future package shape:

```text
PrivacyLens-Engine-SDK/
  include/pl_engine.h
  bin/PrivacyLensEngine.dll
  lib/PrivacyLensEngine.lib
  manifests/
    runtime-manifest.md
    provider-manifest.md
```

## Privacy Rule

Real screenshots and real screen-derived text stay out of Git even if they are
useful for debugging. Use synthetic samples first. If real samples are needed,
store them only under Atlas Artifacts with explicit handling notes and do not
include raw text in logs.
