#include "pl_ocr.h"

#include "core/ocr_error.hpp"
#include "core/ocr_engine.hpp"

#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <string>
#include <vector>

struct pl_ocr_context {
    explicit pl_ocr_context(const plocr::OcrEngineOptions& options)
        : engine(options)
    {
    }

    plocr::OcrEngine engine;
    plocr::BackendInfo backend_info;
    pl_ocr_status last_status = PL_OCR_STATUS_OK;
    std::string last_error = "OK";
};

namespace {

thread_local pl_ocr_status g_last_status = PL_OCR_STATUS_OK;
thread_local std::string g_last_error = "OK";

struct ResultStorage {
    pl_ocr_result result{};
    std::vector<pl_ocr_box> boxes;
    std::vector<std::string> texts;
    std::string backend_name;
    std::string model_version;
};

void set_last_error(pl_ocr_context* context, pl_ocr_status status, const char* message)
{
    if (context != nullptr) {
        context->last_status = status;
        context->last_error = message;
        return;
    }

    g_last_status = status;
    g_last_error = message;
}

pl_ocr_status fail(pl_ocr_context* context, pl_ocr_status status, const char* message)
{
    set_last_error(context, status, message);
    return status;
}

void clear_error(pl_ocr_context* context)
{
    set_last_error(context, PL_OCR_STATUS_OK, "OK");
}

bool is_empty_or_stub(const char* value)
{
    return value == nullptr || value[0] == '\0' || std::strcmp(value, "stub") == 0;
}

pl_ocr_status map_error_code(plocr::ErrorCode code)
{
    switch (code) {
    case plocr::ErrorCode::invalid_argument:
        return PL_OCR_STATUS_INVALID_ARGUMENT;
    case plocr::ErrorCode::model_not_configured:
        return PL_OCR_STATUS_MODEL_NOT_CONFIGURED;
    case plocr::ErrorCode::backend_unavailable:
        return PL_OCR_STATUS_BACKEND_UNAVAILABLE;
    case plocr::ErrorCode::image_format_unsupported:
        return PL_OCR_STATUS_IMAGE_FORMAT_UNSUPPORTED;
    case plocr::ErrorCode::internal_error:
        return PL_OCR_STATUS_INTERNAL_ERROR;
    default:
        return PL_OCR_STATUS_INTERNAL_ERROR;
    }
}

uint32_t PL_OCR_CALL get_abi_version_impl()
{
    return PL_OCR_ABI_VERSION_V1;
}

uint32_t PL_OCR_CALL get_sdk_version_impl()
{
    return PL_OCR_SDK_VERSION;
}

const char* PL_OCR_CALL status_to_string_impl(pl_ocr_status status)
{
    switch (status) {
    case PL_OCR_STATUS_OK:
        return "OK";
    case PL_OCR_STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case PL_OCR_STATUS_UNSUPPORTED_ABI:
        return "unsupported ABI";
    case PL_OCR_STATUS_MODEL_NOT_CONFIGURED:
        return "model not configured";
    case PL_OCR_STATUS_BACKEND_UNAVAILABLE:
        return "backend unavailable";
    case PL_OCR_STATUS_IMAGE_FORMAT_UNSUPPORTED:
        return "image format unsupported";
    case PL_OCR_STATUS_INTERNAL_ERROR:
        return "internal error";
    default:
        return "unknown status";
    }
}

pl_ocr_status PL_OCR_CALL create_context_impl(const pl_ocr_context_options* options,
                                              pl_ocr_context** out_context)
{
    if (out_context == nullptr) {
        return fail(nullptr, PL_OCR_STATUS_INVALID_ARGUMENT, "out_context is null");
    }

    *out_context = nullptr;
    if (options != nullptr && options->struct_size != 0U &&
        options->struct_size < sizeof(pl_ocr_context_options)) {
        return fail(nullptr, PL_OCR_STATUS_INVALID_ARGUMENT, "context options struct is too small");
    }

    try {
        plocr::OcrEngineOptions engine_options;
        if (options != nullptr) {
            engine_options.requested_backend =
                options->requested_backend_utf8 != nullptr ? options->requested_backend_utf8 : "";
            engine_options.model_dir = options->model_dir_utf8 != nullptr ? options->model_dir_utf8 : "";
        }

        auto* context = new pl_ocr_context(engine_options);
        context->backend_info = context->engine.backend_info();
        clear_error(context);
        *out_context = context;
        return PL_OCR_STATUS_OK;
    } catch (const plocr::OcrError& ex) {
        return fail(nullptr, map_error_code(ex.code()), ex.what());
    } catch (const std::bad_alloc&) {
        return fail(nullptr, PL_OCR_STATUS_INTERNAL_ERROR, "context allocation failed");
    } catch (const std::exception& ex) {
        return fail(nullptr, PL_OCR_STATUS_INTERNAL_ERROR, ex.what());
    } catch (...) {
        return fail(nullptr, PL_OCR_STATUS_INTERNAL_ERROR, "unknown context creation failure");
    }
}

void PL_OCR_CALL destroy_context_impl(pl_ocr_context* context)
{
    delete context;
}

pl_ocr_status PL_OCR_CALL get_backend_info_impl(pl_ocr_context* context, pl_ocr_backend_info* out_info)
{
    if (context == nullptr) {
        return fail(nullptr, PL_OCR_STATUS_INVALID_ARGUMENT, "context is null");
    }
    if (out_info == nullptr) {
        return fail(context, PL_OCR_STATUS_INVALID_ARGUMENT, "out_info is null");
    }

    if (out_info->struct_size != 0U && out_info->struct_size < sizeof(pl_ocr_backend_info)) {
        return fail(context, PL_OCR_STATUS_INVALID_ARGUMENT, "backend info struct is too small");
    }

    std::memset(out_info, 0, sizeof(*out_info));
    out_info->struct_size = sizeof(*out_info);
    context->backend_info = context->engine.backend_info();
    out_info->backend_name_utf8 = context->backend_info.name.c_str();
    out_info->model_version_utf8 = context->backend_info.model_version.c_str();
    out_info->runtime_version_utf8 = context->backend_info.runtime_version.c_str();
    out_info->execution_provider_utf8 = context->backend_info.execution_provider.c_str();
    out_info->is_configured = context->backend_info.is_configured ? 1U : 0U;
    clear_error(context);
    return PL_OCR_STATUS_OK;
}

bool validate_image(const pl_ocr_image* image, pl_ocr_context* context)
{
    if (image == nullptr) {
        fail(context, PL_OCR_STATUS_INVALID_ARGUMENT, "image is null");
        return false;
    }
    if (image->struct_size != 0U && image->struct_size < sizeof(pl_ocr_image)) {
        fail(context, PL_OCR_STATUS_INVALID_ARGUMENT, "image struct is too small");
        return false;
    }
    if (image->data == nullptr) {
        fail(context, PL_OCR_STATUS_INVALID_ARGUMENT, "image data is null");
        return false;
    }
    if (image->width == 0U || image->height == 0U) {
        fail(context, PL_OCR_STATUS_INVALID_ARGUMENT, "image dimensions must be non-zero");
        return false;
    }
    if (image->pixel_format != PL_OCR_PIXEL_FORMAT_BGRA8) {
        fail(context, PL_OCR_STATUS_IMAGE_FORMAT_UNSUPPORTED, "only BGRA8 is supported");
        return false;
    }

    const auto min_stride = static_cast<std::uint64_t>(image->width) * 4ULL;
    if (min_stride > std::numeric_limits<std::uint32_t>::max() || image->stride_bytes < min_stride) {
        fail(context, PL_OCR_STATUS_INVALID_ARGUMENT, "stride is smaller than width * 4");
        return false;
    }

    return true;
}

void copy_quad(const plocr::Quad& source, pl_ocr_quad* target)
{
    for (std::size_t index = 0; index < 4U; ++index) {
        target->x[index] = source.x[index];
        target->y[index] = source.y[index];
    }
    target->has_quad = source.has_quad ? 1U : 0U;
}

pl_ocr_status materialize_result(const plocr::OcrResult& source, pl_ocr_result** out_result)
{
    auto* storage = new ResultStorage();
    storage->texts.reserve(source.boxes.size());
    storage->boxes.resize(source.boxes.size());
    storage->backend_name = source.backend.name;
    storage->model_version = source.backend.model_version;

    for (const auto& source_box : source.boxes) {
        storage->texts.push_back(source_box.text_utf8);
    }

    for (std::size_t index = 0; index < source.boxes.size(); ++index) {
        const auto& source_box = source.boxes[index];
        auto& target_box = storage->boxes[index];
        target_box.struct_size = sizeof(target_box);
        target_box.text_utf8 = storage->texts[index].c_str();
        target_box.text_bytes = static_cast<std::uint32_t>(storage->texts[index].size());
        target_box.confidence = source_box.confidence;
        target_box.bbox = {source_box.bbox.x,
                           source_box.bbox.y,
                           source_box.bbox.width,
                           source_box.bbox.height};
        copy_quad(source_box.quad, &target_box.quad);
    }

    storage->result.struct_size = sizeof(storage->result);
    storage->result.box_count = static_cast<std::uint32_t>(storage->boxes.size());
    storage->result.boxes = storage->boxes.empty() ? nullptr : storage->boxes.data();
    storage->result.latency = {source.latency.preprocess_ms,
                               source.latency.inference_ms,
                               source.latency.postprocess_ms,
                               source.latency.total_ms};
    storage->result.backend_name_utf8 = storage->backend_name.c_str();
    storage->result.model_version_utf8 = storage->model_version.c_str();
    storage->result.reserved_ptrs[0] = storage;

    *out_result = &storage->result;
    return PL_OCR_STATUS_OK;
}

pl_ocr_status PL_OCR_CALL recognize_image_impl(pl_ocr_context* context,
                                               const pl_ocr_image* image,
                                               pl_ocr_result** out_result)
{
    if (context == nullptr) {
        return fail(nullptr, PL_OCR_STATUS_INVALID_ARGUMENT, "context is null");
    }
    if (out_result == nullptr) {
        return fail(context, PL_OCR_STATUS_INVALID_ARGUMENT, "out_result is null");
    }

    *out_result = nullptr;
    if (!validate_image(image, context)) {
        return context->last_status;
    }

    try {
        const plocr::ImageView image_view{
            static_cast<const std::uint8_t*>(image->data),
            image->width,
            image->height,
            image->stride_bytes,
            plocr::PixelFormat::bgra8,
        };
        const auto result = context->engine.recognize(image_view);
        const auto status = materialize_result(result, out_result);
        clear_error(context);
        return status;
    } catch (const plocr::OcrError& ex) {
        return fail(context, map_error_code(ex.code()), ex.what());
    } catch (const std::bad_alloc&) {
        return fail(context, PL_OCR_STATUS_INTERNAL_ERROR, "result allocation failed");
    } catch (const std::exception& ex) {
        return fail(context, PL_OCR_STATUS_INTERNAL_ERROR, ex.what());
    } catch (...) {
        return fail(context, PL_OCR_STATUS_INTERNAL_ERROR, "unknown recognition failure");
    }
}

void PL_OCR_CALL destroy_result_impl(pl_ocr_result* result)
{
    if (result == nullptr) {
        return;
    }

    auto* storage = static_cast<ResultStorage*>(result->reserved_ptrs[0]);
    delete storage;
}

pl_ocr_status PL_OCR_CALL get_last_error_impl(pl_ocr_context* context, pl_ocr_error_info* out_error)
{
    if (out_error == nullptr) {
        return PL_OCR_STATUS_INVALID_ARGUMENT;
    }
    if (out_error->struct_size != 0U && out_error->struct_size < sizeof(pl_ocr_error_info)) {
        return PL_OCR_STATUS_INVALID_ARGUMENT;
    }

    const pl_ocr_status status = context != nullptr ? context->last_status : g_last_status;
    const std::string& message = context != nullptr ? context->last_error : g_last_error;

    std::memset(out_error, 0, sizeof(*out_error));
    out_error->struct_size = sizeof(*out_error);
    out_error->status = status;
    out_error->message_utf8 = message.c_str();
    out_error->message_bytes = static_cast<std::uint32_t>(message.size());
    return PL_OCR_STATUS_OK;
}

void fill_api(pl_ocr_api_v1* out_api)
{
    std::memset(out_api, 0, sizeof(*out_api));
    out_api->abi_version = PL_OCR_ABI_VERSION_V1;
    out_api->struct_size = sizeof(*out_api);
    out_api->sdk_version = PL_OCR_SDK_VERSION;
    out_api->get_abi_version = &get_abi_version_impl;
    out_api->get_sdk_version = &get_sdk_version_impl;
    out_api->create_context = &create_context_impl;
    out_api->destroy_context = &destroy_context_impl;
    out_api->get_backend_info = &get_backend_info_impl;
    out_api->recognize_image = &recognize_image_impl;
    out_api->destroy_result = &destroy_result_impl;
    out_api->get_last_error = &get_last_error_impl;
    out_api->status_to_string = &status_to_string_impl;
}

}  // namespace

extern "C" PL_OCR_EXPORT pl_ocr_status PL_OCR_CALL pl_ocr_get_api(uint32_t requested_version,
                                                                  pl_ocr_api_v1* out_api)
{
    if (out_api == nullptr) {
        return PL_OCR_STATUS_INVALID_ARGUMENT;
    }

    std::memset(out_api, 0, sizeof(*out_api));
    if (requested_version != PL_OCR_ABI_VERSION_V1) {
        return PL_OCR_STATUS_UNSUPPORTED_ABI;
    }

    fill_api(out_api);
    return PL_OCR_STATUS_OK;
}
