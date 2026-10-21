// Parsing of data/ui/town_map.json.

#include "phantomcrash/town_map.hpp"

#include "phantomcrash/json_fields.hpp"

#include <stdexcept>
#include <vector>

namespace phantomcrash {
namespace {

using json::numbers;
using json::required;

Sprite member_sprite(const json::JsonValue &value, const char *key) {
  try {
    return parse_sprite(required(value, key));
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string(key) + ": " + error.what());
  }
}

// The place's sign and rider stand at offsets from its spot (view units);
// they keep their pictures' size on the canvas.
TownPlace parse_place(const json::JsonValue &value, const TownView &view,
                      const TownMarkerStyle &markers) {
  TownPlace place;
  place.id = required(value, "id").string();
  place.next = value.string_or("next", "");
  place.help = parse_help(value);
  place.name = required(value, "name").string();
  place.bars = value.string_or("bars", "");
  if (place.bars.find_first_not_of("123456789") != std::string::npos) {
    throw std::runtime_error("bars: widths are the digits 1 to 9");
  }
  place.spot = numbers<2>(required(value, "spot"), "spot");
  place.side = value.number_or("side", 1.0) < 0.0 ? -1 : 1;
  place.label_width = static_cast<float>(required(value, "label_width").number());
  place.slai = value.boolean_or("slai", false);
  place.enabled = value.boolean_or("enabled", true);
  const auto at = [&](const char *key) {
    const auto offset = numbers<2>(required(value, key), key);
    return view.point({place.spot[0] + offset[0], place.spot[1] + offset[1]});
  };
  if (value.find("sign") != nullptr) {
    Sprite sign = member_sprite(value, "sign");
    sign.position = at("sign_at");
    place.sign = sign;
    place.sign_from = numbers<2>(required(value, "sign_from"), "sign_from");
  }
  place.rider = at("rider");
  place.rider_flipped = value.boolean_or("rider_flipped", false);
  place.boxes = town_marker_boxes(view, markers, place.spot, place.side, place.label_width);
  return place;
}

} // namespace

TownMapScript parse_town_map(const json::JsonValue &document) {
  try {
    TownMapScript script;
    parse_menu_basics(document, script);
    script.scene = static_cast<int>(required(document, "scene").number());
    script.help = parse_ticker(required(document, "help"));
    script.frame = parse_frame_placement(required(document, "frame"));
    const auto &rider = required(document, "rider");
    script.rider = member_sprite(document, "rider");
    script.bob = static_cast<float>(rider.number_or("bob", script.bob));
    script.bob_x_seconds = rider.number_or("bob_x_seconds", script.bob_x_seconds);
    script.bob_y_seconds = rider.number_or("bob_y_seconds", script.bob_y_seconds);
    script.pop_seconds = required(document, "pop_seconds").number();
    if (script.pop_seconds <= 0.0 || script.bob_x_seconds <= 0.0 || script.bob_y_seconds <= 0.0) {
      throw std::runtime_error("pop_seconds and the rider's bob times must be above 0");
    }
    const auto &buttons = required(document, "buttons");
    script.back_buttons = buttons_from_json(required(buttons, "back"));
    script.forward_buttons = buttons_from_json(required(buttons, "forward"));
    script.orient_to_start = buttons.boolean_or("orient_to_start", false);
    const auto &sounds = required(document, "sounds");
    script.place_sound = required(sounds, "place").string();
    script.spin_sound = sounds.string_or("spin", "");
    if (const auto *spin = document.find("spin")) {
      script.spin_pay = static_cast<int>(spin->number_or("pay", script.spin_pay));
      script.spin_limit = static_cast<int>(spin->number_or("limit", script.spin_limit));
    }
    script.closed_sound = sounds.string_or("closed", "");
    script.view = parse_town_view(required(document, "view"));
    script.markers = parse_town_markers(required(document, "markers"));
    if (const auto *bands = document.find("bands")) {
      script.bands = parse_town_bands(*bands);
    }
    if (const auto *lettering = document.find("lettering")) {
      script.lettering = parse_town_lettering(*lettering);
    }
    if (const auto *water = document.find("water")) {
      script.water = parse_town_water(*water);
    }
    if (const auto *clear = document.find("keep_clear")) {
      for (const auto &box : clear->array()) {
        script.keep_clear.push_back(numbers<4>(box, "keep_clear"));
      }
    }
    for (const auto &value : required(document, "places").array()) {
      try {
        script.places.push_back(parse_place(value, script.view, script.markers));
      } catch (const std::exception &error) {
        throw std::runtime_error("place \"" + value.string_or("id", "?") + "\": " + error.what());
      }
    }
    if (script.places.empty()) {
      throw std::runtime_error("places needs at least one place");
    }
    if (const auto *start = document.find("start_places")) {
      for (const auto &[screen, place] : start->object()) {
        script.start_places[screen] = place.string();
      }
    }
    return script;
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string("town_map.json: ") + error.what());
  }
}

TownMapScript for_content(TownMapScript script, const bool slai) {
  std::erase_if(script.places,
                [&](const TownPlace &place) { return !place.enabled || (place.slai && !slai); });
  if (script.places.empty()) {
    throw std::runtime_error("town_map.json: no place is shown");
  }
  return script;
}

} // namespace phantomcrash
