#include "desktop_effects.hpp"

#include "phantomcrash/json.hpp"

#include <SDL3/SDL.h>

#include <exception>
#include <utility>

namespace phantomcrash {
namespace {

constexpr const char *bank_asset = "tables/effects.json";
constexpr float half = 0.5F;           // 0x17accc: the size is the sprite's whole width
constexpr float cluster_centre = 3.5F; // 0x19f918: a cluster's centre is drawn larger
constexpr float short_streak = 0.2F;   // 0x3e4ccccd: streak_short's share of a frame's motion
constexpr float byte_max = 255.0F;

Vec3 between(const Vec3 from, const Vec3 to, const float t) {
  return from + (to - from) * t;
}

} // namespace

std::unique_ptr<DesktopEffects> DesktopEffects::load(WorldRenderer *world, const DataRoots &roots) {
  if (world == nullptr || !world->available()) {
    return nullptr;
  }
  const auto file = roots.find_asset(bank_asset);
  if (!file) {
    SDL_Log("%s is missing: convert the robots again for the effects", bank_asset);
    return nullptr;
  }
  try {
    auto bank = std::make_shared<const EffectBank>(parse_effect_bank(json::read_file(*file)));
    const int atlas = world->texture(bank->atlas.texture);
    if (atlas < 0) {
      SDL_Log("The effect atlas %s is missing", bank->atlas.texture.c_str());
      return nullptr;
    }
    return std::make_unique<DesktopEffects>(std::move(bank), atlas);
  } catch (const std::exception &error) {
    SDL_Log("%s: %s", bank_asset, error.what());
    return nullptr;
  }
}

DesktopEffects::DesktopEffects(std::shared_ptr<const EffectBank> bank, const int atlas)
    : system_(std::move(bank)), atlas_(atlas) {}

void DesktopEffects::add_sprites(std::vector<WorldSprite> &sprites) const {
  const EffectBank &bank = system_.bank();
  const float t = system_.between();
  const auto cells = static_cast<int>(bank.atlas.cells.size());
  for (const EffectEmitter &emitter : system_.emitters()) {
    const EffectDefinition &d = bank.definitions[static_cast<std::size_t>(emitter.definition)];
    const bool streak = (emitter.flags & effect_flags::streaks) != 0;
    const float reach = (emitter.flags & effect_flags::streak_short) != 0 ? short_streak : 1.0F;
    for (const EffectParticle &particle : emitter.particles) {
      if (!particle.alive) {
        continue;
      }
      WorldSprite sprite;
      sprite.texture = atlas_;
      sprite.position = between(particle.previous, particle.position, t);
      sprite.size = particle.scale * particle.size * half;
      if (particle.kind == 1) {
        sprite.size *= cluster_centre;
      }
      sprite.rotation = particle.rotation;
      if (streak) {
        sprite.streak = true;
        sprite.end = sprite.position + particle.velocity * reach;
      }
      if (particle.cell >= 0 && particle.cell < cells) {
        const auto &corner = bank.atlas.cells[static_cast<std::size_t>(particle.cell)];
        sprite.uv = {corner[0], corner[1], corner[0] + bank.atlas.cell_size[0],
                     corner[1] + bank.atlas.cell_size[1]};
      }
      for (std::size_t c = 0; c < 4; ++c) {
        sprite.colour[c] = static_cast<float>(particle.colour[c]) / byte_max;
      }
      sprite.additive = d.additive;
      sprites.push_back(sprite);
    }
  }
}

} // namespace phantomcrash
