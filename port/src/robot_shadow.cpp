#include "phantomcrash/robot_shadow.hpp"

#include "phantomcrash/json_fields.hpp"

namespace phantomcrash {

RobotShadow robot_shadow(const RobotShadowLook &look, const bool piloted, const bool split_screen,
                         const bool grounded) {
  if (piloted && look.volumes && (!split_screen || look.split_screen_volumes)) {
    return RobotShadow::volume;
  }
  if (!grounded || look.ground_model.empty()) {
    return RobotShadow::none;
  }
  return RobotShadow::ground;
}

RobotShadowLook parse_robot_shadow_look(const json::JsonValue &value) {
  RobotShadowLook look;
  look.volumes = value.boolean_or("volumes", look.volumes);
  look.split_screen_volumes = value.boolean_or("split_screen_volumes", look.split_screen_volumes);
  look.volume_length = static_cast<float>(value.number_or("volume_length", look.volume_length));
  look.ground_model = value.string_or("ground_model", look.ground_model);
  if (const json::JsonValue *colour = value.find("ground_colour")) {
    look.ground_colour = json::numbers<4>(*colour, "robot_shadows ground_colour");
  }
  return look;
}

} // namespace phantomcrash
