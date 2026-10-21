#pragma once

#include "ll/api/mod/NativeMod.h"

namespace beetroot {

/// Mod entry point. Loads paths/config/translations, then installs every registered
/// app::Module (see app/Module.h). Feature units never need to edit this file.
class BeetrootSkin {
public:
    static BeetrootSkin& getInstance();

    BeetrootSkin() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace beetroot
