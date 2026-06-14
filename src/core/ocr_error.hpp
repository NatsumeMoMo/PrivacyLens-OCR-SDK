#pragma once

#include <stdexcept>
#include <string>

namespace plocr {

enum class ErrorCode {
    invalid_argument,
    model_not_configured,
    backend_unavailable,
    image_format_unsupported,
    internal_error,
};

class OcrError final : public std::runtime_error {
public:
    OcrError(ErrorCode code, std::string message)
        : std::runtime_error(message)
        , code_(code)
    {
    }

    [[nodiscard]] ErrorCode code() const noexcept
    {
        return code_;
    }

private:
    ErrorCode code_;
};

}  // namespace plocr
