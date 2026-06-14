#ifndef PL_OCR_H
#define PL_OCR_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define PL_OCR_CALL __cdecl
#if defined(PL_OCR_BUILD_DLL)
#define PL_OCR_EXPORT __declspec(dllexport)
#elif defined(PL_OCR_NO_IMPORT)
#define PL_OCR_EXPORT
#else
#define PL_OCR_EXPORT __declspec(dllimport)
#endif
#else
#define PL_OCR_CALL
#define PL_OCR_EXPORT
#endif

#define PL_OCR_ABI_VERSION_V1 1u
#define PL_OCR_MAKE_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 24) | (((uint32_t)(minor)) << 16) | ((uint32_t)(patch)))
#define PL_OCR_SDK_VERSION PL_OCR_MAKE_VERSION(0u, 1u, 0u)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pl_ocr_context pl_ocr_context;
typedef struct pl_ocr_result pl_ocr_result;

typedef enum pl_ocr_status {
    PL_OCR_STATUS_OK = 0,
    PL_OCR_STATUS_INVALID_ARGUMENT = 1,
    PL_OCR_STATUS_UNSUPPORTED_ABI = 2,
    PL_OCR_STATUS_MODEL_NOT_CONFIGURED = 3,
    PL_OCR_STATUS_BACKEND_UNAVAILABLE = 4,
    PL_OCR_STATUS_IMAGE_FORMAT_UNSUPPORTED = 5,
    PL_OCR_STATUS_INTERNAL_ERROR = 6
} pl_ocr_status;

typedef enum pl_ocr_pixel_format {
    PL_OCR_PIXEL_FORMAT_UNKNOWN = 0,
    PL_OCR_PIXEL_FORMAT_BGRA8 = 1
} pl_ocr_pixel_format;

typedef struct pl_ocr_context_options {
    uint32_t struct_size;
    const char* requested_backend_utf8;
    const char* model_dir_utf8;
    uint32_t flags;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
} pl_ocr_context_options;

typedef struct pl_ocr_image {
    uint32_t struct_size;
    const void* data;
    uint32_t width;
    uint32_t height;
    uint32_t stride_bytes;
    pl_ocr_pixel_format pixel_format;
    uint64_t timestamp_ns;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
} pl_ocr_image;

typedef struct pl_ocr_rect {
    float x;
    float y;
    float width;
    float height;
} pl_ocr_rect;

typedef struct pl_ocr_quad {
    float x[4];
    float y[4];
    uint32_t has_quad;
    uint32_t reserved[3];
} pl_ocr_quad;

typedef struct pl_ocr_latency {
    double preprocess_ms;
    double inference_ms;
    double postprocess_ms;
    double total_ms;
} pl_ocr_latency;

typedef struct pl_ocr_box {
    uint32_t struct_size;
    const char* text_utf8;
    uint32_t text_bytes;
    float confidence;
    pl_ocr_rect bbox;
    pl_ocr_quad quad;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
} pl_ocr_box;

struct pl_ocr_result {
    uint32_t struct_size;
    uint32_t box_count;
    const pl_ocr_box* boxes;
    pl_ocr_latency latency;
    const char* backend_name_utf8;
    const char* model_version_utf8;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
};

typedef struct pl_ocr_backend_info {
    uint32_t struct_size;
    const char* backend_name_utf8;
    const char* model_version_utf8;
    const char* runtime_version_utf8;
    const char* execution_provider_utf8;
    uint32_t is_configured;
    uint32_t reserved32[7];
    uint64_t reserved64[8];
    void* reserved_ptrs[4];
} pl_ocr_backend_info;

typedef struct pl_ocr_error_info {
    uint32_t struct_size;
    pl_ocr_status status;
    const char* message_utf8;
    uint32_t message_bytes;
    uint64_t reserved[8];
    void* reserved_ptrs[4];
} pl_ocr_error_info;

typedef struct pl_ocr_api_v1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t sdk_version;
    uint32_t reserved0;

    uint32_t(PL_OCR_CALL* get_abi_version)(void);
    uint32_t(PL_OCR_CALL* get_sdk_version)(void);
    pl_ocr_status(PL_OCR_CALL* create_context)(const pl_ocr_context_options* options,
                                               pl_ocr_context** out_context);
    void(PL_OCR_CALL* destroy_context)(pl_ocr_context* context);
    pl_ocr_status(PL_OCR_CALL* get_backend_info)(pl_ocr_context* context,
                                                 pl_ocr_backend_info* out_info);
    pl_ocr_status(PL_OCR_CALL* recognize_image)(pl_ocr_context* context,
                                                const pl_ocr_image* image,
                                                pl_ocr_result** out_result);
    void(PL_OCR_CALL* destroy_result)(pl_ocr_result* result);
    pl_ocr_status(PL_OCR_CALL* get_last_error)(pl_ocr_context* context,
                                               pl_ocr_error_info* out_error);
    const char*(PL_OCR_CALL* status_to_string)(pl_ocr_status status);

    uint64_t reserved[8];
    void* reserved_ptrs[8];
} pl_ocr_api_v1;

typedef pl_ocr_status(PL_OCR_CALL* pl_ocr_get_api_fn)(uint32_t requested_version,
                                                      pl_ocr_api_v1* out_api);

PL_OCR_EXPORT pl_ocr_status PL_OCR_CALL pl_ocr_get_api(uint32_t requested_version,
                                                       pl_ocr_api_v1* out_api);

#ifdef __cplusplus
}
#endif

#endif
