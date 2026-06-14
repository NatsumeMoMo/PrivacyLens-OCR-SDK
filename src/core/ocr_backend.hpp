#pragma once

#include "core/ocr_types.hpp"

namespace plocr {

class IOcrBackend {
public:
    virtual ~IOcrBackend() = default;

    [[nodiscard]] virtual BackendInfo backend_info() const = 0;
    [[nodiscard]] virtual OcrResult recognize(const ImageView& image) const = 0;
};

}  // namespace plocr
