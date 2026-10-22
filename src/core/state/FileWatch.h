#pragma once

// Change detection for a file edited by the user while the game runs (config.json live reload).
// Pure logic plus one std::filesystem stat; the caller owns the clock and the reload.

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>

namespace beetroot::core {

/// What a cheap stat tells about a file: enough to notice an edit without reading it.
struct FileStamp {
    bool          exists = false;
    std::int64_t  mtime  = 0; ///< last_write_time ticks (only compared for equality).
    std::uint64_t size   = 0;

    friend bool operator==(FileStamp const&, FileStamp const&) = default;
};

/// Stat `file`. Missing or unreadable -> {exists = false}.
[[nodiscard]] FileStamp statFile(std::filesystem::path const& file);

/// Decides when to stat a watched file and when a change is ready to apply.
///
/// - due(now): true at most once per `interval` (call it as often as you like, it only compares
///   time points); then stat the file and pass the result to update().
/// - update(stamp, now): true when the file differs from the known state and was unchanged for
///   `settle` (so an editor's multi-step save or a half-written file is not read mid-write).
///   The new stamp then becomes the known state: each distinct change is reported once.
/// - acknowledge(stamp, now): our own write (or initial load) produced `stamp`; never report it.
class FileChangeDetector {
public:
    using Clock    = std::chrono::steady_clock;
    using Duration = Clock::duration;

    static constexpr auto DEFAULT_INTERVAL = std::chrono::milliseconds(1000);
    static constexpr auto DEFAULT_SETTLE   = std::chrono::milliseconds(250);

    explicit FileChangeDetector(Duration interval = DEFAULT_INTERVAL, Duration settle = DEFAULT_SETTLE)
    : mInterval(interval),
      mSettle(settle) {}

    [[nodiscard]] bool due(Clock::time_point now) const { return !mNext || now >= *mNext; }

    bool update(FileStamp const& stamp, Clock::time_point now);

    void acknowledge(FileStamp const& stamp, Clock::time_point now);

    [[nodiscard]] FileStamp const& known() const { return mKnown; }
    [[nodiscard]] bool             pending() const { return mCandidate.has_value(); }

private:
    Duration                         mInterval;
    Duration                         mSettle;
    FileStamp                        mKnown;
    std::optional<FileStamp>         mCandidate; ///< Changed stamp waiting to settle.
    std::optional<Clock::time_point> mNext;      ///< Next due time (none: due now).
};

} // namespace beetroot::core
