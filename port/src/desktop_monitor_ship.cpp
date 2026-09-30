#include "desktop_monitor_ship.hpp"

#include "phantomcrash/json_fields.hpp"
#include "phantomcrash/supply_drop.hpp"

#include <algorithm>
#include <cmath>

namespace phantomcrash {
namespace {
Vec3 vector(const json::JsonValue &value) {
  const auto xyz = json::numbers<3>(value, "ship route point");
  return {xyz[0], xyz[1], xyz[2]};
}
std::vector<int> integers(const json::JsonValue &value) {
  std::vector<int> out;
  for (const auto &entry : value.array()) {
    out.push_back(static_cast<int>(entry.number()));
  }
  return out;
}
bool contains(const std::vector<int> &values, const int value) {
  return std::ranges::find(values, value) != values.end();
}
} // namespace

MonitorShip::MonitorShip(WorldRenderer &world, const DataRoots &roots, const int stage) {
  const auto path = roots.find_asset("tables/arena_monitor.json");
  if (!path || stage < 0) {
    return;
  }
  try {
    const auto table = json::read_file(*path);
    const auto *route = json::required(table, "routes").find(std::to_string(stage));
    if (route == nullptr) {
      return;
    }
    for (const auto &entry : route->array()) {
      const auto &points = entry.array();
      if (points.size() != 4) {
        throw std::runtime_error("Ship route needs four control points per segment");
      }
      route_.push_back(
          {vector(points[0]), vector(points[1]), vector(points[2]), vector(points[3])});
    }
    speed_ = static_cast<float>(json::required(table, "segment_per_second").number());
    hinge_speed_ = static_cast<float>(json::required(table, "hinge_per_second").number());
    spin_speed_ = static_cast<float>(json::required(table, "spin_per_second").number());
    hinge_max_ = static_cast<float>(json::required(table, "hinge_max").number());
    open_segments_ = integers(json::required(table, "open_segments"));
    close_segments_ = integers(json::required(table, "close_segments"));
    const auto &look = json::required(table, "models").array().at(stage % 2);
    model_ = world.model(look.string_or("model", ""));
    const auto *model = world.model_data(model_);
    if (model == nullptr) {
      return;
    }
    const auto screens = integers(json::required(look, "screens"));
    for (std::size_t i = 0; i < model->nodes.size(); ++i) {
      const auto &name = model->nodes[i].name;
      if (!name.starts_with("mesh")) {
        continue;
      }
      const int mesh = std::stoi(name.substr(4));
      Part part;
      part.node = static_cast<int>(i);
      part.name = name;
      part.screen = contains(screens, mesh);
      for (const auto &hinge : json::required(look, "hinges").array()) {
        if (contains(integers(json::required(hinge, "meshes")), mesh)) {
          part.hinge_x = static_cast<float>(hinge.number_or("x", 0));
          part.hinge_sign = static_cast<float>(hinge.number_or("sign", 0));
        }
      }
      for (const auto &spinner : json::required(look, "spinners").array()) {
        if (spinner.number_or("mesh", -1) == mesh) {
          part.spinner = true;
          part.pivot = vector(json::required(spinner, "pivot"));
        }
      }
      parts_.push_back(std::move(part));
    }
    tick(0);
  } catch (const std::exception &error) {
    SDL_Log("Arena monitor ship: %s", error.what());
    model_ = -1;
  }
}

void MonitorShip::tick(const float seconds) {
  if (model_ < 0 || route_.empty()) {
    return;
  }
  along_ += seconds * speed_;
  while (along_ >= 1.0F) {
    along_ -= 1.0F;
    segment_ = (segment_ + 1) % static_cast<int>(route_.size());
    if (contains(open_segments_, segment_)) {
      hinge_direction_ = 1;
    } else if (contains(close_segments_, segment_)) {
      hinge_direction_ = -1;
    }
  }
  hinge_ = std::clamp(hinge_ + hinge_direction_ * seconds * hinge_speed_, 0.0F, hinge_max_);
  spin_ = wrap_angle(spin_ + seconds * spin_speed_);
  const auto &curve = route_[static_cast<std::size_t>(segment_)];
  const float t = along_, u = 1.0F - t;
  const Vec3 direction = (curve[1] - curve[0]) * (3 * u * u) + (curve[2] - curve[1]) * (6 * u * t) +
                         (curve[3] - curve[2]) * (3 * t * t);
  // The ship's local +Z follows its route; unlike a robot it faces +Z.
  world_ = rotation_y(std::atan2(direction.x, direction.z)) * translation(bezier_point(curve, t));
}

void MonitorShip::add_items(std::vector<WorldItem> &items) const {
  if (model_ < 0 || route_.empty()) {
    return;
  }
  for (const Part &part : parts_) {
    WorldItem item;
    item.model = model_;
    item.node = part.node;
    item.world = world_;
    if (part.hinge_sign != 0) {
      item.world = translation({-part.hinge_x, 0, 0}) * rotation_z(part.hinge_sign * hinge_) *
                   translation({part.hinge_x, 0, 0}) * world_;
    } else if (part.spinner) {
      const Vec3 p = part.pivot;
      item.world = translation({-p.x, -p.y, -p.z}) * rotation_y(spin_) * translation(p) * world_;
    }
    if (part.screen) {
      item.monitor_nodes.push_back(part.name);
    }
    items.push_back(std::move(item));
  }
}

} // namespace phantomcrash
