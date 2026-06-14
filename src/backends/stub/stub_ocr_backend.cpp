#include "backends/stub/stub_ocr_backend.hpp"

#include <algorithm>
#include <array>
#include <chrono>

namespace plocr {
namespace {

Rect make_box(std::uint32_t image_width,
              std::uint32_t image_height,
              float rel_x,
              float rel_y,
              float rel_width,
              float rel_height)
{
    const float width = static_cast<float>(std::max<std::uint32_t>(image_width, 1U));
    const float height = static_cast<float>(std::max<std::uint32_t>(image_height, 1U));

    const float max_x = std::max(0.0F, width - 1.0F);
    const float max_y = std::max(0.0F, height - 1.0F);
    const float x = std::clamp(width * rel_x, 0.0F, max_x);
    const float y = std::clamp(height * rel_y, 0.0F, max_y);
    const float box_width = std::clamp(width * rel_width, 1.0F, width - x);
    const float box_height = std::clamp(height * rel_height, 1.0F, height - y);

    return Rect{x, y, box_width, box_height};
}

Quad make_quad(const Rect& rect)
{
    Quad quad;
    quad.x = {rect.x, rect.x + rect.width, rect.x + rect.width, rect.x};
    quad.y = {rect.y, rect.y, rect.y + rect.height, rect.y + rect.height};
    quad.has_quad = true;
    return quad;
}

OcrBox make_text_box(const char* text, float confidence, const Rect& rect)
{
    OcrBox box;
    box.text_utf8 = text;
    box.confidence = confidence;
    box.bbox = rect;
    box.quad = make_quad(rect);
    return box;
}

}  // namespace

BackendInfo StubOcrBackend::backend_info() const
{
    BackendInfo info;
    info.name = "stub";
    info.model_version = "stub-v1";
    info.runtime_version = "none";
    info.execution_provider = "cpu";
    info.is_configured = true;
    return info;
}

OcrResult StubOcrBackend::recognize(const ImageView& image) const
{
    const auto started_at = std::chrono::steady_clock::now();

    OcrResult result;
    result.backend = backend_info();
    result.boxes.reserve(3);
    result.boxes.push_back(make_text_box("sk-demo-REDACTED-1234",
                                         0.98F,
                                         make_box(image.width, image.height, 0.10F, 0.16F, 0.42F, 0.10F)));
    result.boxes.push_back(make_text_box("demo@example.com",
                                         0.95F,
                                         make_box(image.width, image.height, 0.18F, 0.42F, 0.36F, 0.09F)));
    result.boxes.push_back(make_text_box("+1 555 0100",
                                         0.93F,
                                         make_box(image.width, image.height, 0.58F, 0.68F, 0.26F, 0.08F)));

    const auto finished_at = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double, std::milli>(finished_at - started_at).count();
    result.latency.total_ms = std::max(elapsed, 0.05);
    result.latency.preprocess_ms = result.latency.total_ms * 0.20;
    result.latency.inference_ms = result.latency.total_ms * 0.55;
    result.latency.postprocess_ms = result.latency.total_ms * 0.25;
    return result;
}

}  // namespace plocr
