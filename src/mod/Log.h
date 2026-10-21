#pragma once

#include "ll/api/io/Logger.h"

namespace beetroot {

/// The mod's logger (writes to the LeviLamina console/log as "BeetrootSkin").
[[nodiscard]] ll::io::Logger& log();

} // namespace beetroot
