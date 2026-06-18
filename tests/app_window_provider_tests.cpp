#include "providers/app_window/app_window_provider.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main()
{
    plengine::PolicySnapshot disabled;
    disabled.generation = 12;
    disabled.mask_app_wechat = false;
    const auto disabled_masks = plengine::make_wechat_masks_from_visible_regions(disabled, {
        {10, 20, 300, 200},
    });
    require(disabled_masks.empty(), "disabled WeChat policy should not emit masks");

    plengine::PolicySnapshot enabled;
    enabled.generation = 99;
    enabled.mask_app_wechat = true;
    const auto masks = plengine::make_wechat_masks_from_visible_regions(enabled, {
        {10, 20, 300, 200},
        {400, 50, 120, 80},
    });

    require(masks.size() == 2U, "enabled WeChat policy should emit one mask per visible fragment");
    require(masks[0].rect == plengine::Rect{10, 20, 300, 200}, "first mask rect is wrong");
    require(masks[0].source == plengine::MaskSource::app_window, "mask source should be app_window");
    require(masks[0].category == plengine::MaskCategory::app_wechat, "mask category should be app_wechat");
    require(masks[0].style == plengine::MaskStyle::mosaic, "mask style should default to mosaic");
    require(masks[0].confidence == 1.0F, "geometry-derived mask confidence should be 1");
    require(masks[0].policy_generation == 99, "mask should copy policy generation");
    require(masks[0].reason_code == "app.wechat.visible_region", "mask reason code is wrong");
    require(masks[1].id == masks[0].id + 1U, "mask IDs should be stable and incremental");

    return 0;
}
