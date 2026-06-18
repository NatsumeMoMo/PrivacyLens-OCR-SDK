#pragma once

#include "providers/app_window/visible_regions.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace plengine {

enum class Status {
    ok,
    invalid_argument,
    backend_unavailable,
    internal_error,
};

enum class SourceType {
    unknown,
    monitor,
    window,
    region,
};

enum class MaskSource {
    unknown,
    app_window,
    ocr,
    yolo,
    manual_region,
};

enum class MaskCategory {
    unknown,
    app_wechat,
    app_qq,
    secret_text,
    notification_toast,
};

enum class MaskStyle {
    solid,
    mosaic,
    blur,
};

struct PolicySnapshot {
    std::uint64_t generation = 0;
    bool mask_app_wechat = false;
};

struct FrameInfo {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t frame_id = 0;
    std::int64_t timestamp_ms = 0;
};

struct FrameContext {
    SourceType source_type = SourceType::unknown;
    Rect monitor_rect;
    Rect capture_rect;
    void* source_hwnd = nullptr;
};

struct Mask {
    std::uint64_t id = 0;
    Rect rect;
    MaskSource source = MaskSource::unknown;
    MaskCategory category = MaskCategory::unknown;
    MaskStyle style = MaskStyle::mosaic;
    float confidence = 0.0F;
    std::uint64_t policy_generation = 0;
    std::string reason_code;
};

struct MaskSnapshot {
    std::uint64_t policy_generation = 0;
    std::vector<Mask> masks;
};

}  // namespace plengine
