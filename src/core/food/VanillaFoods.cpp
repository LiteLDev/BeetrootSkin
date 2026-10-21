#include "core/food/VanillaFoods.h"

#include <algorithm>

namespace beetroot::core {

namespace {

constexpr int SECONDS = 20; // ticks

struct Entry {
    std::string_view id;
    FoodEffect       effect;
};

FoodEffect fx(int id, int amplifier, int seconds, float chance, char const* name) {
    return FoodEffect{id, amplifier, seconds * SECONDS, chance, name};
}

std::vector<Entry> const& table() {
    using namespace effect_id;
    static std::vector<Entry> const t{
        {"minecraft:golden_apple",           fx(REGENERATION,    1, 5,   1.f,  "regeneration")   },
        {"minecraft:golden_apple",           fx(ABSORPTION,      0, 120, 1.f,  "absorption")     },
        {"minecraft:enchanted_golden_apple", fx(REGENERATION,    1, 30,  1.f,  "regeneration")   },
        {"minecraft:enchanted_golden_apple", fx(ABSORPTION,      3, 120, 1.f,  "absorption")     },
        {"minecraft:enchanted_golden_apple", fx(RESISTANCE,      0, 300, 1.f,  "resistance")     },
        {"minecraft:enchanted_golden_apple", fx(FIRE_RESISTANCE, 0, 300, 1.f,  "fire_resistance")},
        {"minecraft:rotten_flesh",           fx(HUNGER,          0, 30,  0.8f, "hunger")         },
        {"minecraft:chicken",                fx(HUNGER,          0, 30,  0.3f, "hunger")         },
        {"minecraft:pufferfish",             fx(HUNGER,          2, 15,  1.f,  "hunger")         },
        {"minecraft:pufferfish",             fx(POISON,          3, 60,  1.f,  "poison")         },
        {"minecraft:pufferfish",             fx(NAUSEA,          1, 15,  1.f,  "nausea")         },
        {"minecraft:spider_eye",             fx(POISON,          0, 5,   1.f,  "poison")         },
        {"minecraft:poisonous_potato",       fx(POISON,          0, 5,   0.6f, "poison")         },
    };
    return t;
}

} // namespace

std::vector<FoodEffect> vanillaFoodEffects(std::string_view itemId) {
    std::vector<FoodEffect> out;
    for (auto const& e : table()) {
        if (e.id == itemId) out.push_back(e.effect);
    }
    return out;
}

bool anyHarmful(std::vector<FoodEffect> const& effects) noexcept {
    return std::any_of(effects.begin(), effects.end(), [](FoodEffect const& e) { return isHarmfulEffect(e.id); });
}

} // namespace beetroot::core
