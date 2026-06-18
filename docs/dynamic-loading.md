# Dynamic Loading

## Purpose

Describe how the PrivacyLens main project loads the Engine SDK DLL at runtime.
The current SDK uses direct exported C functions. The previous OCR-only
`pl_ocr_get_api` function table has been removed from the supported SDK surface.

## DLL And Header

Current development output:

```text
D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens-OCR-SDK\build-engine\Debug\PrivacyLensEngine.dll
```

Public header:

```text
include/pl_engine.h
```

When compiling a runtime loader that does not link against the import library,
define `PL_ENGINE_NO_IMPORT` before including `pl_engine.h`.

## Required Exports

The main project should resolve these symbols:

```text
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

All are direct C exports. There is no exported function table in this Engine
SDK pass, and no `PrivacyLensOcr.dll` compatibility export remains.

## Windows Loading Sketch

```cpp
#define PL_ENGINE_NO_IMPORT
#include "pl_engine.h"

#include <windows.h>

HMODULE module = LoadLibraryW(dll_path.c_str());
if (module == nullptr) {
    // Degrade to engine-unavailable mode.
}

auto create_engine = reinterpret_cast<pl_engine_create_fn>(
    GetProcAddress(module, "pl_engine_create"));
auto destroy_engine = reinterpret_cast<pl_engine_destroy_fn>(
    GetProcAddress(module, "pl_engine_destroy"));

if (create_engine == nullptr || destroy_engine == nullptr) {
    FreeLibrary(module);
    // Degrade to incompatible-SDK mode.
}
```

After resolving all required exports, the main project creates one engine and
one session for the active preview pipeline:

```cpp
pl_engine_config config{};
config.struct_size = sizeof(config);
config.sdk_version = PL_ENGINE_SDK_VERSION;

pl_engine* engine = nullptr;
pl_engine_status status = create_engine(&config, &engine);
```

## Runtime Policy

Persistent user settings stay in the PrivacyLens main app. The loader pushes a
copied policy snapshot whenever the UI setting changes:

```cpp
pl_engine_policy policy{};
policy.struct_size = sizeof(policy);
policy.policy_generation = next_generation;
policy.mask_app_wechat = 1;

update_policy(session, &policy);
```

The SDK should not watch or parse the main app's settings file.

## Suggested Search Order

Proposed search order for development:

1. App directory beside `PrivacyLens.exe`.
2. User-configured Engine SDK directory.
3. Environment variable `PRIVACYLENS_ENGINE_SDK_DIR`.
4. Local SDK build output:

```text
D:\Atlas\Projects\Computer\AI-Infra\PrivacyLens-OCR-SDK\build-engine\Debug
```

## Missing SDK Behavior

If the DLL is missing, incompatible, or cannot create a session:

- Keep capture and preview available.
- Mark Engine SDK status as unavailable.
- Do not claim app-level masking is active.
- Keep user settings intact so the policy can be applied after the SDK loads.
- Log only privacy-safe status, such as `engine_sdk_missing`,
  `engine_sdk_incompatible`, or `engine_session_unavailable`.

## Ownership Notes

The main project must copy mask rectangles and reason strings before calling
`pl_engine_mask_list_destroy`. It must never free SDK-owned arrays or strings
directly.

## Deferred

- Signed package verification.
- Hot reload.
- Multi-provider diagnostics UI.
- Packaged SDK search path policy.
