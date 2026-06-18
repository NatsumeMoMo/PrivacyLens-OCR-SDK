#include "pl_engine.h"

#include "core/engine_error.hpp"
#include "core/engine_session.hpp"

#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <vector>

struct pl_engine {
    plengine::Engine engine;
};

struct pl_engine_session {
    explicit pl_engine_session(pl_engine* owner)
        : engine(owner)
    {
    }

    pl_engine* engine = nullptr;
    plengine::EngineSession session;
};

namespace {

thread_local pl_engine_status g_last_status = PL_ENGINE_STATUS_OK;
thread_local std::string g_last_error = "OK";

struct MaskListStorage {
    pl_engine_mask_list list{};
    std::vector<pl_engine_mask> masks;
    std::vector<std::string> reason_codes;
};

pl_engine_status map_status(plengine::Status status)
{
    switch (status) {
    case plengine::Status::ok:
        return PL_ENGINE_STATUS_OK;
    case plengine::Status::invalid_argument:
        return PL_ENGINE_STATUS_INVALID_ARGUMENT;
    case plengine::Status::backend_unavailable:
        return PL_ENGINE_STATUS_BACKEND_UNAVAILABLE;
    case plengine::Status::internal_error:
        return PL_ENGINE_STATUS_INTERNAL_ERROR;
    default:
        return PL_ENGINE_STATUS_INTERNAL_ERROR;
    }
}

plengine::Status map_status(pl_engine_status status)
{
    switch (status) {
    case PL_ENGINE_STATUS_OK:
        return plengine::Status::ok;
    case PL_ENGINE_STATUS_INVALID_ARGUMENT:
        return plengine::Status::invalid_argument;
    case PL_ENGINE_STATUS_BACKEND_UNAVAILABLE:
        return plengine::Status::backend_unavailable;
    case PL_ENGINE_STATUS_INTERNAL_ERROR:
        return plengine::Status::internal_error;
    default:
        return plengine::Status::internal_error;
    }
}

void set_last_error(pl_engine_session* session, pl_engine_status status, const char* message)
{
    if (session != nullptr) {
        session->session.set_error(map_status(status), message);
        return;
    }
    g_last_status = status;
    g_last_error = message;
}

pl_engine_status fail(pl_engine_session* session, pl_engine_status status, const char* message)
{
    set_last_error(session, status, message);
    return status;
}

bool validate_struct_size(std::uint32_t actual, std::size_t expected)
{
    return actual == 0U || actual >= expected;
}

plengine::Rect to_rect(const pl_engine_rect& rect)
{
    return {rect.x, rect.y, rect.width, rect.height};
}

pl_engine_rect to_rect(const plengine::Rect& rect)
{
    return {rect.x, rect.y, rect.width, rect.height};
}

plengine::SourceType to_source_type(pl_engine_source_type source)
{
    switch (source) {
    case PL_ENGINE_SOURCE_MONITOR:
        return plengine::SourceType::monitor;
    case PL_ENGINE_SOURCE_WINDOW:
        return plengine::SourceType::window;
    case PL_ENGINE_SOURCE_REGION:
        return plengine::SourceType::region;
    default:
        return plengine::SourceType::unknown;
    }
}

pl_engine_mask_source to_mask_source(plengine::MaskSource source)
{
    switch (source) {
    case plengine::MaskSource::app_window:
        return PL_ENGINE_MASK_SOURCE_APP_WINDOW;
    case plengine::MaskSource::ocr:
        return PL_ENGINE_MASK_SOURCE_OCR;
    case plengine::MaskSource::yolo:
        return PL_ENGINE_MASK_SOURCE_YOLO;
    case plengine::MaskSource::manual_region:
        return PL_ENGINE_MASK_SOURCE_MANUAL_REGION;
    default:
        return PL_ENGINE_MASK_SOURCE_UNKNOWN;
    }
}

pl_engine_mask_category to_mask_category(plengine::MaskCategory category)
{
    switch (category) {
    case plengine::MaskCategory::app_wechat:
        return PL_ENGINE_MASK_CATEGORY_APP_WECHAT;
    case plengine::MaskCategory::app_qq:
        return PL_ENGINE_MASK_CATEGORY_APP_QQ;
    case plengine::MaskCategory::secret_text:
        return PL_ENGINE_MASK_CATEGORY_SECRET_TEXT;
    case plengine::MaskCategory::notification_toast:
        return PL_ENGINE_MASK_CATEGORY_NOTIFICATION_TOAST;
    default:
        return PL_ENGINE_MASK_CATEGORY_UNKNOWN;
    }
}

pl_engine_mask_style to_mask_style(plengine::MaskStyle style)
{
    switch (style) {
    case plengine::MaskStyle::solid:
        return PL_ENGINE_MASK_STYLE_SOLID;
    case plengine::MaskStyle::mosaic:
        return PL_ENGINE_MASK_STYLE_MOSAIC;
    case plengine::MaskStyle::blur:
        return PL_ENGINE_MASK_STYLE_BLUR;
    default:
        return PL_ENGINE_MASK_STYLE_MOSAIC;
    }
}

pl_engine_status materialize_masks(const plengine::MaskSnapshot& source, pl_engine_mask_list** out_masks)
{
    auto* storage = new MaskListStorage();
    storage->list.struct_size = sizeof(storage->list);
    storage->list.policy_generation = source.policy_generation;
    storage->masks.resize(source.masks.size());
    storage->reason_codes.reserve(source.masks.size());

    for (const auto& source_mask : source.masks) {
        storage->reason_codes.push_back(source_mask.reason_code);
    }

    for (std::size_t index = 0; index < source.masks.size(); ++index) {
        const auto& source_mask = source.masks[index];
        auto& target = storage->masks[index];
        target.struct_size = sizeof(target);
        target.id = source_mask.id;
        target.rect = to_rect(source_mask.rect);
        target.source = to_mask_source(source_mask.source);
        target.category = to_mask_category(source_mask.category);
        target.style = to_mask_style(source_mask.style);
        target.confidence = source_mask.confidence;
        target.policy_generation = source_mask.policy_generation;
        target.reason_code_utf8 = storage->reason_codes[index].c_str();
        target.reason_code_bytes = static_cast<std::uint32_t>(storage->reason_codes[index].size());
    }

    storage->list.mask_count = static_cast<std::uint32_t>(storage->masks.size());
    storage->list.masks = storage->masks.empty() ? nullptr : storage->masks.data();
    storage->list.reserved_ptrs[0] = storage;
    *out_masks = &storage->list;
    return PL_ENGINE_STATUS_OK;
}

}  // namespace

