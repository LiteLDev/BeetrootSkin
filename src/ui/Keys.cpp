#include "ui/Keys.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace beetroot::ui {

namespace {
bool down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
} // namespace

bool isShiftDown() { return down(VK_SHIFT); }
bool isControlDown() { return down(VK_CONTROL); }
bool isAltDown() { return down(VK_MENU); }

} // namespace beetroot::ui
