#pragma once

#include "core/engine_types.hpp"

#include <string>

namespace plengine {

class Engine final {
public:
    Engine() = default;
};

class EngineSession final {
public:
    void update_policy(const PolicySnapshot& policy);
    void submit_frame(const FrameInfo& frame, const FrameContext& context);

    [[nodiscard]] MaskSnapshot latest_masks() const;

    void set_error(Status status, std::string message);
    void clear_error();

    [[nodiscard]] Status last_status() const;
    [[nodiscard]] const std::string& last_error() const;

private:
    PolicySnapshot policy_;
    MaskSnapshot latest_;
    Status last_status_ = Status::ok;
    std::string last_error_ = "OK";
};

}  // namespace plengine
