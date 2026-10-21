#pragma once

namespace beetroot::ui {

/// Physical key state, polled from the OS (any thread). Use for "hold Shift to show more" UI;
/// not for hotkeys (those belong to ll::input / KeyInputEvent handling).
[[nodiscard]] bool isShiftDown();
[[nodiscard]] bool isControlDown();
[[nodiscard]] bool isAltDown();

} // namespace beetroot::ui
