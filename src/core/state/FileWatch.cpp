#include "core/state/FileWatch.h"

#include <system_error>

namespace beetroot::core {

FileStamp statFile(std::filesystem::path const& file) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_regular_file(file, ec) || ec) return {};
    auto size = fs::file_size(file, ec);
    if (ec) return {};
    auto mtime = fs::last_write_time(file, ec);
    if (ec) return {};
    return {true, static_cast<std::int64_t>(mtime.time_since_epoch().count()), static_cast<std::uint64_t>(size)};
}

bool FileChangeDetector::update(FileStamp const& stamp, Clock::time_point now) {
    mNext = now + mInterval;
    if (stamp == mKnown) { // unchanged, or changed back before it settled
        mCandidate.reset();
        return false;
    }
    if (!mCandidate || *mCandidate != stamp) { // new change (or still being written): wait
        mCandidate = stamp;
        mNext      = now + mSettle;
        return false;
    }
    mKnown = stamp; // same stamp across the settle delay: apply once
    mCandidate.reset();
    return true;
}

void FileChangeDetector::acknowledge(FileStamp const& stamp, Clock::time_point now) {
    mKnown = stamp;
    mCandidate.reset();
    mNext = now + mInterval;
}

} // namespace beetroot::core
