// A boss's defeat (robot states 0x13-0x17 of the state table at 0x242d38):
// 0x07675e puts a boss a player's robot shot down in state 0x13. Its
// entry (0x089861) slides the letterbox bars in (global +0xef320 2), takes
// the players' controls and HUD (0x091dc4) and points the camera at it
// (0x067319); an Area Ranker's becomes state 0x14 + area and speaks its
// defeat line, the First Ranker's state 0x17 with a longer timer. Its tick
// (0x089bb9) counts the timer down and waits for the line to end; its exit
// (0x089d1b) ends a ranker's battle and gives another boss's back.

#include "phantomcrash/battle_scene.hpp"

#include <algorithm>
#include <utility>

namespace phantomcrash {

void BattleScene::start_defeat(const int robot, const float frames, const bool ends) {
  if (defeat_ >= 0) {
    return;
  }
  defeat_ = robot;
  defeat_left_ = frames;
  defeat_ends_ = ends;
  defeat_over_ = false;
  // A ranker's ends the battle; another boss's gives it back, and meanwhile
  // the players, their controls taken, are spared as for a boss's entrance.
  defeat_spares_ = !ends && tuning_.boss_spares_players;
  letterbox_way_ = 1.0F;
}

void BattleScene::tick_defeat(Battle &battle, const float step) {
  if (defeat_ < 0 || defeat_over_) {
    return;
  }
  if (defeat_spares_) {
    battle.set_players_spared(true);
  }
  defeat_left_ = std::max(defeat_left_ - step, 0.0F);
  if (defeat_left_ > 0.0F || defeat_speaking_) {
    return;
  }
  if (defeat_ends_) {
    defeat_over_ = true; // the bars stay in until the battle ends (global +0xee528)
    return;
  }
  // 0x089d1b, state 0x13: the controls and the camera come back (0x091ea7,
  // 0x067350) and the bars slide out (+0xef320 3).
  defeat_ = -1;
  letterbox_way_ = -1.0F;
  if (std::exchange(defeat_spares_, false) && !cinematic()) {
    battle.set_players_spared(false); // a boss's entrance keeps its own
  }
}

} // namespace phantomcrash
