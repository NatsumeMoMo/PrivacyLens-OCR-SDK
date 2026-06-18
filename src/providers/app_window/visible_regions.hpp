#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace plengine {

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] constexpr bool empty() const
    {
        return width <= 0 || height <= 0;
    }

    [[nodiscard]] constexpr int right() const
    {
        return x + width;
    }

    [[nodiscard]] constexpr int bottom() const
    {
        return y + height;
    }
};

struct Size {
    int width = 0;
    int height = 0;

    [[nodiscard]] constexpr bool empty() const
    {
        return width <= 0 || height <= 0;
    }
};

[[nodiscard]] constexpr bool operator==(const Rect& lhs, const Rect& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width && lhs.height == rhs.height;
}

[[nodiscard]] constexpr Rect intersect_rect(const Rect& lhs, const Rect& rhs)
{
    const int left = std::max(lhs.x, rhs.x);
    const int top = std::max(lhs.y, rhs.y);
    const int right = std::min(lhs.right(), rhs.right());
    const int bottom = std::min(lhs.bottom(), rhs.bottom());
    return {left, top, right - left, bottom - top};
}

inline void append_if_visible(std::vector<Rect>& regions, const Rect& rect)
{
    if (!rect.empty()) {
        regions.push_back(rect);
    }
}

[[nodiscard]] inline std::vector<Rect> subtract_rect(const Rect& source, const Rect& occluder)
{
    if (source.empty()) {
        return {};
    }

    const Rect cut = intersect_rect(source, occluder);
    if (cut.empty()) {
        return {source};
    }

    std::vector<Rect> regions;
    append_if_visible(regions, {source.x, source.y, source.width, cut.y - source.y});
    append_if_visible(regions, {source.x, cut.bottom(), source.width, source.bottom() - cut.bottom()});
    append_if_visible(regions, {source.x, cut.y, cut.x - source.x, cut.height});
    append_if_visible(regions, {cut.right(), cut.y, source.right() - cut.right(), cut.height});
    return regions;
}

[[nodiscard]] inline std::vector<Rect> subtract_occluders(const Rect& source, const std::vector<Rect>& occluders)
{
    std::vector<Rect> visible;
    if (!source.empty()) {
        visible.push_back(source);
    }

    for (const Rect& occluder : occluders) {
        std::vector<Rect> next;
        for (const Rect& region : visible) {
            const auto pieces = subtract_rect(region, occluder);
            next.insert(next.end(), pieces.begin(), pieces.end());
        }
        visible = std::move(next);
        if (visible.empty()) {
            break;
        }
    }

    return visible;
}

[[nodiscard]] inline std::optional<Rect> map_rect_to_frame(const Rect& source_rect,
                                                           const Rect& monitor_rect,
                                                           const Size& frame_size)
{
    if (source_rect.empty() || monitor_rect.empty() || frame_size.empty()) {
        return std::nullopt;
    }

    const Rect clipped_source = intersect_rect(source_rect, monitor_rect);
    if (clipped_source.empty()) {
        return std::nullopt;
    }

    const double x_scale = static_cast<double>(frame_size.width) / static_cast<double>(monitor_rect.width);
    const double y_scale = static_cast<double>(frame_size.height) / static_cast<double>(monitor_rect.height);
    const int mapped_x = static_cast<int>(std::lround(static_cast<double>(clipped_source.x - monitor_rect.x) * x_scale));
    const int mapped_y = static_cast<int>(std::lround(static_cast<double>(clipped_source.y - monitor_rect.y) * y_scale));
    const int mapped_width = static_cast<int>(std::lround(static_cast<double>(clipped_source.width) * x_scale));
    const int mapped_height = static_cast<int>(std::lround(static_cast<double>(clipped_source.height) * y_scale));

    const Rect frame_rect{0, 0, frame_size.width, frame_size.height};
    const Rect clipped_frame = intersect_rect({mapped_x, mapped_y, mapped_width, mapped_height}, frame_rect);
    if (clipped_frame.empty()) {
        return std::nullopt;
    }

    return clipped_frame;
}

[[nodiscard]] inline std::vector<Rect> map_rects_to_frame(const std::vector<Rect>& source_rects,
                                                          const Rect& monitor_rect,
                                                          const Size& frame_size)
{
    std::vector<Rect> mapped;
    for (const Rect& source_rect : source_rects) {
        if (auto rect = map_rect_to_frame(source_rect, monitor_rect, frame_size)) {
            mapped.push_back(*rect);
        }
    }
    return mapped;
}

}  // namespace plengine
