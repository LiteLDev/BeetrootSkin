#pragma once

// All BeetrootSkin API events (AppleSkin's squeek.appleskin.api.event). They are registered with
// LeviLamina's EventBus while BeetrootSkin loads, so listen from your mod's enable() (and declare
// the dependency on BeetrootSkin in your manifest.json).

#include "beetrootskin/api/FoodValues.h"                // IWYU pragma: export
#include "beetrootskin/api/event/FoodValuesEvent.h"     // IWYU pragma: export
#include "beetrootskin/api/event/HUDOverlayEvent.h"     // IWYU pragma: export
#include "beetrootskin/api/event/TooltipOverlayEvent.h" // IWYU pragma: export
