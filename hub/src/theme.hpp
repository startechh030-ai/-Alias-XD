#pragma once
// The hub theme. Dark is the product, not a mode: there is no light path in step 1, so the
// palette lives here as data and the mockup in assets/hub/hub-mockup.svg uses these exact
// values. If a colour changes, change it here and regenerate the mockup in the same commit.

#include <cstdint>
#include <string>

namespace lume::hub::theme {

struct Rgba {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;
};

constexpr std::uint32_t pack(float r, float g, float b) {
  return (static_cast<std::uint32_t>(r * 255.0f) << 16) |
         (static_cast<std::uint32_t>(g * 255.0f) << 8) | static_cast<std::uint32_t>(b * 255.0f);
}

// Surfaces: the hub is built out of exactly four of these. Anything softer is a bug.
constexpr std::uint32_t kWindow = 0x0B0D10;   // behind everything
constexpr std::uint32_t kPanel = 0x111419;    // sidebar, top bar, footer
constexpr std::uint32_t kCard = 0x171B22;     // project card body
constexpr std::uint32_t kRaised = 0x1E242D;   // hover / selected fill
constexpr std::uint32_t kField = 0x0E1116;    // search box, inputs
constexpr std::uint32_t kThumbTop = 0x1B222C; // card thumbnail gradient start
constexpr std::uint32_t kThumbBottom = 0x0D1116;

// Lines: 1px, never 2px. Borders do the structural work the shadows cannot.
constexpr std::uint32_t kBorder = 0x232A35;
constexpr std::uint32_t kBorderStrong = 0x2E3846;
constexpr std::uint32_t kFocus = 0x4FD1C5;

// Text: four weights of colour, no more.
constexpr std::uint32_t kTextHi = 0xE9EDF3;
constexpr std::uint32_t kText = 0xB9C2CF;
constexpr std::uint32_t kTextDim = 0x7C8796;
constexpr std::uint32_t kTextFaint = 0x545E6C;

// Accents. Teal is the default; the choice is a hub setting, not a per-project one.
constexpr std::uint32_t kAccentTeal = 0x4FD1C5;
constexpr std::uint32_t kAccentAmber = 0xF5B45E;
constexpr std::uint32_t kAccentRose = 0xF1757F;
constexpr std::uint32_t kAccentViolet = 0x8F87F1;
constexpr std::uint32_t kDanger = 0xE0574F;
constexpr std::uint32_t kOk = 0x58C97A;

// Geometry. One card is 320x208: a 148px thumbnail, then three text lines whose baselines are
// 166 / 184 / 200 from the card top. The numbers are chosen so the block ends 3px above the
// border instead of overflowing it - which is why the thumbnail is 148 and not a rounder 160.
constexpr float kCardW = 320.0f;
constexpr float kCardH = 208.0f;
constexpr float kThumbH = 148.0f;
constexpr float kTextTitle = 166.0f;   // baseline offsets, from the card top
constexpr float kTextMeta = 184.0f;
constexpr float kTextNote = 200.0f;
constexpr float kTextPad = 14.0f;      // left inset for the three text lines
constexpr float kCardGap = 16.0f;
constexpr float kGridPad = 24.0f;
constexpr float kTopBarH = 56.0f;
constexpr float kSidebarW = 216.0f;
constexpr float kFooterH = 28.0f;
constexpr float kRadius = 10.0f;
constexpr float kFontSizeTitle = 22.0f;
constexpr float kFontSizeBody = 14.0f;
constexpr float kFontSizeMeta = 12.0f;

constexpr std::uint32_t accent_for(const std::string& name) {
  return name == "amber"   ? kAccentAmber
         : name == "rose"  ? kAccentRose
         : name == "violet" ? kAccentViolet
                            : kAccentTeal;
}

constexpr std::uint32_t with_alpha(std::uint32_t rgb, float a) {
  return (static_cast<std::uint32_t>(a * 255.0f) << 24) | rgb;
}

// A deterministic, low-saturation tint per card, so a folder of projects reads as a wall of
// quiet slabs rather than a lottery. Derived from the project id, never random per frame.
constexpr std::uint32_t card_tint(std::uint64_t id_hash) {
  constexpr std::uint32_t kTints[] = {0x4FD1C5, 0x8F87F1, 0xF5B45E, 0x6FA8F5, 0xF1757F, 0x58C97A};
  return kTints[id_hash % 6u];
}

inline Rgba rgba(std::uint32_t rgb) {
  Rgba c;
  c.r = static_cast<float>((rgb >> 16) & 0xFFu) / 255.0f;
  c.g = static_cast<float>((rgb >> 8) & 0xFFu) / 255.0f;
  c.b = static_cast<float>(rgb & 0xFFu) / 255.0f;
  return c;
}

}  // namespace lume::hub::theme
