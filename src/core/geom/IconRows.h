#pragma once

// Resting (un-jittered) positions of a vanilla HUD icon bar (hunger shanks, hearts).
//
// Vanilla shakes the icons vertically (hunger at saturation 0, hearts at low health) by
// writing the shaken y straight into the renderer's per-icon positions; the resting layout is
// not stored anywhere. When every icon of a row shakes, the highest (or any other) icon of the
// frame is not the resting line, so the resting y is learned from frames in which the bar is
// still (every row of 2+ icons level). Vanilla shakes each icon independently, so a whole row
// can still land on one shaken y by chance (e.g. all ten hearts +1, about 1 tick in 1000 at
// low health); every still frame is therefore a vote and the resting y of a row is the y with
// the most votes, which a rare lockstep shake cannot outvote once the bar has been seen at rest.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace beetroot::core {

class RestingRows {
public:
    /// Shake amplitude (GUI units) beyond which a fallback resting y is considered implausible.
    static constexpr float MAX_SHAKE = 2;

    /// Feed one frame of icon y values (in a frame of reference that only changes with the
    /// layout, e.g. relative to the owner control) laid out in rows of `perRow` icons (perRow
    /// <= 0: a single row) and get the resting y of every icon (a reference to an internal
    /// buffer, valid until the next call).
    /// Rows without votes yet fall back to `fallbackRow0` for row 0 when given and within
    /// MAX_SHAKE of the row's icons (vanilla lays the first row out at the owner control's
    /// origin), else to the most common y of the row (ties: the highest, i.e. smallest y).
    /// A different icon count or `perRow` than the previous frame forgets everything learned.
    [[nodiscard]] std::vector<float> const&
    update(std::vector<float> const& ys, int perRow, std::optional<float> fallbackRow0 = std::nullopt);

    /// Forget the learned rows (layout changed: owner control moved, GUI scale).
    void reset() noexcept;

    /// Whether row `row` has a learned (voted) resting y.
    [[nodiscard]] bool learned(std::size_t row) const noexcept { return row < mVotes.size() && !mVotes[row].empty(); }

private:
    using Votes = std::vector<std::pair<float, std::uint32_t>>; ///< y -> still frames seen at it
    std::vector<Votes> mVotes;
    std::vector<float> mRest;
    std::size_t        mCount  = 0;
    int                mPerRow = 0;
    bool               mFresh  = true;
};

/// y - resting y for each icon (sizes must match; extra entries of either are ignored).
[[nodiscard]] std::vector<float> jitterOffsets(std::vector<float> const& ys, std::vector<float> const& resting);

} // namespace beetroot::core
