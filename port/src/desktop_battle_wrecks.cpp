#include "desktop_battle_view.hpp"
#include "phantomcrash/money.hpp"

namespace phantomcrash {
namespace {
StageCollision posed_collision(const WorldRenderer &world, const std::vector<WorldItem> &items) {
  std::vector<CollisionTriangle> triangles;
  for (const auto &item : items) {
    const auto *model = world.model_data(item.model);
    if (!model) {
      continue;
    }
    for (const auto &draw : model->draws) {
      const auto &material = model->materials.at(static_cast<std::size_t>(draw.material));
      if (draw.pass != 0 || material.additive || material.alpha == AlphaMode::blend ||
          (item.node >= 0 && item.node != draw.node)) {
        continue;
      }
      const auto node = static_cast<std::size_t>(draw.node);
      const Mat4 transform = item.node_worlds.size() == model->nodes.size()
                                 ? item.node_worlds[node]
                                 : model->nodes[node].world * item.world;
      for (std::size_t offset = draw.first_index; offset + 2 < draw.first_index + draw.index_count;
           offset += 3) {
        CollisionTriangle triangle;
        Vec3 average_normal{};
        for (std::size_t corner = 0; corner < 3; ++corner) {
          const auto &vertex = model->vertices[model->indices[offset + corner]];
          triangle.points[corner] = transform_point(
              {vertex.position[0], vertex.position[1], vertex.position[2]}, transform);
          average_normal =
              average_normal +
              (transform_point({vertex.normal[0], vertex.normal[1], vertex.normal[2]}, transform) -
               transform_point({}, transform));
        }
        Vec3 normal =
            cross(triangle.points[1] - triangle.points[0], triangle.points[2] - triangle.points[0]);
        if (length(normal) < 1e-6F) {
          continue;
        }
        if (dot(normal, average_normal) < 0) {
          std::swap(triangle.points[1], triangle.points[2]);
          normal = normal * -1.0F;
        }
        triangle.normal = normalize(normal);
        triangle.surface = 0;
        triangles.push_back(triangle);
      }
    }
  }
  return StageCollision{std::move(triangles), {}};
}
} // namespace

void BattleView::sync_wrecks(Battle &battle) {
  const auto &wrecks = battle.wrecks().items();
  while (wreck_items_.size() < wrecks.size()) {
    const std::size_t index = wreck_items_.size();
    const auto &wreck = wrecks[index];
    auto &items = wreck_items_.emplace_back();
    wreck_solid_.push_back(false);
    RobotVisual visual(world_, wreck.assembly);
    visual.add_items(wreck.pose, wreck.root, items, &wreck.shoulders, wreck.lost);
    auto collision = posed_collision(world_, items);
    SDL_Log("Robot wreck: created=%zu parts=%zu triangles=%zu burn_left=%.3f reward=%d NY", index,
            items.size(), collision.walls().size(), wreck.burn_left,
            battle.settings().robot_wrecks.reward_ny);
    battle.wrecks().set_collision(index, std::move(collision));
  }
  for (const auto &event : battle.events()) {
    if (event.kind == BattleEventKind::wreck_destroyed) {
      SDL_Log("Robot wreck: destroyed by=%d reward=%.0f NY (no shootdown)", event.robot,
              event.amount * ny_per_stored_unit);
    }
  }
  for (std::size_t i = 0; i < wrecks.size(); ++i) {
    if (wrecks[i].solid() && !wreck_solid_[i]) {
      SDL_Log("Robot wreck: solid=%zu fire extinguished, persistent collision enabled", i);
      wreck_solid_[i] = true;
    }
    if (wrecks[i].removed) {
      wreck_items_[i].clear();
    }
  }
}

void BattleView::clear_wrecks() {
  wreck_items_.clear();
  wreck_solid_.clear();
}

void BattleView::add_wreck_items(const Battle &battle, std::vector<WorldItem> &items) const {
  const auto &wrecks = battle.wrecks().items();
  for (std::size_t i = 0; i < wreck_items_.size() && i < wrecks.size(); ++i) {
    if (!wrecks[i].removed) {
      items.insert(items.end(), wreck_items_[i].begin(), wreck_items_[i].end());
    }
  }
}
} // namespace phantomcrash
