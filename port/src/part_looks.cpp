#include "phantomcrash/part_looks.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace phantomcrash {
namespace {

// What tells two meshes apart: the models a type draws. Types copied for
// another game's paints draw the same models and are one look.
using MeshKey = std::vector<int>;

// The mesh kinds by PartKind: body, legs, arm, shoulder; chips and modules
// have none.
std::optional<std::size_t> mesh_kind(const PartKind kind) {
  switch (kind) {
  case PartKind::body:
    return 0;
  case PartKind::legs:
    return 1;
  case PartKind::arm:
    return 2;
  case PartKind::shoulder:
    return 3;
  case PartKind::chip:
  case PartKind::module:
    break;
  }
  return std::nullopt;
}

const std::vector<int> &part_map(const RobotCatalogue &catalogue, const std::size_t kind) {
  const std::array<const std::vector<int> *, 4> maps{
      &catalogue.body_parts, &catalogue.leg_parts, &catalogue.arm_parts, &catalogue.shoulder_parts};
  return *maps[kind];
}

std::size_t type_count(const RobotCatalogue &catalogue, const std::size_t kind) {
  const std::array<std::size_t, 4> counts{catalogue.body_types.size(), catalogue.leg_types.size(),
                                          catalogue.arm_types.size(),
                                          catalogue.shoulder_types.size()};
  return counts[kind];
}

// The type part `id` draws, -1 when it has none.
int mesh_type(const RobotCatalogue &catalogue, const std::size_t kind, const int id) {
  const std::vector<int> &parts = part_map(catalogue, kind);
  if (id < 0 || id >= static_cast<int>(parts.size())) {
    return -1;
  }
  const int type = parts[static_cast<std::size_t>(id)];
  return type >= 0 && type < static_cast<int>(type_count(catalogue, kind)) ? type : -1;
}

MeshKey mesh_key(const RobotCatalogue &catalogue, const std::size_t kind, const int type) {
  const auto at = static_cast<std::size_t>(type);
  switch (kind) {
  case 0: {
    const auto &body = catalogue.body_types[at];
    MeshKey key{body.model};
    for (const auto &[legs, model] : body.models_by_leg_type) {
      key.insert(key.end(), {legs, model});
    }
    return key;
  }
  case 1:
    return {catalogue.leg_types[at].model};
  case 2:
    return {catalogue.arm_types[at].models[0], catalogue.arm_types[at].models[1]};
  default:
    return {catalogue.shoulder_types[at].models[0], catalogue.shoulder_types[at].models[1]};
  }
}

// What a weapon look must share with the weapon: its kind, and an arm's
// class; nothing for bodies and legs.
std::tuple<int, int> fit_key(const RobotCatalogue &catalogue, const PartTables &parts,
                             const PartLookRules &rules, const std::size_t kind, const int id) {
  if (kind < 2) {
    return {0, 0};
  }
  const auto &records = kind == 2 ? parts.arm_weapons : parts.shoulder_weapons;
  int weapon_kind = 0;
  if (rules.weapons_keep_kind && id >= 0 && id < static_cast<int>(records.size())) {
    weapon_kind = records[static_cast<std::size_t>(id)].kind;
  }
  int arm_class = 0;
  if (kind == 2 && rules.arms_keep_class) {
    arm_class =
        catalogue.arm_types[static_cast<std::size_t>(mesh_type(catalogue, kind, id))].arm_class;
  }
  return {weapon_kind, arm_class};
}

const std::vector<int> no_choices;

} // namespace

PartLookRules parse_part_look_rules(const json::JsonValue &document) {
  try {
    PartLookRules rules;
    rules.enabled = document.boolean_or("enabled", rules.enabled);
    rules.weapons_keep_kind = document.boolean_or("weapons_keep_kind", rules.weapons_keep_kind);
    rules.arms_keep_class = document.boolean_or("arms_keep_class", rules.arms_keep_class);
    return rules;
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string("part_looks.json: ") + error.what());
  }
}

std::optional<LookArrows> parse_look_arrows(const json::JsonValue &screen) {
  const json::JsonValue *style = screen.find("look_arrows");
  if (style == nullptr) {
    return std::nullopt;
  }
  const json::JsonValue *arrows = style->find("arrows");
  const std::vector<Sprite> sprites =
      arrows != nullptr ? parse_sprites(*arrows) : std::vector<Sprite>{};
  if (sprites.size() != 2) {
    throw std::runtime_error("look_arrows.arrows: needs left and right");
  }
  return LookArrows{sprites[0], sprites[1]};
}

PartLooks::PartLooks(const RobotCatalogue &catalogue, const PartTables &parts,
                     const PartLookRules &rules) {
  if (!rules.enabled) {
    return;
  }
  for (std::size_t kind = 0; kind < choices_.size(); ++kind) {
    const int count = static_cast<int>(part_map(catalogue, kind).size());
    // The first part to draw each mesh stands for it, grouped by what a
    // look must share.
    std::map<std::tuple<int, int>, std::vector<std::pair<MeshKey, int>>> meshes;
    for (int id = 0; id < count; ++id) {
      const int type = mesh_type(catalogue, kind, id);
      if (type < 0) {
        continue;
      }
      auto &group = meshes[fit_key(catalogue, parts, rules, kind, id)];
      MeshKey key = mesh_key(catalogue, kind, type);
      if (std::none_of(group.begin(), group.end(),
                       [&key](const auto &mesh) { return mesh.first == key; })) {
        group.emplace_back(std::move(key), id);
      }
    }
    auto &out = choices_[kind];
    out.resize(static_cast<std::size_t>(count));
    for (int id = 0; id < count; ++id) {
      const int type = mesh_type(catalogue, kind, id);
      if (type < 0) {
        continue;
      }
      const MeshKey own = mesh_key(catalogue, kind, type);
      std::vector<int> &list = out[static_cast<std::size_t>(id)];
      list.push_back(id);
      for (const auto &[key, part] : meshes[fit_key(catalogue, parts, rules, kind, id)]) {
        if (key != own) {
          list.push_back(part);
        }
      }
    }
  }
}

const std::vector<int> &PartLooks::choices(const PartKind kind, const int id) const {
  const auto mesh = mesh_kind(kind);
  if (!mesh || id < 0 || id >= static_cast<int>(choices_[*mesh].size())) {
    return no_choices;
  }
  return choices_[*mesh][static_cast<std::size_t>(id)];
}

bool PartLooks::offers(const PartKind kind, const int id) const {
  return choices(kind, id).size() > 1;
}

int PartLooks::step(const PartKind kind, const int id, const int look, const int step) const {
  const std::vector<int> &list = choices(kind, id);
  if (list.size() < 2) {
    return -1;
  }
  const auto found = std::find(list.begin(), list.end(), look);
  const auto at = found == list.end() ? 0 : static_cast<int>(found - list.begin());
  const int size = static_cast<int>(list.size());
  const int next = ((at + step) % size + size) % size;
  return next == 0 ? -1 : list[static_cast<std::size_t>(next)];
}

bool has_mesh(const RobotCatalogue &catalogue, const PartKind kind, const int id) {
  const auto mesh = mesh_kind(kind);
  return mesh && mesh_type(catalogue, *mesh, id) >= 0;
}

int worn_part(const StockItem &item, const RobotCatalogue *catalogue) {
  if (item.look < 0 || (catalogue != nullptr && !has_mesh(*catalogue, item.kind, item.look))) {
    return item.id;
  }
  return item.look;
}

} // namespace phantomcrash
