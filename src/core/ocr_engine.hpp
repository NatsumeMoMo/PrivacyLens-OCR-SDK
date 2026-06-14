#pragma once

#include "core/ocr_backend.hpp"
#include "backends/stub/stub_ocr_backend.hpp"
#include "core/ocr_types.hpp"

#include <memory>

namespace plocr {

class OcrEngine {
public:
    explicit OcrEngine(const OcrEngineOptions& options = {});

    [[nodiscard]] BackendInfo backend_info() const;
    [[nodiscard]] BackendCapabilities backend_capabilities() const;
    [[nodiscard]] OcrResult recognize(const ImageView& image) const;

private:
    std::unique_ptr<IOcrBackend> backend_;
};

}  // namespace plocr
