// The supply drops' settings (data/world/training.json "supply") and
// routes (tables/battle_scenes.json "supply_routes").
#include "phantomcrash/supply_drop.hpp"

#include "phantomcrash/json_fields.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace phantomcrash {

namespace {

using json::JsonValue;

Vec3 vec3(const JsonValue &value, const char *name) {
  const auto v = json::numbers<3>(value, name);
  return {v[0], v[1], v[2]};
}

SupplyEffect effect_named(const std::string &name) {
  if (name == "money") {
    return SupplyEffect::money;
  }
  if (name == "repair") {
    return SupplyEffect::repair;
  }
  if (name == "ammo") {
    return SupplyEffect::ammo;
  }
  throw std::runtime_error("Unknown supply effect " + name);
}

SupplyModel parse_model(const JsonValue &value, SupplyModel model) {
  const auto mesh = [&](const char *key, const int fallback) {
    return static_cast<int>(value.number_or(key, fallback));
  };
  model.model = value.string_or("model", model.model);
  model.body = mesh("body", model.body);
  model.shell = mesh("shell", model.shell);
  if (const JsonValue *doors = value.find("doors")) {
    const auto v = json::numbers<2>(*doors, "doors");
    model.doors = {static_cast<int>(v[0]), static_cast<int>(v[1])};
  }
  model.spinner = mesh("spinner", model.spinner);
  model.wreck = mesh("wreck", model.wreck);
  model.first_pickup = mesh("first_pickup", model.first_pickup);
  return model;
}

SupplyLook parse_look(const JsonValue &value) {
  SupplyLook look;
  if (const auto *pickups = value.find("pickups")) {
    look.pickups = parse_supply_pickup_look(*pickups);
  }
  if (const JsonValue *models = value.find("models")) {
    const auto &list = models->array();
    for (std::size_t index = 0; index < look.models.size() && index < list.size(); ++index) {
      look.models[index] = parse_model(list[index], look.models[index]);
    }
  }
  if (const JsonValue *hinges = value.find("door_hinges")) {
    const auto &list = hinges->array();
    for (std::size_t index = 0; index < look.door_hinges.size() && index < list.size(); ++index) {
      look.door_hinges[index] = vec3(list[index], "door_hinges");
    }
  }
  if (const JsonValue *turns = value.find("door_turns")) {
    look.door_turns = json::numbers<2>(*turns, "door_turns");
  }
  look.pickup_scale = static_cast<float>(value.number_or("pickup_scale", look.pickup_scale));
  const auto number = [&](const char *key, const float fallback) {
    return static_cast<float>(value.number_or(key, fallback));
  };
  look.glow_texture = value.string_or("glow_texture", look.glow_texture);
  look.glow_size = number("glow_size", look.glow_size);
  look.glow_pull = number("glow_pull", look.glow_pull);
  look.glow_alpha = number("glow_alpha", look.glow_alpha);
  look.tint_pulse = number("tint_pulse", look.tint_pulse);
  look.tint_base = number("tint_base", look.tint_base);
  if (const JsonValue *colours = value.find("glow_colours")) {
    const auto &list = colours->array();
    for (std::size_t index = 0; index < look.glow_colours.size() && index < list.size(); ++index) {
      look.glow_colours[index] = json::numbers<3>(list[index], "glow_colours");
    }
  }
  if (const JsonValue *effects = value.find("take_effects")) {
    const auto groups = json::numbers<3>(*effects, "take_effects");
    for (std::size_t index = 0; index < groups.size(); ++index) {
      look.take_effects[index] = static_cast<int>(groups[index]);
    }
  }
  look.take_rate = number("take_rate", look.take_rate);
  look.take_bursts = static_cast<int>(value.number_or("take_bursts", look.take_bursts));
  look.crash_radius = static_cast<float>(value.number_or("crash_radius", look.crash_radius));
  look.crash_seconds = static_cast<float>(value.number_or("crash_seconds", look.crash_seconds));
  look.crash_effect = static_cast<int>(value.number_or("crash_effect", look.crash_effect));
  look.burst_effect = static_cast<int>(value.number_or("burst_effect", look.burst_effect));
  look.take_sound = value.string_or("take_sound", look.take_sound);
  return look;
}

} // namespace