extern "C" PL_ENGINE_EXPORT const char* PL_ENGINE_CALL pl_engine_status_to_string(pl_engine_status status)
{
    switch (status) {
    case PL_ENGINE_STATUS_OK:
        return "OK";
    case PL_ENGINE_STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case PL_ENGINE_STATUS_BACKEND_UNAVAILABLE:
        return "backend unavailable";
    case PL_ENGINE_STATUS_INTERNAL_ERROR:
        return "internal error";
    default:
        return "unknown status";
    }
}

extern "C" PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_create(const pl_engine_config* config,
                                                                             pl_engine** out_engine)
{
    if (out_engine == nullptr) {
        return fail(nullptr, PL_ENGINE_STATUS_INVALID_ARGUMENT, "out_engine is null");
    }
    *out_engine = nullptr;
    if (config != nullptr && !validate_struct_size(config->struct_size, sizeof(pl_engine_config))) {
        return fail(nullptr, PL_ENGINE_STATUS_INVALID_ARGUMENT, "engine config struct is too small");
    }

    try {
        *out_engine = new pl_engine();
        set_last_error(nullptr, PL_ENGINE_STATUS_OK, "OK");
        return PL_ENGINE_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return fail(nullptr, PL_ENGINE_STATUS_INTERNAL_ERROR, "engine allocation failed");
    } catch (...) {
        return fail(nullptr, PL_ENGINE_STATUS_INTERNAL_ERROR, "unknown engine creation failure");
    }
}

extern "C" PL_ENGINE_EXPORT void PL_ENGINE_CALL pl_engine_destroy(pl_engine* engine)
{
    delete engine;
}

extern "C" PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_session_create(
    pl_engine* engine,
    const pl_engine_session_config* config,
    pl_engine_session** out_session)
{
    if (engine == nullptr) {
        return fail(nullptr, PL_ENGINE_STATUS_INVALID_ARGUMENT, "engine is null");
    }
    if (out_session == nullptr) {
        return fail(nullptr, PL_ENGINE_STATUS_INVALID_ARGUMENT, "out_session is null");
    }
    *out_session = nullptr;
    if (config != nullptr && !validate_struct_size(config->struct_size, sizeof(pl_engine_session_config))) {
        return fail(nullptr, PL_ENGINE_STATUS_INVALID_ARGUMENT, "session config struct is too small");
    }

    try {
        *out_session = new pl_engine_session(engine);
        (*out_session)->session.clear_error();
        return PL_ENGINE_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return fail(nullptr, PL_ENGINE_STATUS_INTERNAL_ERROR, "session allocation failed");
    } catch (...) {
        return fail(nullptr, PL_ENGINE_STATUS_INTERNAL_ERROR, "unknown session creation failure");
    }
}

extern "C" PL_ENGINE_EXPORT void PL_ENGINE_CALL pl_engine_session_destroy(pl_engine_session* session)
{
    delete session;
}

