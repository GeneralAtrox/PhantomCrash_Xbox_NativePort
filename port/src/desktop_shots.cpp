#include "desktop_shots.hpp"

#include "desktop_cockpit.hpp"
#include "phantomcrash/skeleton.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace phantomcrash {
namespace {

constexpr float half = 0.5F; // grid_mdl and the sprites span -0.5 to 0.5
constexpr float byte_max = 255.0F;

// +z along `direction`, for row vectors (the rows are the new axes). The
// looks' models lie along z, their exhaust trailing toward -z.
Mat4 heading_along(const Vec3 direction) {
  const float size = length(direction);
  const Vec3 z = size > 0.0F ? direction * (1.0F / size) : Vec3{0.0F, 0.0F, 1.0F};
  const Vec3 helper = std::fabs(z.y) < 0.99F ? Vec3{0.0F, 1.0F, 0.0F} : Vec3{1.0F, 0.0F, 0.0F};
  const Vec3 x = normalize(cross(helper, z));
  const Vec3 y = cross(z, x);
  Mat4 m;
  const Vec3 rows[] = {x, y, z};
  for (int row = 0; row < 3; ++row) {
    m.at(row, 0) = rows[row].x;
    m.at(row, 1) = rows[row].y;
    m.at(row, 2) = rows[row].z;
  }
  return m;
}

// The camera's turn: a model's xy plane faces the eye (0x066223).
Mat4 facing(const Mat4 &view) {
  Mat4 m;
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      m.at(row, column) = view.at(column, row);
    }
  }
  return m;
}

Vec3 between(const Vec3 from, const Vec3 to, const float t) {
  return from + (to - from) * t;
}

} // namespace

DesktopShots::DesktopShots(WorldRenderer &world, const EffectBank &bank)
    : bank_(&bank), fence_(bank.fence) {
  for (const int mesh : bank.fence.meshes) {
    fence_meshes_.emplace(mesh, load_mesh(world, EffectPicture{bank.fence.model, mesh}));
  }
  for (const ShotLook &look : bank.shots) {
    looks_.push_back(load_mesh(world, look.model));
    look_sprites_.push_back(look.sprite.empty() ? -1 : world.texture(look.sprite));
  }
  exhaust_ = load_mesh(world, bank.exhaust);
  for (const EffectFlash &flash : bank.flashes) {
    std::vector<int> &textures = flash_textures_.emplace_back();
    for (const std::string &frame : flash.frames) {
      textures.push_back(frame.empty() ? -1 : world.texture(frame));
    }
  }
}

DesktopShots::Mesh DesktopShots::load_mesh(WorldRenderer &world,
                                           const std::optional<EffectPicture> &picture) {
  Mesh out;
  if (!picture) {
    return out;
  }
  const int model = world.model(picture->model);
  const ModelData *data = world.model_data(model);
  if (data == nullptr) {
    return out;
  }
  const auto nodes = converter_mesh_nodes(*data);
  if (picture->mesh < 0 || picture->mesh >= static_cast<int>(nodes.size()) ||
      nodes[static_cast<std::size_t>(picture->mesh)].empty()) {
    return out;
  }
  out.model = model;
  out.nodes = nodes[static_cast<std::size_t>(picture->mesh)];
  return out;
}

void DesktopShots::add_mesh(const Mesh &mesh, const Mat4 &world, std::vector<WorldItem> &items) {
  for (const int node : mesh.nodes) {
    WorldItem item;
    item.model = mesh.model;
    item.node = node;
    item.world = world;
    items.push_back(std::move(item));
  }
}

