// Parsing of data/ui/slai_area_select.json and the pack's blueprint maps.

#include "phantomcrash/slai_area_select_script.hpp"

#include "phantomcrash/json_fields.hpp"

#include <stdexcept>

namespace phantomcrash {
namespace {

using json::member;
using json::numbers;
using json::required;

std::array<float, 2> point(const json::JsonValue &value) {
  return numbers<2>(value, "point");
}

std::array<float, 4> box(const json::JsonValue &value) {
  return numbers<4>(value, "box");
}

SlaiAreaTitle parse_title(const json::JsonValue &value) {
  SlaiAreaTitle title;
  title.emblem_box = member(value, "emblem_box", box);
  title.label_at = member(value, "label_at", point);
  title.label_scale = static_cast<float>(value.number_or("label_scale", 1.0));
  title.venue_at = member(value, "venue_at", point);
  title.venue_scale = static_cast<float>(value.number_or("venue_scale", 1.0));
  title.layer = static_cast<int>(value.number_or("layer", title.layer));
  return title;
}

// The sprite drawn `scale` times its size about `anchor`.
void scale_about(Sprite &sprite, const std::array<float, 2> &anchor, const float scale) {
  const std::array<float, 2> size = sprite.size.value_or(
      std::array<float, 2>{sprite.rect[2] - sprite.rect[0], sprite.rect[3] - sprite.rect[1]});
  sprite.size = std::array<float, 2>{size[0] * scale, size[1] * scale};
  for (int axis = 0; axis < 2; ++axis) {
    sprite.position[axis] = anchor[axis] + (sprite.position[axis] - anchor[axis]) * scale;
  }
}

void shrink_picture(RumblingScript &rumbling, const float scale) {
  RumblingAreaSelect &style = rumbling.area_select;
  const std::array<float, 2> anchor = style.picture_frame.position;
  for (Sprite *sprite : {&style.picture_frame, &style.closed, &style.noise, &style.clear}) {
    scale_about(*sprite, anchor, scale);
  }
  RumblingArea &exit = rumbling.areas.back();
  for (Sprite *sprite : {&exit.picture, &exit.night_picture, &exit.label}) {
    scale_about(*sprite, anchor, scale);
  }
}

} // namespace

SlaiAreaSelectScript parse_slai_area_select(const json::JsonValue &document,
                                            SlaiRumbleHubScript hub) {
  SlaiAreaSelectScript script;
  script.rumbling = parse_rumbling(document);
  if (script.rumbling.areas.size() < 2) {
    throw std::runtime_error("slai_area_select.json: areas are the plate's template and EXIT");
  }
  script.hub = std::move(hub);
  try {
    const json::JsonValue &slai = required(document, "slai");
    script.maps = required(slai, "maps").string();
    for (const json::JsonValue &at : required(slai, "plates").array()) {
      script.plates.push_back(point(at));
    }
    script.exit_plate = member(slai, "exit_plate", point);
    script.plate_slide = static_cast<float>(required(slai, "plate_slide").number());
    script.plate_id = required(slai, "plate_id").string();
    script.picture_scale = static_cast<float>(slai.number_or("picture_scale", 1.0));
    script.picture_box = member(slai, "picture_box", box);
    script.picture_back = member(slai, "picture_back", parse_sprite);
    script.title = member(slai, "title", parse_title);
    script.help = required(slai, "help").string();
    script.closed_help = slai.string_or("closed_help", script.help);
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string("slai_area_select.json: ") + error.what());
  }
  shrink_picture(script.rumbling, script.picture_scale);
  return script;
}

std::optional<SlaiAreaSelectScript> load_slai_area_select(const DataRoots &roots,
                                                          const std::string &file) {
  if (!roots.has_document(file)) {
    return std::nullopt;
  }
  const json::JsonValue document = roots.document(file);
  const std::string hub = document.string_or("hub", "");
  if (hub.empty() || !roots.has_document(hub)) {
    return std::nullopt;
  }
  return parse_slai_area_select(document, parse_slai_rumble_hub(roots.document(hub)));
}

SlaiAreaMaps parse_slai_area_maps(const json::JsonValue &document) {
  SlaiAreaMaps maps;
  if (const json::JsonValue *areas = document.find("areas")) {
    for (const auto &[stage, value] : areas->object()) {
      RumblingArea area;
      area.id = stage;
      parse_rumbling_area_map(value, area);
      maps.emplace(stage, std::move(area));
    }
  }
  return maps;
}

} // namespace phantomcrash
