#pragma once

#include "core/engine_types.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace plengine {

class EngineError final : public std::runtime_error {
public:
    EngineError(Status status, std::string message)
        : std::runtime_error(message)
        , status_(status)
    {
    }

    [[nodiscard]] Status status() const
    {
        return status_;
    }

private:
    Status status_ = Status::internal_error;
};

}  // namespace plengine
