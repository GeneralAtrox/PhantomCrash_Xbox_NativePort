// S.L.A.I.'s area select: the cities' areas and what each one holds today
// (phantomcrash/slai_area_select.hpp).

#include "phantomcrash/slai_area_select.hpp"

#include <algorithm>

namespace phantomcrash {
namespace {

// The rect's art fitted into `box`, centred and keeping its proportions.
Sprite fitted(const std::string &image, const TexelRect &rect, const std::array<float, 4> &box,
              const int layer) {
  Sprite sprite;
  sprite.image = image;
  sprite.rect = rect;
  sprite.layer = layer;
  const float width = rect[2] - rect[0];
  const float height = rect[3] - rect[1];
  if (width <= 0.0F || height <= 0.0F) {
    return sprite;
  }
  const float scale = std::min((box[2] - box[0]) / width, (box[3] - box[1]) / height);
  sprite.size = std::array<float, 2>{width * scale, height * scale};
  sprite.position = {box[0] + (box[2] - box[0] - width * scale) * 0.5F,
                     box[1] + (box[3] - box[1] - height * scale) * 0.5F};
  return sprite;
}

std::string with_city(std::string text, const std::string &city) {
  const std::string key = "{city}";
  for (auto at = text.find(key); at != std::string::npos; at = text.find(key, at)) {
    text.replace(at, key.size(), city);
    at += city.size();
  }
  return text;
}

} // namespace

// The cities by the table's display order, each with the plate, picture and
// map of the rumble it holds today, then EXIT.
void SlaiAreaSelect::build_areas(const SlaiAreaMaps &maps) {
  RumblingScript &rumbling = script_.rumbling;
  const RumblingArea plate = rumbling.areas.front();
  RumblingArea exit = rumbling.areas.back();
  const SlaiRumbleHubScript &hub = script_.hub;
  const SlaiRumbleSave &save = quest_.slai_rumble;
  rumbling.areas.clear();
  const auto &order = data_.table.display_order;
  for (std::size_t slot = 0; slot < order.size() && slot < script_.plates.size(); ++slot) {
    const int column = order[slot];
    RumblingArea area = plate;
    const auto &names = hub.city_names;
    const std::string name = column < static_cast<int>(names.size())
                                 ? names[static_cast<std::size_t>(column)]
                                 : std::string{};
    area.id = "slai_city_" + std::to_string(column);
    area.item.id = area.item_lit.id = script_.plate_id + std::to_string(column);
    area.item.position = area.item_lit.position = script_.plates[slot];
    area.item_from_x = script_.plates[slot][0] + script_.plate_slide;
    if (column < static_cast<int>(hub.city_art.size())) {
      const SlaiCityArt &art = hub.city_art[static_cast<std::size_t>(column)];
      area.picture = fitted(art.image, art.blueprint, script_.picture_box, plate.picture.layer);
      area.night_picture = area.picture;
    }
    area.map_frames.clear();
    area.gates.clear();
    area.marks.clear();
    std::optional<SlaiCityMatch> held = slai_city_match(data_, quest_.day, column, save);
    if (held) {
      const SlaiMatch *match = slai_match(data_.table, held->match);
      // Rumble S.L.A.I.'s records and exclude_records (the tutorial's).
      const auto listed_in = [&](const std::vector<int> &records) {
        return match != nullptr && std::ranges::find(records, match->save_record) != records.end();
      };
      if (match == nullptr || listed_in(hub.exclude_records) ||
          (!hub.records.empty() && !listed_in(hub.records))) {
        held.reset();
      }
    }
    area.help = with_city(held ? script_.help : script_.closed_help, name);
    if (held) {
      const auto map = maps.find(slai_match_stage(data_.world, column, held->night));
      if (map != maps.end()) {
        area.map_frames = map->second.map_frames;
        area.gates = map->second.gates;
        area.marks = map->second.marks;
        area.robot_map = map->second.robot_map;
      }
    }
    rumbling.areas.push_back(std::move(area));
    held_.push_back(held);
    cities_.push_back(column);
  }
  exit.item.position = exit.item_lit.position = script_.exit_plate;
  exit.item_from_x = script_.exit_plate[0] + script_.plate_slide;
  rumbling.areas.push_back(std::move(exit));
  held_.emplace_back();
  cities_.push_back(-1);
}

const std::optional<SlaiCityMatch> &SlaiAreaSelect::held(const int area) const {
  static const std::optional<SlaiCityMatch> none;
  return area >= 0 && area < static_cast<int>(held_.size()) ? held_[static_cast<std::size_t>(area)]
                                                            : none;
}

int SlaiAreaSelect::city(const int area) const {
  return area >= 0 && area < static_cast<int>(cities_.size())
             ? cities_[static_cast<std::size_t>(area)]
             : -1;
}

bool SlaiAreaSelect::cleared(const int area) const {
  const auto &shown = held(area);
  const SlaiMatch *match = shown ? slai_match(data_.table, shown->match) : nullptr;
  return match != nullptr && slai_record_cleared(quest_.slai_rumble, match->save_record);
}

const SlaiRegulation *SlaiAreaSelect::rule() const {
  const auto &shown = held(area_);
  const SlaiMatch *match = shown ? slai_match(data_.table, shown->match) : nullptr;
  return match != nullptr ? slai_regulation(data_.table, match->regulation) : nullptr;
}

bool SlaiAreaSelect::rental_only() const {
  const SlaiRegulation *shown = rule();
  return shown != nullptr && shown->special == SlaiSpecialRule::rental_only;
}

SlaiEntry SlaiAreaSelect::entry_check() const {
  const SlaiRegulation *shown = rule();
  return shown != nullptr ? slai_entry_check(data_.world, *shown, quest_.money, riding_)
                          : SlaiEntry::not_ready;
}

// ENTRY is greyed for a rental-only rumble and where the ridden robot fails
// the rumble's rules; SV RENTAL everywhere else, and without a robot to lend.
bool SlaiAreaSelect::can_choose(const RumblingAction action) const {
  switch (action) {
  case RumblingAction::entry:
    return rule() != nullptr && !rental_only() && entry_check() == SlaiEntry::allowed;
  case RumblingAction::rental:
    return rental_only() && rentals_;
  default:
    return true;
  }
}

} // namespace phantomcrash
