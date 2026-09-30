#include "desktop_stage_objects.hpp"

#include <SDL3/SDL.h>

namespace phantomcrash {

int load_stage_objects(WorldRenderer *world, const DataRoots &roots, const int stage,
                       Battle &battle) {
  const auto file = roots.find_asset("tables/stage_objects.json");
  if (!file) {
    return -1;
  }
  try {
    const auto document = json::read_file(*file);
    for (const auto &entry : document.array()) {
      if (entry.number_or("stage", -1) != stage) {
        continue;
      }
      // A live area's wrecks survive leaving and reentering its screen.
      if (battle.objects() == nullptr) {
        battle.set_objects(parse_stage_objects(document, stage));
      }
      return world != nullptr && world->available()
                 ? world->model(entry.string_or("door_model", ""))
                 : -1;
    }
  } catch (const std::exception &error) {
    SDL_Log("Stage objects: %s", error.what());
  }
  return -1;
}

void add_object_items(const Battle &battle, const int model, std::vector<WorldItem> &items) {
  if (battle.objects() == nullptr || model < 0) {
    return;
  }
  for (const auto &object : battle.objects()->objects()) {
    if (object.visible_mesh() < 0) {
      continue;
    }
    WorldItem item;
    item.model = model;
    item.node = object.visible_mesh(); // effect model node n owns mesh n
    item.world = object.world;
    items.push_back(std::move(item));
  }
}

} // namespace phantomcrash
