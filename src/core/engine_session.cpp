#include "core/engine_session.hpp"

#include "providers/app_window/app_window_provider.hpp"

namespace plengine {

void EngineSession::update_policy(const PolicySnapshot& policy)
{
    policy_ = policy;
    latest_.policy_generation = policy_.generation;
    clear_error();
}

void EngineSession::submit_frame(const FrameInfo& frame, const FrameContext& context)
{
    latest_.policy_generation = policy_.generation;
    latest_.masks = AppWindowProvider().evaluate(policy_, frame, context);
    clear_error();
}

MaskSnapshot EngineSession::latest_masks() const
{
    return latest_;
}

void EngineSession::set_error(Status status, std::string message)
{
    last_status_ = status;
    last_error_ = std::move(message);
}

void EngineSession::clear_error()
{
    set_error(Status::ok, "OK");
}

Status EngineSession::last_status() const
{
    return last_status_;
}

const std::string& EngineSession::last_error() const
{
    return last_error_;
}

}  // namespace plengine
