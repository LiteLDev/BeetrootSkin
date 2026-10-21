#pragma once

// Layout of AppleSkin's food tooltip (TooltipOverlayHandler.FoodTooltip / FoodTooltipRenderer):
// a row of 9x9 hunger shanks and below it a row of 7x7 saturation icons, both drawn right to
// left, with an "xN" text instead of the icons when there would be more than 10 of them.
// All positions are GUI pixels relative to the top-left corner of the tooltip component.

#include <string>
#include <vector>

namespace beetroot::core::tooltip {

inline constexpr int HUNGER_ICON_SIZE     = 9;
inline constexpr int SATURATION_ICON_SIZE = 7;
inline constexpr int SATURATION_ROW_Y     = HUNGER_ICON_SIZE + 1; ///< 10
/// Component height: hunger + spacing + saturation + AppleSkin's "3 extra looks best".
inline constexpr int HEIGHT   = HUNGER_ICON_SIZE + 1 + SATURATION_ICON_SIZE + 3; ///< 20
inline constexpr int MAX_BARS = 10;
/// Overflow text ("x12") is drawn at this scale and color (ARGB).
inline constexpr float        OVERFLOW_TEXT_SCALE = 0.75f;
inline constexpr unsigned int OVERFLOW_TEXT_ARGB  = 0xFFAAAAAA;
/// Width in pixels of one glyph of the Minecraft font used for the overflow text ('x', '-', digits).
inline constexpr int GLYPH_WIDTH = 6;

/// AppleSkin FoodOutline: tint of the outline drawn behind a hunger shank.
enum class Outline { Negative, Extra, Normal, Partial, Missing };

/// FoodOutline.get(modifiedHunger, defaultHunger, i) where i = 2 * barIndex.
[[nodiscard]] Outline outlineFor(int modifiedHunger, int defaultHunger, int i) noexcept;

struct Rgba {
    float       r = 0, g = 0, b = 0, a = 1;
    friend bool operator==(Rgba const&, Rgba const&) = default;
};
/// FoodOutline.argb(): NEGATIVE white, EXTRA dark green, NORMAL black, PARTIAL brown, MISSING red @0.5.
[[nodiscard]] Rgba outlineColor(Outline o) noexcept;

/// One hunger shank (bar index k, i = 2k half-shanks before it).
struct HungerIcon {
    int         x            = 0;               ///< Left edge (y is 0).
    Outline     outline      = Outline::Normal; ///< Normal = no outline drawn.
    bool        defaultHalf  = false;           ///< The default-value shank (alpha 0.25) is a half shank.
    bool        showModified = false;           ///< Draw the modified-value shank at full alpha.
    bool        modifiedHalf = false;           ///< ... as a half shank.
    friend bool operator==(HungerIcon const&, HungerIcon const&) = default;
};

/// One saturation icon from icons.png (7x7 at u, v).
struct SaturationIcon {
    int         x     = 0; ///< Left edge (y is SATURATION_ROW_Y).
    int         u     = 0;
    int         v     = 0;
    bool        faded = false; ///< Beyond the actual value: drawn at alpha 0.5.
    friend bool operator==(SaturationIcon const&, SaturationIcon const&) = default;
};

/// A text drawn after the icons (at OVERFLOW_TEXT_SCALE); position of the unscaled text origin.
struct OverflowText {
    std::string text; ///< Empty = none.
    float       x = 0;
    float       y = 0;
};

struct FoodTooltipLayout {
    int         hungerBars = 0;
    std::string hungerBarsText; ///< "x12" / "x-11" when hungerBars was > 10 (then hungerBars = 1).
    int         saturationBars = 0;
    std::string saturationBarsText; ///< Set when the saturation bars were > 10 or 0 (then saturationBars = 1).

    std::vector<HungerIcon>     hunger;     ///< Draw order (rightmost bar first, like AppleSkin).
    std::vector<SaturationIcon> saturation; ///< Draw order (rightmost first).
    OverflowText                hungerText;
    OverflowText                saturationText;

    /// AppleSkin shows the component only when hungerBars > 0.
    [[nodiscard]] bool visible() const noexcept { return hungerBars > 0; }
    /// getWidth(): max(hungerBars*9 + textW, saturationBars*7 + textW) + 2, with the unscaled
    /// font width of the texts estimated by estimateTextWidth.
    [[nodiscard]] int        width() const;
    [[nodiscard]] static int height() noexcept { return HEIGHT; }
};

/// Unscaled width of an overflow text in the Minecraft font (GLYPH_WIDTH per character).
[[nodiscard]] int estimateTextWidth(std::string const& s) noexcept;

/// "x" + n, e.g. overflowText(12) == "x12", overflowText(-11) == "x-11".
[[nodiscard]] std::string overflowText(int signedBars);

/// Build the layout for a food whose default values are (defaultHunger, defaultSaturation) and
/// whose player-modified values are (modifiedHunger, modifiedSaturation). Saturation values are
/// the saturation *gained* by eating (Bedrock: nutrition * modifier * 2).
[[nodiscard]] FoodTooltipLayout
layoutFoodTooltip(int defaultHunger, float defaultSaturation, int modifiedHunger, float modifiedSaturation);

} // namespace beetroot::core::tooltip
