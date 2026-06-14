#pragma once

#include "core/ocr_backend.hpp"
#include "core/ocr_types.hpp"

namespace plocr {

class StubOcrBackend final : public IOcrBackend {
public:
    [[nodiscard]] BackendInfo backend_info() const override;
    [[nodiscard]] OcrResult recognize(const ImageView& image) const override;
};

}  // namespace plocr