SupplyPickupLook parse_supply_pickup_look(const JsonValue &value) {
  SupplyPickupLook look;
  look.model = json::required(value, "model").string();
  look.scale = static_cast<float>(value.number_or("scale", look.scale));
  look.carrier_model = value.string_or("carrier_model", "");
  look.carrier_light_model = value.string_or("carrier_light_model", "");
  look.light_spin_per_second = static_cast<float>(value.number_or("light_spin_per_second", 0));
  if (const auto *pulse = value.find("pulse")) {
    const auto number = [&](const char *key) {
      return static_cast<float>(json::required(*pulse, key).number());
    };
    look.pulse =
        SupplyPickupPulse{number("frame_rate"),   number("step"),           number("fast_frames"),
                          number("fast_rate"),    number("life_numerator"), number("colour_base"),
                          number("colour_steps"), number("colour_divisor")};
    if (look.pulse->frame_rate <= 0 || look.pulse->colour_divisor <= 0) {
      throw std::runtime_error("Invalid supply pickup pulse");
    }
  }
  const auto &images = json::required(value, "images").array();
  if (images.size() != look.images.size()) {
    throw std::runtime_error("Supply pickup images must cover all ten kinds");
  }
  for (std::size_t i = 0; i < images.size(); ++i) {
    look.images[i] = images[i].string();
  }
  return look;
}

SupplySettings parse_supply_settings(const JsonValue &value) {
  SupplySettings s;
  const auto number = [&](const char *key, const float fallback) {
    return static_cast<float>(value.number_or(key, fallback));
  };
  s.tick_rate = std::max(number("tick_rate", s.tick_rate), 1.0F);
  s.speed = number("speed", s.speed);
  s.pickups = std::clamp(static_cast<int>(value.number_or("pickups", s.pickups)), 0, 16);
  s.drop_ticks = number("drop_ticks", s.drop_ticks);
  s.hatch_open = number("hatch_open", s.hatch_open);
  s.hatch_speed = number("hatch_speed", s.hatch_speed);
  s.spin_speed = number("spin_speed", s.spin_speed);
  if (value.find("hold") != nullptr) {
    s.hold = vec3(json::required(value, "hold"), "hold");
  }
  s.fall_speed = number("fall_speed", s.fall_speed);
  s.rest_height = number("rest_height", s.rest_height);
  s.lifetime_ticks = number("lifetime_ticks", s.lifetime_ticks);
  s.reach = number("reach", s.reach);
  s.blink_fast_ticks = number("blink_fast_ticks", s.blink_fast_ticks);
  s.blink_fast = number("blink_fast", s.blink_fast);
  s.blink_step = number("blink_step", s.blink_step);
  s.carrier_radius = number("carrier_radius", s.carrier_radius);
  s.pickup_radius = number("pickup_radius", s.pickup_radius);
  s.shot_bounty = number("shot_bounty", s.shot_bounty);
  s.wreck_keep = number("wreck_keep", s.wreck_keep);
  s.wreck_sink = number("wreck_sink", s.wreck_sink);
  s.wreck_ticks = number("wreck_ticks", s.wreck_ticks);
  if (const JsonValue *chances = value.find("chances")) {
    s.chances = json::numbers<supply_kind_count>(*chances, "chances");
  }
  if (const JsonValue *kinds = value.find("kinds")) {
    const auto &list = kinds->array();
    if (list.size() != supply_kind_count) {
      throw std::runtime_error("supply kinds: expected " + std::to_string(supply_kind_count));
    }
    for (std::size_t index = 0; index < list.size(); ++index) {
      s.kinds[index].effect = effect_named(list[index].string_or("effect", "money"));
      s.kinds[index].amount = static_cast<float>(list[index].number_or("amount", 0.0));
    }
  }
  if (const JsonValue *look = value.find("look")) {
    s.look = parse_look(*look);
  }
  return s;
}

std::vector<SupplyRoute> parse_supply_routes(const JsonValue &document, const int stage) {
  std::vector<SupplyRoute> routes;
  const JsonValue *stages = document.find("supply_routes");
  const JsonValue *entry = stages != nullptr ? stages->find(std::to_string(stage)) : nullptr;
  if (entry == nullptr) {
    return routes;
  }
  return parse_supply_route_list(*entry);
}

std::vector<SupplyRoute> parse_supply_route_list(const JsonValue &entry) {
  std::vector<SupplyRoute> routes;
  for (const JsonValue &value : entry.array()) {
    SupplyRoute route;
    for (const JsonValue &segment : json::required(value, "segments").array()) {
      const auto &points = segment.array();
      if (points.size() != 4) {
        throw std::runtime_error("supply route: a segment has four points");
      }
      route.segments.push_back({vec3(points[0], "supply point"), vec3(points[1], "supply point"),
                                vec3(points[2], "supply point"), vec3(points[3], "supply point")});
    }
    route.closing = static_cast<int>(value.number_or("closing", 0));
    route.drop = static_cast<int>(value.number_or("drop", 0));
    route.leave = static_cast<int>(value.number_or("leave", 0));
    if (!route.segments.empty()) {
      routes.push_back(std::move(route));
    }
  }
  return routes;
}

} // namespace phantomcrash
