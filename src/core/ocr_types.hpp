#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace plocr {

struct OcrEngineOptions {
    std::string requested_backend;
    std::string model_dir;
};

enum class PixelFormat {
    bgra8,
};

struct ImageView {
    const std::uint8_t* data = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride_bytes = 0;
    PixelFormat pixel_format = PixelFormat::bgra8;
};

struct OcrRequest {
    ImageView image;
};

struct BackendCapabilities {
    bool accepts_memory_input = false;
    bool requires_filesystem_input = false;
    bool returns_source_space_bbox = false;
    bool returns_source_space_quad = false;
    bool returns_confidence = false;
    bool supports_orientation = false;
    bool supports_line_boxes = false;
    bool supports_word_boxes = false;
};

struct Rect {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

struct Quad {
    std::array<float, 4> x{};
    std::array<float, 4> y{};
    bool has_quad = false;
};

struct Latency {
    double preprocess_ms = 0.0;
    double inference_ms = 0.0;
    double postprocess_ms = 0.0;
    double total_ms = 0.0;
};

struct OcrBox {
    std::string text_utf8;
    float confidence = 0.0F;
    Rect bbox;
    Quad quad;
};

struct BackendInfo {
    std::string name;
    std::string model_version;
    std::string runtime_version;
    std::string execution_provider;
    bool is_configured = false;
};

struct OcrResult {
    std::vector<OcrBox> boxes;
    Latency latency;
    BackendInfo backend;
};

}  // namespace plocr
