#ifndef PL_ENGINE_H
#define PL_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define PL_ENGINE_CALL __cdecl
#if defined(PL_ENGINE_BUILD_DLL)
#define PL_ENGINE_EXPORT __declspec(dllexport)
#elif defined(PL_ENGINE_NO_IMPORT)
#define PL_ENGINE_EXPORT
#else
#define PL_ENGINE_EXPORT __declspec(dllimport)
#endif
#else
#define PL_ENGINE_CALL
#define PL_ENGINE_EXPORT
#endif

#define PL_ENGINE_MAKE_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 24) | (((uint32_t)(minor)) << 16) | ((uint32_t)(patch)))
#define PL_ENGINE_SDK_VERSION PL_ENGINE_MAKE_VERSION(0u, 1u, 0u)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pl_engine pl_engine;
typedef struct pl_engine_session pl_engine_session;
typedef struct pl_engine_mask_list pl_engine_mask_list;

typedef enum pl_engine_status {
    PL_ENGINE_STATUS_OK = 0,
    PL_ENGINE_STATUS_INVALID_ARGUMENT = 1,
    PL_ENGINE_STATUS_BACKEND_UNAVAILABLE = 2,
    PL_ENGINE_STATUS_INTERNAL_ERROR = 3
} pl_engine_status;

typedef enum pl_engine_pixel_format {
    PL_ENGINE_PIXEL_FORMAT_UNKNOWN = 0,
    PL_ENGINE_PIXEL_FORMAT_BGRA8 = 1,
    PL_ENGINE_PIXEL_FORMAT_RGBA8 = 2
} pl_engine_pixel_format;

typedef enum pl_engine_source_type {
    PL_ENGINE_SOURCE_UNKNOWN = 0,
    PL_ENGINE_SOURCE_MONITOR = 1,
    PL_ENGINE_SOURCE_WINDOW = 2,
    PL_ENGINE_SOURCE_REGION = 3
} pl_engine_source_type;

typedef enum pl_engine_mask_source {
    PL_ENGINE_MASK_SOURCE_UNKNOWN = 0,
    PL_ENGINE_MASK_SOURCE_APP_WINDOW = 1,
    PL_ENGINE_MASK_SOURCE_OCR = 2,
    PL_ENGINE_MASK_SOURCE_YOLO = 3,
    PL_ENGINE_MASK_SOURCE_MANUAL_REGION = 4
} pl_engine_mask_source;

typedef enum pl_engine_mask_category {
    PL_ENGINE_MASK_CATEGORY_UNKNOWN = 0,
    PL_ENGINE_MASK_CATEGORY_APP_WECHAT = 1,
    PL_ENGINE_MASK_CATEGORY_APP_QQ = 2,
    PL_ENGINE_MASK_CATEGORY_SECRET_TEXT = 100,
    PL_ENGINE_MASK_CATEGORY_NOTIFICATION_TOAST = 200
} pl_engine_mask_category;

typedef enum pl_engine_mask_style {
    PL_ENGINE_MASK_STYLE_SOLID = 0,
    PL_ENGINE_MASK_STYLE_MOSAIC = 1,
    PL_ENGINE_MASK_STYLE_BLUR = 2
} pl_engine_mask_style;

typedef struct pl_engine_rect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} pl_engine_rect;

typedef struct pl_engine_config {
    uint32_t struct_size;
    uint32_t flags;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
} pl_engine_config;

typedef struct pl_engine_session_config {
    uint32_t struct_size;
    uint32_t flags;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
} pl_engine_session_config;

typedef struct pl_engine_policy {
    uint32_t struct_size;
    uint32_t flags;
    uint64_t policy_generation;
    uint32_t mask_app_wechat;
    uint32_t reserved32[7];
    uint64_t reserved64[8];
    void* reserved_ptrs[4];
} pl_engine_policy;