void DesktopShots::tick(const Battle &battle, EffectSystem &effects, const float seconds) {
  fence_.tick(seconds);
  const EffectBank &bank = *bank_;
  for (const BattleEvent &event : battle.events()) {
    if (event.kind == BattleEventKind::impact && event.weapon != nullptr) {
      if (event.surface && starts_impact(*event.weapon, event.on_robot)) {
        fence_.start(*event.surface);
      }
      // 0x098eda: where the weapon's landing case starts its impact.
      const int impact = event.weapon->impact;
      if (impact < 0 || impact >= static_cast<int>(bank.impacts.size()) ||
          !starts_impact(*event.weapon, event.on_robot)) {
        continue;
      }
      const ShotImpact &data = bank.impacts[static_cast<std::size_t>(impact)];
      effects.start(data.group, event.at, event.at, {0.0F, 1.0F, 0.0F}, data.flash_scale);
    } else if (event.kind == BattleEventKind::destroyed) {
      for (const int group : bank.wreck_groups) {
        effects.start(group, event.at, event.at);
      }
    } else if (event.kind == BattleEventKind::effect) {
      effects.start(event.slot, event.at, event.at);
    }
  }
}

void DesktopShots::add_items(const Battle &battle, const Mat4 &view,
                             std::vector<WorldItem> &items) {
  for (const auto &frame : fence_.frames()) {
    const auto &mesh = fence_meshes_.at(frame.mesh);
    for (const int node : mesh.nodes) {
      WorldItem item;
      item.model = mesh.model;
      item.node = node;
      item.world = frame.world;
      item.optical = frame.refraction;
      items.push_back(std::move(item));
    }
  }
  const Mat4 camera = facing(view);
  std::uniform_real_distribution<float> roll(0.0F, 2.0F * std::numbers::pi_v<float>);
  for (const Projectile &shot : battle.armory().projectiles()) {
    const int look = shot.weapon->look;
    if (look < 0 || look >= static_cast<int>(looks_.size())) {
      continue;
    }
    const ShotLook &data = bank_->shots[static_cast<std::size_t>(look)];
    const Mesh &mesh = looks_[static_cast<std::size_t>(look)];
    if (mesh.model < 0) {
      continue;
    }
    const Mat4 place =
        (data.billboard ? camera : heading_along(shot.velocity)) * translation(shot.position);
    add_mesh(mesh, scaling({data.scale, data.scale, data.scale}) * place, items);
    if (data.trail && exhaust_.model >= 0) {
      // 0x09831e: the flame behind, turned about the flight each frame.
      const Mat4 flame = scaling(data.exhaust_scale) * rotation_z(roll(random_)) *
                         translation({0.0F, 0.0F, -data.exhaust_offset});
      add_mesh(exhaust_, flame * place, items);
    }
  }
}

void DesktopShots::add_sprites(const Battle &battle, const EffectSystem &effects,
                               std::vector<WorldSprite> &sprites) const {
  for (const Projectile &shot : battle.armory().projectiles()) {
    const int look = shot.weapon->look;
    if (look < 0 || look >= static_cast<int>(look_sprites_.size()) ||
        look_sprites_[static_cast<std::size_t>(look)] < 0) {
      continue;
    }
    WorldSprite sprite;
    sprite.texture = look_sprites_[static_cast<std::size_t>(look)];
    sprite.position = shot.position;
    sprite.size = bank_->shots[static_cast<std::size_t>(look)].scale * half;
    sprite.additive = true;
    sprites.push_back(sprite);
  }
  const float t = effects.between();
  for (const EffectFlashState &flash : effects.flashes()) {
    const auto &textures = flash_textures_[static_cast<std::size_t>(flash.flash)];
    const int frame = std::clamp(flash.age, 0, static_cast<int>(textures.size()) - 1);
    if (textures.empty() || textures[static_cast<std::size_t>(frame)] < 0) {
      continue;
    }
    WorldSprite sprite;
    sprite.texture = textures[static_cast<std::size_t>(frame)];
    sprite.position = between(flash.previous, flash.position, t);
    sprite.size = flash.scale * half;
    sprite.colour = {1.0F, 1.0F, 1.0F, static_cast<float>(flash.alpha) / byte_max};
    sprite.additive = bank_->flashes[static_cast<std::size_t>(flash.flash)].additive;
    sprites.push_back(sprite);
  }
}

} // namespace phantomcrash
