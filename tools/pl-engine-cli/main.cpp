#include "pl_engine.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

struct Options {
    bool self_test = false;
    bool mask_wechat = false;
};

void print_usage()
{
    std::cout << "Usage:\n"
              << "  pl-engine-cli --self-test [--mask-wechat]\n";
}

Options parse_options(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--self-test") {
            options.self_test = true;
        } else if (arg == "--mask-wechat") {
            options.mask_wechat = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage();
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    return options;
}

pl_engine_rect primary_monitor_rect()
{
    const HMONITOR monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &info)) {
        return {0, 0, 1920, 1080};
    }
    return {info.rcMonitor.left,
            info.rcMonitor.top,
            info.rcMonitor.right - info.rcMonitor.left,
            info.rcMonitor.bottom - info.rcMonitor.top};
}

void print_error(pl_engine_session* session, pl_engine_status status)
{
    pl_engine_error_info error{};
    error.struct_size = sizeof(error);
    const auto error_status = pl_engine_get_last_error(session, &error);
    std::cerr << "status=" << pl_engine_status_to_string(status);
    if (error_status == PL_ENGINE_STATUS_OK && error.message_utf8 != nullptr) {
        std::cerr << " message=\"" << error.message_utf8 << "\"";
    }
    std::cerr << '\n';
}

int run_self_test(const Options& options)
{
    pl_engine* engine = nullptr;
    pl_engine_config config{};
    config.struct_size = sizeof(config);
    auto status = pl_engine_create(&config, &engine);
    if (status != PL_ENGINE_STATUS_OK) {
        print_error(nullptr, status);
        return 1;
    }

    pl_engine_session* session = nullptr;
    pl_engine_session_config session_config{};
    session_config.struct_size = sizeof(session_config);
    status = pl_engine_session_create(engine, &session_config, &session);
    if (status != PL_ENGINE_STATUS_OK) {
        print_error(nullptr, status);
        pl_engine_destroy(engine);
        return 1;
    }

    pl_engine_policy policy{};
    policy.struct_size = sizeof(policy);
    policy.policy_generation = 1;
    policy.mask_app_wechat = options.mask_wechat ? 1U : 0U;
    status = pl_engine_session_update_policy(session, &policy);
    if (status != PL_ENGINE_STATUS_OK) {
        print_error(session, status);
        pl_engine_session_destroy(session);
        pl_engine_destroy(engine);
        return 1;
    }

    const pl_engine_rect monitor = primary_monitor_rect();
    pl_engine_frame frame{};
    frame.struct_size = sizeof(frame);
    frame.width = static_cast<std::uint32_t>(monitor.width);
    frame.height = static_cast<std::uint32_t>(monitor.height);
    frame.frame_id = 1;
    frame.timestamp_ms = 0;

    pl_engine_frame_context context{};
    context.struct_size = sizeof(context);
    context.source_type = PL_ENGINE_SOURCE_MONITOR;
    context.monitor_rect = monitor;
    context.capture_rect = monitor;

    status = pl_engine_session_submit_frame(session, &frame, &context);
    if (status != PL_ENGINE_STATUS_OK) {
        print_error(session, status);
        pl_engine_session_destroy(session);
        pl_engine_destroy(engine);
        return 1;
    }

    pl_engine_mask_list* masks = nullptr;
    status = pl_engine_session_get_latest_masks(session, &masks);
    if (status != PL_ENGINE_STATUS_OK || masks == nullptr) {
        print_error(session, status);
        pl_engine_session_destroy(session);
        pl_engine_destroy(engine);
        return 1;
    }

    std::cout << "PrivacyLens Engine SDK\n"
              << "sdk_version=0x" << std::hex << PL_ENGINE_SDK_VERSION << std::dec << '\n'
              << "status=" << pl_engine_status_to_string(status)
              << " policy_generation=" << masks->policy_generation
              << " mask_wechat=" << (options.mask_wechat ? 1 : 0)
              << " masks=" << masks->mask_count << '\n';

    pl_engine_mask_list_destroy(masks);
    pl_engine_session_destroy(session);
    pl_engine_destroy(engine);
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == 1) {
            print_usage();
            return 0;
        }
        const Options options = parse_options(argc, argv);
        if (!options.self_test) {
            print_usage();
            return 2;
        }
        return run_self_test(options);
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 2;
    }
}
