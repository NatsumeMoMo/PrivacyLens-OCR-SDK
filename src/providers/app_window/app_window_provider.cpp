#include "providers/app_window/app_window_provider.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwmapi.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <optional>
#include <string>
#include <vector>

namespace plengine {
namespace {

struct TrackedWindow {
    HWND handle = nullptr;
    Rect visual_rect;
    int priority = 0;
};

Rect rect_from_win32(const RECT& rect)
{
    return {rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top};
}

Rect visual_window_bounds(HWND window)
{
    RECT rect{};
    HRESULT hr = DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect));
    if (FAILED(hr)) {
        if (!GetWindowRect(window, &rect)) {
            return {};
        }
    }
    return rect_from_win32(rect);
}

bool is_dwm_cloaked(HWND window)
{
    DWORD cloaked = 0;
    const HRESULT hr = DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return SUCCEEDED(hr) && cloaked != 0U;
}

bool is_minimized(HWND window)
{
    if (IsIconic(window)) {
        return true;
    }

    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(window, &placement)) {
        return false;
    }

    return placement.showCmd == SW_SHOWMINIMIZED || placement.showCmd == SW_MINIMIZE;
}

std::wstring lower_copy(std::wstring value)
{
    for (auto& ch : value) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return value;
}

std::wstring process_image_base_name(HWND window)
{
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id == 0U) {
        return {};
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) {
        return {};
    }

    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    const BOOL ok = QueryFullProcessImageNameW(process, 0, path.data(), &length);
    CloseHandle(process);
    if (!ok || length == 0U) {
        return {};
    }

    path.resize(length);
    const auto slash = path.find_last_of(L"\\/");
    const std::wstring base = slash == std::wstring::npos ? path : path.substr(slash + 1U);
    return lower_copy(base);
}

bool is_trackable_top_level_window(HWND window)
{
    if (window == nullptr || !IsWindow(window) || !IsWindowVisible(window) || is_minimized(window) ||
        is_dwm_cloaked(window)) {
        return false;
    }

    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    if ((style & WS_DISABLED) != 0) {
        return false;
    }

    const Rect bounds = visual_window_bounds(window);
    return bounds.width >= 16 && bounds.height >= 16;
}

DWORD process_id_for_window(HWND window)
{
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    return process_id;
}

int wechat_priority(HWND window)
{
    if (process_id_for_window(window) == GetCurrentProcessId()) {
        return 0;
    }

    const std::wstring process_name = process_image_base_name(window);
    if (process_name == L"wechat.exe" || process_name == L"weixin.exe") {
        return 120;
    }
    if (process_name == L"wechatappex.exe") {
        return 20;
    }
    return 0;
}

std::optional<TrackedWindow> find_wechat_window()
{
    struct EnumContext {
        std::vector<TrackedWindow> candidates;
    };

    EnumContext context;
    EnumWindows([](HWND window, LPARAM parameter) -> BOOL {
        auto* context = reinterpret_cast<EnumContext*>(parameter);
        if (context == nullptr || !is_trackable_top_level_window(window)) {
            return TRUE;
        }

        const int priority = wechat_priority(window);
        if (priority <= 0) {
            return TRUE;
        }

        const Rect bounds = visual_window_bounds(window);
        if (bounds.width < 120 || bounds.height < 120) {
            return TRUE;
        }

        context->candidates.push_back({window, bounds, priority});
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));

    if (context.candidates.empty()) {
        return std::nullopt;
    }

    return *std::max_element(context.candidates.begin(), context.candidates.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.priority != rhs.priority) {
            return lhs.priority < rhs.priority;
        }
        return (lhs.visual_rect.width * lhs.visual_rect.height) < (rhs.visual_rect.width * rhs.visual_rect.height);
    });
}

std::vector<Rect> occluding_window_rects_above(HWND target_window, const Rect& monitor_rect)
{
    std::vector<Rect> occluders;
    for (HWND window = GetTopWindow(nullptr); window != nullptr; window = GetWindow(window, GW_HWNDNEXT)) {
        if (window == target_window) {
            break;
        }

        if (!is_trackable_top_level_window(window)) {
            continue;
        }

        const Rect occluder = intersect_rect(visual_window_bounds(window), monitor_rect);
        if (!occluder.empty()) {
            occluders.push_back(occluder);
        }
    }
    return occluders;
}

std::vector<Rect> visible_window_regions(HWND target_window, const Rect& target_rect, const Rect& monitor_rect)
{
    if (!is_trackable_top_level_window(target_window)) {
        return {};
    }

    const Rect clipped_target = intersect_rect(target_rect, monitor_rect);
    if (clipped_target.empty()) {
        return {};
    }

    return subtract_occluders(clipped_target, occluding_window_rects_above(target_window, monitor_rect));
}

}  // namespace

std::vector<Mask> make_wechat_masks_from_visible_regions(const PolicySnapshot& policy,
                                                         const std::vector<Rect>& visible_regions)
{
    std::vector<Mask> masks;
    if (!policy.mask_app_wechat) {
        return masks;
    }

    masks.reserve(visible_regions.size());
    std::uint64_t next_id = 1;
    for (const Rect& region : visible_regions) {
        if (region.empty()) {
            continue;
        }

        Mask mask;
        mask.id = next_id++;
        mask.rect = region;
        mask.source = MaskSource::app_window;
        mask.category = MaskCategory::app_wechat;
        mask.style = MaskStyle::mosaic;
        mask.confidence = 1.0F;
        mask.policy_generation = policy.generation;
        mask.reason_code = "app.wechat.visible_region";
        masks.push_back(std::move(mask));
    }
    return masks;
}

std::vector<Mask> AppWindowProvider::evaluate(const PolicySnapshot& policy,
                                              const FrameInfo& frame,
                                              const FrameContext& context) const
{
    if (!policy.mask_app_wechat || context.source_type != SourceType::monitor || frame.width == 0U || frame.height == 0U ||
        context.monitor_rect.empty()) {
        return {};
    }

    const std::optional<TrackedWindow> wechat = find_wechat_window();
    if (!wechat.has_value()) {
        return {};
    }

    const auto visible_regions = visible_window_regions(wechat->handle, wechat->visual_rect, context.monitor_rect);
    const auto mapped_regions =
        map_rects_to_frame(visible_regions, context.monitor_rect, {static_cast<int>(frame.width), static_cast<int>(frame.height)});
    return make_wechat_masks_from_visible_regions(policy, mapped_regions);
}

}  // namespace plengine
