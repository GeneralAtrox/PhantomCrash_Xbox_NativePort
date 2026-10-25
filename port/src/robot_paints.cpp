#include "phantomcrash/robot.hpp"

namespace phantomcrash {

const std::vector<int> &native_paints(const RobotCatalogue &catalogue, const int body_type,
                                      const int leg_type) {
  const auto &types = catalogue.leg_types;
  const auto native = [&](const int type) {
    return type >= 0 && type < static_cast<int>(types.size()) &&
           types[static_cast<std::size_t>(type)].paint_images.empty();
  };
  const auto &legs = types.at(static_cast<std::size_t>(leg_type));
  if (native(leg_type)) {
    return legs.paints;
  }
  const auto &homes = catalogue.body_leg_types;
  const int home = body_type >= 0 && body_type < static_cast<int>(homes.size())
                       ? homes[static_cast<std::size_t>(body_type)]
                       : -1;
  for (const int type : {home, 0}) {
    if (native(type)) {
      return types[static_cast<std::size_t>(type)].paints;
    }
  }
  return legs.paints;
}

} // namespace phantomcrash
