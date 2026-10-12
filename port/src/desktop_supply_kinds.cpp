// The supply kinds' own looks (port addition, docs/SUPPLY_BOXES.md): a
// kind's picture on an original crate, its body and lettering tints, its
// sheen and its glow.
#include "desktop_supply.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace phantomcrash {
namespace {

// A cycling glow's saturation; its value is full.
constexpr float cycle_saturation = 0.75F;

std::array<float, 3> hue_colour(const float hue) {
  const auto channel = [&](const float offset) {
    const float k = std::fmod(offset + hue * 6.0F, 6.0F);
    const float ramp = std::clamp(std::min(k, 4.0F - k), 0.0F, 1.0F);
    return 1.0F - cycle_saturation * ramp;
  };
  return {channel(5.0F), channel(3.0F), channel(1.0F)};
}

} // namespace

const SupplyKindLook *DesktopSupply::kind_look(const SupplyDrop &drop, const int type) {
  const auto &kinds = drop.settings().kinds;
  if (type < 0 || type >= static_cast<int>(kinds.size())) {
    return nullptr;
  }
  return &kinds[static_cast<std::size_t>(type)].look;
}

int DesktopSupply::crate_of(const int type, const SupplyKindLook *kind) {
  if (kind != nullptr && kind->crate >= 0) {
    return kind->crate;
  }
  return type >= 0 && type < supply_kind_count ? type : 0;
}

void DesktopSupply::dress_crate(const int type, const SupplyKindLook *kind, const std::size_t first,
                                std::vector<WorldItem> &items) const {
  if (kind == nullptr) {
    return;
  }
  const int image = type >= 0 && type < static_cast<int>(kind_images_.size())
                        ? kind_images_[static_cast<std::size_t>(type)]
                        : -1;
  for (std::size_t index = first; index < items.size(); ++index) {
    WorldItem &item = items[index];
    if (image >= 0) {
      item.paint = image;
      item.paint_any = true;
    }
    if (kind->tint) {
      item.colour = std::array<float, 4>{(*kind->tint)[0], (*kind->tint)[1], (*kind->tint)[2], 1};
    }
    item.label_colour = kind->label_tint;
    item.sheen = kind->sheen;
  }
}

std::array<float, 3> DesktopSupply::glow_colour(const SupplyDrop &drop, const int type) const {
  const SupplyKindLook *kind = kind_look(drop, type);
  if (kind != nullptr && kind->glow_cycle > 0.0F) {
    const double seconds = static_cast<double>(SDL_GetTicks()) / 1000.0;
    return hue_colour(static_cast<float>(std::fmod(seconds * kind->glow_cycle, 1.0)));
  }
  if (kind != nullptr && kind->glow) {
    return *kind->glow;
  }
  if (kind != nullptr && kind->tint) {
    return *kind->tint;
  }
  return look_.glow_colours[static_cast<std::size_t>(effect_of(drop, type))];
}

} // namespace phantomcrash
