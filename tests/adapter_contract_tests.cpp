#include "backends/stub/stub_ocr_backend.hpp"
#include "core/ocr_engine.hpp"
#include "core/ocr_types.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

plocr::ImageView make_image(std::vector<unsigned char>* pixels)
{
    constexpr std::uint32_t width = 320;
    constexpr std::uint32_t height = 180;
    pixels->assign(static_cast<std::size_t>(width) * height * 4U, 0x7F);

    plocr::ImageView image;
    image.data = pixels->data();
    image.width = width;
    image.height = height;
    image.stride_bytes = width * 4U;
    image.pixel_format = plocr::PixelFormat::bgra8;
    return image;
}

void stub_declares_privacylens_capabilities()
{
    const plocr::StubOcrBackend backend;
    const auto capabilities = backend.capabilities();

    require(capabilities.accepts_memory_input, "stub must accept memory input");
    require(!capabilities.requires_filesystem_input, "stub must not require filesystem input");
    require(capabilities.returns_source_space_bbox, "stub must return source-space bbox");
    require(capabilities.returns_source_space_quad, "stub must return source-space quad");
    require(capabilities.returns_confidence, "stub must return confidence");
}

void stub_accepts_ocr_request()
{
    const plocr::StubOcrBackend backend;
    std::vector<unsigned char> pixels;

    plocr::OcrRequest request;
    request.image = make_image(&pixels);

    const auto result = backend.recognize(request);

    require(result.backend.name == "stub", "stub result should identify backend");
    require(result.boxes.size() == 3U, "stub should return three boxes");
    require(result.boxes[0].bbox.x >= 0.0F, "stub bbox x must be source-space");
    require(result.boxes[0].bbox.y >= 0.0F, "stub bbox y must be source-space");
    require(result.boxes[0].bbox.width > 0.0F, "stub bbox width must be positive");
    require(result.boxes[0].bbox.height > 0.0F, "stub bbox height must be positive");
    require(result.boxes[0].quad.has_quad, "stub should preserve quad shape");
}

void engine_exposes_selected_backend_capabilities()
{
    plocr::OcrEngine engine;
    const auto capabilities = engine.backend_capabilities();

    require(capabilities.accepts_memory_input, "engine default backend must accept memory input");
    require(capabilities.returns_source_space_bbox, "engine default backend must expose bbox capability");
}

}  // namespace

int main()
{
    stub_declares_privacylens_capabilities();
    stub_accepts_ocr_request();
    engine_exposes_selected_backend_capabilities();
    return 0;
}
