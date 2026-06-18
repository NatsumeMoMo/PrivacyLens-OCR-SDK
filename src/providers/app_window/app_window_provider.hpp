#pragma once

#include "core/engine_types.hpp"

#include <vector>

namespace plengine {

[[nodiscard]] std::vector<Mask> make_wechat_masks_from_visible_regions(
    const PolicySnapshot& policy,
    const std::vector<Rect>& visible_regions);

class AppWindowProvider final {
public:
    [[nodiscard]] std::vector<Mask> evaluate(const PolicySnapshot& policy,
                                             const FrameInfo& frame,
                                             const FrameContext& context) const;
};

}  // namespace plengine
