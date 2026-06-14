#pragma once

#include "core/ocr_backend.hpp"

#include <memory>
#include <string>

namespace plocr {

class RapidOcrOnnxBackend final : public IOcrBackend {
public:
    explicit RapidOcrOnnxBackend(std::string model_dir);
    ~RapidOcrOnnxBackend() override;

    RapidOcrOnnxBackend(const RapidOcrOnnxBackend&) = delete;
    RapidOcrOnnxBackend& operator=(const RapidOcrOnnxBackend&) = delete;

    [[nodiscard]] BackendInfo backend_info() const override;
    [[nodiscard]] OcrResult recognize(const ImageView& image) const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace plocr
