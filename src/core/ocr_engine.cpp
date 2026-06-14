#include "core/ocr_engine.hpp"

#include "backends/paddleocr_onnx/paddleocr_onnx_backend.hpp"
#include "backends/rapidocr_onnx/rapidocr_onnx_backend.hpp"
#include "backends/stub/stub_ocr_backend.hpp"
#include "core/ocr_error.hpp"

namespace plocr {

namespace {

bool is_stub_backend(const std::string& name)
{
    return name.empty() || name == "stub";
}

bool is_rapidocr_backend(const std::string& name)
{
    return name == "rapidocr_onnx" || name == "rapidocr";
}

bool is_paddleocr_backend(const std::string& name)
{
    return name == "paddleocr_onnx" || name == "paddleocr";
}

}  // namespace

OcrEngine::OcrEngine(const OcrEngineOptions& options)
{
    if (is_stub_backend(options.requested_backend)) {
        backend_ = std::make_unique<StubOcrBackend>();
    } else if (is_rapidocr_backend(options.requested_backend)) {
        backend_ = std::make_unique<RapidOcrOnnxBackend>(options.model_dir);
    } else if (is_paddleocr_backend(options.requested_backend)) {
        backend_ = std::make_unique<PaddleOcrOnnxBackend>(options.model_dir);
    } else {
        throw OcrError(ErrorCode::backend_unavailable, "unknown OCR backend requested");
    }
}

BackendInfo OcrEngine::backend_info() const
{
    return backend_->backend_info();
}

BackendCapabilities OcrEngine::backend_capabilities() const
{
    return backend_->capabilities();
}

OcrResult OcrEngine::recognize(const ImageView& image) const
{
    OcrRequest request;
    request.image = image;
    return backend_->recognize(request);
}

}  // namespace plocr
