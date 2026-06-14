#pragma once

#include "core/ocr_backend.hpp"

#include <memory>
#include <string>

namespace plocr {

class PaddleOcrOnnxBackend final : public IOcrBackend {
public:
    explicit PaddleOcrOnnxBackend(std::string model_dir);
    ~PaddleOcrOnnxBackend() override;

    PaddleOcrOnnxBackend(const PaddleOcrOnnxBackend&) = delete;
    PaddleOcrOnnxBackend& operator=(const PaddleOcrOnnxBackend&) = delete;

    [[nodiscard]] BackendInfo backend_info() const override;
    [[nodiscard]] BackendCapabilities capabilities() const override;
    [[nodiscard]] OcrResult recognize(const OcrRequest& request) const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace plocr
