#include "providers/app_window/visible_regions.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool contains_rect(const std::vector<plengine::Rect>& regions, const plengine::Rect& rect)
{
    for (const auto& region : regions) {
        if (region == rect) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main()
{
    {
        const auto visible = plengine::subtract_rect({0, 0, 100, 80}, {0, 0, 100, 80});
        require(visible.empty(), "full occlusion should remove every visible region");
    }

    {
        const auto visible = plengine::subtract_rect({0, 0, 100, 80}, {40, 0, 60, 80});
        require(visible.size() == 1U, "side occlusion should leave one visible strip");
        require(visible[0] == plengine::Rect{0, 0, 40, 80}, "side occlusion strip has wrong geometry");
    }

    {
        const auto visible = plengine::subtract_rect({0, 0, 100, 100}, {25, 25, 50, 50});
        require(visible.size() == 4U, "center occlusion should split into four non-overlapping regions");
        require(contains_rect(visible, {0, 0, 100, 25}), "missing top region");
        require(contains_rect(visible, {0, 75, 100, 25}), "missing bottom region");
        require(contains_rect(visible, {0, 25, 25, 50}), "missing left region");
        require(contains_rect(visible, {75, 25, 25, 50}), "missing right region");
    }

    {
        const auto visible = plengine::subtract_occluders({0, 0, 100, 80}, {
            {20, 0, 20, 80},
            {40, 0, 60, 80},
        });
        require(visible.size() == 1U, "multiple occluders should leave only uncovered fragments");
        require(visible[0] == plengine::Rect{0, 0, 20, 80}, "remaining fragment after multiple occluders is wrong");
    }

    {
        const auto mapped = plengine::map_rect_to_frame({100, 100, 400, 200},
                                                        {0, 0, 1000, 500},
                                                        {2000, 1000});
        require(mapped.has_value(), "rect inside monitor should map to frame coordinates");
        require(*mapped == plengine::Rect{200, 200, 800, 400}, "mapped frame rect has wrong geometry");
    }

    {
        const auto mapped = plengine::map_rect_to_frame({900, 400, 300, 300},
                                                        {0, 0, 1000, 500},
                                                        {2000, 1000});
        require(mapped.has_value(), "partially clipped rect should map to frame coordinates");
        require(*mapped == plengine::Rect{1800, 800, 200, 200}, "clipped mapped frame rect has wrong geometry");
    }

    {
        const auto mapped = plengine::map_rect_to_frame({1200, 100, 50, 50},
                                                        {0, 0, 1000, 500},
                                                        {2000, 1000});
        require(!mapped.has_value(), "off-monitor rect should not map to frame coordinates");
    }

    return 0;
}
