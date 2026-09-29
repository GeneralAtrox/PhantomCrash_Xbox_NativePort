// RUMBLING's live opponents (rumbling_world.hpp): each one out drawn on the
// area map by its maker and legs type (0x01492b), the one CHARACTER INFO
// shows lit with a ring turning round it, and its armour on the card.

#include "desktop_draw_list.hpp"
#include "desktop_rumbling.hpp"
#include "phantomcrash/rumbling_robot.hpp"

#include <cmath>
#include <string>

namespace phantomcrash {

const RosterMember *RumblingScreen::shown_member() const {
  const int area = logic_.area();
  const auto card = static_cast<std::size_t>(logic_.card());
  if (logic_.view() != RumblingView::character_info || area < 0 || area >= roster_areas ||
      card >= logic_.cards().size()) {
    return nullptr;
  }
  const int entrant = logic_.cards()[card].entrant;
  for (const RosterMember &member :
       logic_.quest().rosters[static_cast<std::size_t>(area)].members) {
    if (member.entrant == entrant) {
      return &member;
    }
  }
  return nullptr;
}

void RumblingScreen::draw_robots(DrawList &list) const {
  const RumblingScript &script = logic_.script();
  const RumblingRobotMarkers &markers = script.robot_markers;
  const int area = logic_.area();
  if (!markers.present || area < 0 || area >= roster_areas) {
    return;
  }
  const auto &members = logic_.quest().rosters[static_cast<std::size_t>(area)].members;
  if (members.size() < markers.fewest) {
    return;
  }
  const RumblingMapProjection &map = script.areas[static_cast<std::size_t>(area)].robot_map;
  const RegulationCheck &check = logic_.check();
  const RosterMember *shown = shown_member();
  for (const RosterMember &member : members) {
    const LiveRobot &live = member.live;
    const Entrant *who = contests_.entrant(member.entrant);
    const RobotRecord *record = who != nullptr ? check.garage.robot(who->robot) : nullptr;
    if (!live.placed || live.down || record == nullptr) {
      continue;
    }
    const RobotKind kind = robot_kind(check.parts, check.rules, *record);
    const auto icon = markers.icons.find({kind.maker, kind.legs_type});
    if (icon == markers.icons.end()) {
      continue;
    }
    const std::array<float, 2> at =
        rumbling_map_point(markers, map, live.position[0], live.position[2]);
    Sprite sprite = icon->second;
    sprite.position = at;
    sprite.centered = true;
    sprite.angle = rumbling_map_degrees(map, live.heading);
    sprite.colour = &member == shown ? markers.chosen_colour : markers.colour;
    list.sprite(sprite);
    if (&member == shown) {
      Sprite ring = markers.ring;
      ring.position = at;
      ring.centered = true;
      ring.angle =
          std::fmod(static_cast<float>(logic_.seconds()) * markers.ring_degrees_per_second, 360.0F);
      list.sprite(ring);
    }
  }
}

void RumblingScreen::draw_armour(DrawList &list, const int entrant) const {
  const RumblingCharacterInfo &info = logic_.script().character_info;
  const RosterMember *member = shown_member();
  if (!info.show_armour || member == nullptr || member->entrant != entrant ||
      !member->live.placed) {
    return;
  }
  const LiveRobot &live = member->live;
  const std::string line =
      live.down ? info.down_label
                : info.armour_label + std::to_string(static_cast<int>(std::ceil(live.armour))) +
                      "/" + std::to_string(static_cast<int>(std::ceil(live.max_armour)));
  list.text(info.text_layer,
            layout_run(font_, glyph_run(font_, line), info.armour[0], info.armour[1]),
            info.text_colour);
}

} // namespace phantomcrash