typedef struct pl_engine_frame {
    uint32_t struct_size;
    const void* data;
    uint32_t width;
    uint32_t height;
    uint32_t stride_bytes;
    pl_engine_pixel_format pixel_format;
    uint64_t frame_id;
    int64_t timestamp_ms;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
} pl_engine_frame;

typedef struct pl_engine_frame_context {
    uint32_t struct_size;
    pl_engine_source_type source_type;
    pl_engine_rect monitor_rect;
    pl_engine_rect capture_rect;
    void* source_hwnd;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
} pl_engine_frame_context;

typedef struct pl_engine_mask {
    uint32_t struct_size;
    uint64_t id;
    pl_engine_rect rect;
    pl_engine_mask_source source;
    pl_engine_mask_category category;
    pl_engine_mask_style style;
    float confidence;
    uint64_t policy_generation;
    const char* reason_code_utf8;
    uint32_t reason_code_bytes;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
} pl_engine_mask;

struct pl_engine_mask_list {
    uint32_t struct_size;
    uint32_t mask_count;
    const pl_engine_mask* masks;
    uint64_t policy_generation;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
};

typedef struct pl_engine_error_info {
    uint32_t struct_size;
    pl_engine_status status;
    const char* message_utf8;
    uint32_t message_bytes;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
} pl_engine_error_info;

typedef pl_engine_status(PL_ENGINE_CALL* pl_engine_create_fn)(const pl_engine_config* config,
                                                              pl_engine** out_engine);
typedef void(PL_ENGINE_CALL* pl_engine_destroy_fn)(pl_engine* engine);
typedef pl_engine_status(PL_ENGINE_CALL* pl_engine_session_create_fn)(
    pl_engine* engine,
    const pl_engine_session_config* config,
    pl_engine_session** out_session);
typedef void(PL_ENGINE_CALL* pl_engine_session_destroy_fn)(pl_engine_session* session);
typedef pl_engine_status(PL_ENGINE_CALL* pl_engine_session_update_policy_fn)(
    pl_engine_session* session,
    const pl_engine_policy* policy);
typedef pl_engine_status(PL_ENGINE_CALL* pl_engine_session_submit_frame_fn)(
    pl_engine_session* session,
    const pl_engine_frame* frame,
    const pl_engine_frame_context* context);
typedef pl_engine_status(PL_ENGINE_CALL* pl_engine_session_get_latest_masks_fn)(
    pl_engine_session* session,
    pl_engine_mask_list** out_masks);
typedef void(PL_ENGINE_CALL* pl_engine_mask_list_destroy_fn)(pl_engine_mask_list* masks);
typedef pl_engine_status(PL_ENGINE_CALL* pl_engine_get_last_error_fn)(
    pl_engine_session* session,
    pl_engine_error_info* out_error);
typedef const char*(PL_ENGINE_CALL* pl_engine_status_to_string_fn)(pl_engine_status status);

PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_create(const pl_engine_config* config,
                                                                  pl_engine** out_engine);
PL_ENGINE_EXPORT void PL_ENGINE_CALL pl_engine_destroy(pl_engine* engine);

PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_session_create(
    pl_engine* engine,
    const pl_engine_session_config* config,
    pl_engine_session** out_session);
PL_ENGINE_EXPORT void PL_ENGINE_CALL pl_engine_session_destroy(pl_engine_session* session);

PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_session_update_policy(
    pl_engine_session* session,
    const pl_engine_policy* policy);
PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_session_submit_frame(
    pl_engine_session* session,
    const pl_engine_frame* frame,
    const pl_engine_frame_context* context);
PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_session_get_latest_masks(
    pl_engine_session* session,
    pl_engine_mask_list** out_masks);
PL_ENGINE_EXPORT void PL_ENGINE_CALL pl_engine_mask_list_destroy(pl_engine_mask_list* masks);

PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_get_last_error(
    pl_engine_session* session,
    pl_engine_error_info* out_error);
PL_ENGINE_EXPORT const char* PL_ENGINE_CALL pl_engine_status_to_string(pl_engine_status status);

#ifdef __cplusplus
}
#endif

#endif
