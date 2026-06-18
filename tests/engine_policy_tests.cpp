#include "pl_engine.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

pl_engine_frame make_frame()
{
    pl_engine_frame frame{};
    frame.struct_size = sizeof(frame);
    frame.width = 2000;
    frame.height = 1000;
    frame.stride_bytes = 0;
    frame.pixel_format = PL_ENGINE_PIXEL_FORMAT_UNKNOWN;
    frame.frame_id = 42;
    frame.timestamp_ms = 1234;
    return frame;
}

pl_engine_frame_context make_monitor_context()
{
    pl_engine_frame_context context{};
    context.struct_size = sizeof(context);
    context.source_type = PL_ENGINE_SOURCE_MONITOR;
    context.monitor_rect = {0, 0, 1000, 500};
    context.capture_rect = {0, 0, 1000, 500};
    return context;
}

}  // namespace

int main()
{
    require(std::strcmp(pl_engine_status_to_string(PL_ENGINE_STATUS_OK), "OK") == 0,
            "status string for OK is wrong");

    pl_engine* engine = nullptr;
    pl_engine_config config{};
    config.struct_size = sizeof(config);
    require(pl_engine_create(&config, &engine) == PL_ENGINE_STATUS_OK, "engine creation failed");
    require(engine != nullptr, "engine handle is null");

    pl_engine_session* session = nullptr;
    pl_engine_session_config session_config{};
    session_config.struct_size = sizeof(session_config);
    require(pl_engine_session_create(engine, &session_config, &session) == PL_ENGINE_STATUS_OK,
            "session creation failed");
    require(session != nullptr, "session handle is null");

    pl_engine_policy disabled_policy{};
    disabled_policy.struct_size = sizeof(disabled_policy);
    disabled_policy.policy_generation = 7;
    disabled_policy.mask_app_wechat = 0;
    require(pl_engine_session_update_policy(session, &disabled_policy) == PL_ENGINE_STATUS_OK,
            "disabled policy update failed");

    pl_engine_frame frame = make_frame();
    pl_engine_frame_context context = make_monitor_context();
    require(pl_engine_session_submit_frame(session, &frame, &context) == PL_ENGINE_STATUS_OK,
            "submit frame with disabled policy failed");

    pl_engine_mask_list* masks = nullptr;
    require(pl_engine_session_get_latest_masks(session, &masks) == PL_ENGINE_STATUS_OK,
            "get masks with disabled policy failed");
    require(masks != nullptr, "mask list is null");
    require(masks->struct_size == sizeof(pl_engine_mask_list), "mask list struct_size is wrong");
    require(masks->policy_generation == 7, "mask list did not copy policy generation");
    require(masks->mask_count == 0, "disabled WeChat policy should return zero masks");
    require(masks->masks == nullptr, "empty mask list should not expose a masks pointer");
    pl_engine_mask_list_destroy(masks);

    pl_engine_policy enabled_policy{};
    enabled_policy.struct_size = sizeof(enabled_policy);
    enabled_policy.policy_generation = 8;
    enabled_policy.mask_app_wechat = 1;
    require(pl_engine_session_update_policy(session, &enabled_policy) == PL_ENGINE_STATUS_OK,
            "enabled policy update failed");
    require(pl_engine_session_submit_frame(session, &frame, &context) == PL_ENGINE_STATUS_OK,
            "submit frame with enabled policy failed");

    masks = nullptr;
    require(pl_engine_session_get_latest_masks(session, &masks) == PL_ENGINE_STATUS_OK,
            "get masks with enabled policy failed");
    require(masks != nullptr, "enabled mask list is null");
    require(masks->policy_generation == 8, "enabled mask list did not copy policy generation");
    pl_engine_mask_list_destroy(masks);

    pl_engine_error_info error{};
    error.struct_size = sizeof(error);
    require(pl_engine_get_last_error(session, &error) == PL_ENGINE_STATUS_OK, "last error query failed");
    require(error.status == PL_ENGINE_STATUS_OK, "last error status should be OK");

    pl_engine_session_destroy(session);
    pl_engine_destroy(engine);
    return 0;
}
