#pragma once

// BEETROOTSKIN_API marks the functions BeetrootSkin.dll exports. BeetrootSkin itself is built
// with BEETROOTSKIN_EXPORTS defined; other mods include these headers without it and link
// against BeetrootSkin.lib (bin/BeetrootSkin-sdk/lib).
#ifdef BEETROOTSKIN_EXPORTS
#define BEETROOTSKIN_API __declspec(dllexport)
#else
#define BEETROOTSKIN_API __declspec(dllimport)
#endif
