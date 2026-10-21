#pragma once

#include <algorithm>

namespace beetroot::core {

/// Axis-aligned rectangle in GUI units (the game's scaled UI coordinate space), or in texture
/// pixels when used as a source rectangle.
struct Rect {
    float x = 0, y = 0, w = 0, h = 0;

    [[nodiscard]] float right() const noexcept { return x + w; }
    [[nodiscard]] float bottom() const noexcept { return y + h; }
    [[nodiscard]] float centerX() const noexcept { return x + w / 2; }
    [[nodiscard]] float centerY() const noexcept { return y + h / 2; }
    [[nodiscard]] bool  contains(float px, float py) const noexcept {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    [[nodiscard]] bool empty() const noexcept { return !(w > 0) || !(h > 0); }
    [[nodiscard]] Rect translated(float dx, float dy) const noexcept { return {x + dx, y + dy, w, h}; }
    /// Shrink by `d` on every side (never below zero size; the result stays centered).
    [[nodiscard]] Rect inset(float d) const noexcept {
        float nw = std::max(0.f, w - 2 * d);
        float nh = std::max(0.f, h - 2 * d);
        return {x + (w - nw) / 2, y + (h - nh) / 2, nw, nh};
    }
    /// Smallest rectangle containing both (an empty operand is ignored).
    [[nodiscard]] Rect united(Rect const& o) const noexcept {
        if (empty()) return o;
        if (o.empty()) return *this;
        float l = std::min(x, o.x), t = std::min(y, o.y);
        return {l, t, std::max(right(), o.right()) - l, std::max(bottom(), o.bottom()) - t};
    }

    friend bool operator==(Rect const&, Rect const&) = default;
};

} // namespace beetroot::core
