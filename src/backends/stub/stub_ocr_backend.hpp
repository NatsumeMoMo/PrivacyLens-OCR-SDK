#pragma once

#include "core/ocr_backend.hpp"
#include "core/ocr_types.hpp"

namespace plocr {

class StubOcrBackend final : public IOcrBackend {
public:
    [[nodiscard]] BackendInfo backend_info() const override;
    [[nodiscard]] BackendCapabilities capabilities() const override;
    [[nodiscard]] OcrResult recognize(const OcrRequest& request) const override;
};

}  // namespace plocr
