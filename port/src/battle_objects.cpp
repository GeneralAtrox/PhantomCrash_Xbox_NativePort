#include "phantomcrash/battle.hpp"

namespace phantomcrash {

int Battle::object_count() const {
  return objects_ ? static_cast<int>(objects_->objects().size()) : 0;
}

void Battle::add_object_targets(std::vector<Target> &out) const {
  if (!objects_) {
    return;
  }
  for (const auto &object : objects_->objects()) {
    // Keep a slot even after destruction: outstanding projectiles and hits
    // retain their target indices, including the supply targets that follow.
    out.push_back({object.centre, object.radius, -1, object.solid(), &object.collision, false});
  }
}

bool Battle::hit_object(const WeaponHit &hit) {
  const int index = hit.victim - static_cast<int>(robots_.size());
  if (index < 0 || index >= object_count()) {
    return false;
  }
  if (const auto broken = objects_->hit(static_cast<std::size_t>(index))) {
    BattleEvent event{hit.attacker, BattleEventKind::effect};
    event.slot = broken->effect;
    event.at = broken->at;
    events_.push_back(event);
  }
  return true;
}

} // namespace phantomcrash
