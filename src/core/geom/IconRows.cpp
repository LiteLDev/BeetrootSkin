#include "core/geom/IconRows.h"

#include <algorithm>

namespace beetroot::core {

namespace {

constexpr std::uint32_t VOTE_CAP = 1u << 20; ///< Halve every count when one reaches this.

void vote(std::vector<std::pair<float, std::uint32_t>>& votes, float y) {
    auto it = std::find_if(votes.begin(), votes.end(), [&](auto const& v) { return v.first == y; });
    if (it == votes.end()) {
        votes.emplace_back(y, 1u);
        return;
    }
    if (++it->second >= VOTE_CAP) {
        for (auto& v : votes) v.second = std::max<std::uint32_t>(1, v.second / 2);
    }
}

/// The y with the most votes (ties: smallest y).
float winner(std::vector<std::pair<float, std::uint32_t>> const& votes) {
    auto best = votes.front();
    for (auto const& v : votes) {
        if (v.second > best.second || (v.second == best.second && v.first < best.first)) best = v;
    }
    return best.first;
}

/// Most common value of [begin, end) (ties: smallest).
float mode(float const* begin, float const* end) {
    float best      = *begin;
    int   bestCount = 0;
    for (auto const* p = begin; p != end; ++p) {
        int count = static_cast<int>(std::count(begin, end, *p));
        if (count > bestCount || (count == bestCount && *p < best)) best = *p, bestCount = count;
    }
    return best;
}

} // namespace

std::vector<float> const&
RestingRows::update(std::vector<float> const& ys, int perRow, std::optional<float> fallbackRow0) {
    std::size_t const n    = ys.size();
    std::size_t const step = perRow > 0 ? static_cast<std::size_t>(perRow) : std::max<std::size_t>(n, 1);
    std::size_t const rows = (n + step - 1) / step;
    if (mFresh || n != mCount || perRow != mPerRow) {
        mVotes.assign(rows, {});
        mCount  = n;
        mPerRow = perRow;
        mFresh  = false;
    }

    // A still frame: every row of 2+ icons is level, and there is at least one such row.
    bool anyMulti = false, still = true;
    for (std::size_t r = 0; r < rows && still; ++r) {
        std::size_t begin = r * step, end = std::min(n, begin + step);
        if (end - begin < 2) continue;
        anyMulti = true;
        still    = std::all_of(
            ys.begin() + static_cast<std::ptrdiff_t>(begin) + 1,
            ys.begin() + static_cast<std::ptrdiff_t>(end),
            [&](float y) { return y == ys[begin]; }
        );
    }
    if (still && anyMulti) {
        for (std::size_t r = 0; r < rows; ++r) vote(mVotes[r], ys[r * step]);
    }

    mRest.resize(n);
    for (std::size_t r = 0; r < rows; ++r) {
        std::size_t begin = r * step, end = std::min(n, begin + step);
        float       rest;
        if (!mVotes[r].empty()) {
            rest = winner(mVotes[r]);
        } else {
            auto [lo, hi] = std::minmax_element(ys.data() + begin, ys.data() + end);
            if (r == 0 && fallbackRow0 && *fallbackRow0 >= *lo - MAX_SHAKE && *fallbackRow0 <= *hi + MAX_SHAKE) {
                rest = *fallbackRow0;
            } else {
                rest = mode(ys.data() + begin, ys.data() + end);
            }
        }
        std::fill(
            mRest.begin() + static_cast<std::ptrdiff_t>(begin),
            mRest.begin() + static_cast<std::ptrdiff_t>(end),
            rest
        );
    }
    return mRest;
}

void RestingRows::reset() noexcept {
    mVotes.clear();
    mFresh = true;
}

std::vector<float> jitterOffsets(std::vector<float> const& ys, std::vector<float> const& resting) {
    std::vector<float> out(std::min(ys.size(), resting.size()));
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = ys[i] - resting[i];
    return out;
}

} // namespace beetroot::core