extern "C" PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_session_update_policy(
    pl_engine_session* session,
    const pl_engine_policy* policy)
{
    if (session == nullptr) {
        return fail(nullptr, PL_ENGINE_STATUS_INVALID_ARGUMENT, "session is null");
    }
    if (policy == nullptr) {
        return fail(session, PL_ENGINE_STATUS_INVALID_ARGUMENT, "policy is null");
    }
    if (!validate_struct_size(policy->struct_size, sizeof(pl_engine_policy))) {
        return fail(session, PL_ENGINE_STATUS_INVALID_ARGUMENT, "policy struct is too small");
    }

    plengine::PolicySnapshot snapshot;
    snapshot.generation = policy->policy_generation;
    snapshot.mask_app_wechat = policy->mask_app_wechat != 0U;
    session->session.update_policy(snapshot);
    return PL_ENGINE_STATUS_OK;
}

extern "C" PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_session_submit_frame(
    pl_engine_session* session,
    const pl_engine_frame* frame,
    const pl_engine_frame_context* context)
{
    if (session == nullptr) {
        return fail(nullptr, PL_ENGINE_STATUS_INVALID_ARGUMENT, "session is null");
    }
    if (frame == nullptr) {
        return fail(session, PL_ENGINE_STATUS_INVALID_ARGUMENT, "frame is null");
    }
    if (context == nullptr) {
        return fail(session, PL_ENGINE_STATUS_INVALID_ARGUMENT, "frame context is null");
    }
    if (!validate_struct_size(frame->struct_size, sizeof(pl_engine_frame))) {
        return fail(session, PL_ENGINE_STATUS_INVALID_ARGUMENT, "frame struct is too small");
    }
    if (!validate_struct_size(context->struct_size, sizeof(pl_engine_frame_context))) {
        return fail(session, PL_ENGINE_STATUS_INVALID_ARGUMENT, "frame context struct is too small");
    }
    if (frame->width == 0U || frame->height == 0U) {
        return fail(session, PL_ENGINE_STATUS_INVALID_ARGUMENT, "frame dimensions must be non-zero");
    }

    plengine::FrameInfo frame_info;
    frame_info.width = frame->width;
    frame_info.height = frame->height;
    frame_info.frame_id = frame->frame_id;
    frame_info.timestamp_ms = frame->timestamp_ms;

    plengine::FrameContext frame_context;
    frame_context.source_type = to_source_type(context->source_type);
    frame_context.monitor_rect = to_rect(context->monitor_rect);
    frame_context.capture_rect = to_rect(context->capture_rect);
    frame_context.source_hwnd = context->source_hwnd;
    session->session.submit_frame(frame_info, frame_context);
    return PL_ENGINE_STATUS_OK;
}

extern "C" PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_session_get_latest_masks(
    pl_engine_session* session,
    pl_engine_mask_list** out_masks)
{
    if (session == nullptr) {
        return fail(nullptr, PL_ENGINE_STATUS_INVALID_ARGUMENT, "session is null");
    }
    if (out_masks == nullptr) {
        return fail(session, PL_ENGINE_STATUS_INVALID_ARGUMENT, "out_masks is null");
    }
    *out_masks = nullptr;

    try {
        const auto snapshot = session->session.latest_masks();
        const auto status = materialize_masks(snapshot, out_masks);
        session->session.clear_error();
        return status;
    } catch (const std::bad_alloc&) {
        return fail(session, PL_ENGINE_STATUS_INTERNAL_ERROR, "mask list allocation failed");
    } catch (const std::exception& ex) {
        return fail(session, PL_ENGINE_STATUS_INTERNAL_ERROR, ex.what());
    } catch (...) {
        return fail(session, PL_ENGINE_STATUS_INTERNAL_ERROR, "unknown mask query failure");
    }
}

extern "C" PL_ENGINE_EXPORT void PL_ENGINE_CALL pl_engine_mask_list_destroy(pl_engine_mask_list* masks)
{
    if (masks == nullptr) {
        return;
    }

    auto* storage = static_cast<MaskListStorage*>(masks->reserved_ptrs[0]);
    delete storage;
}

extern "C" PL_ENGINE_EXPORT pl_engine_status PL_ENGINE_CALL pl_engine_get_last_error(
    pl_engine_session* session,
    pl_engine_error_info* out_error)
{
    if (out_error == nullptr) {
        return PL_ENGINE_STATUS_INVALID_ARGUMENT;
    }
    if (!validate_struct_size(out_error->struct_size, sizeof(pl_engine_error_info))) {
        return PL_ENGINE_STATUS_INVALID_ARGUMENT;
    }

    const pl_engine_status status =
        session != nullptr ? map_status(session->session.last_status()) : g_last_status;
    const std::string& message = session != nullptr ? session->session.last_error() : g_last_error;

    std::memset(out_error, 0, sizeof(*out_error));
    out_error->struct_size = sizeof(*out_error);
    out_error->status = status;
    out_error->message_utf8 = message.c_str();
    out_error->message_bytes = static_cast<std::uint32_t>(message.size());
    return PL_ENGINE_STATUS_OK;
}
