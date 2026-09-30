#include "desktop_screens.hpp"
#include "desktop_game_screens.hpp"
#include "desktop_match_result.hpp"
#include "desktop_training.hpp"
#include "phantomcrash/json_fields.hpp"

#include <algorithm>

namespace phantomcrash {
using json::required;

std::unique_ptr<Screen> ScreenRunner::make_match_result() {
  auto *battle = dynamic_cast<TrainingScreen *>(screen_.get());
  if (!battle || !active_match_) {
    return nullptr;
  }
  const auto data = roots_.document("ui/match_result.json");
  const auto rules = parse_match_result_rules(required(data, "rules"));
  MatchSummary summary = battle->match_summary();
  summary.rental = active_match_->rental;
  WorldFrame background = battle->result_background();
  const auto starting_money = quest_.money;
  const auto prize =
      battle->extra_prize() + (rumbling_world_ ? rumbling_world_->give_back(quest_) : 0);
  const MatchReceipt receipt = match_receipt(summary, starting_money, prize, game_->part_tables(),
                                             game_->garage_tables(), rules);
  // The result view owns a copy; neither its animation nor leaving it settles again.
  settle_match(receipt, quest_);
  if (summary.outcome == MatchOutcome::win && active_match_->contest >= 0 &&
      std::ranges::find(quest_.won_contests, active_match_->contest) == quest_.won_contests.end()) {
    quest_.won_contests.push_back(active_match_->contest);
  }
  active_match_.reset();
  SDL_Log("Match result: %s, shootdowns=%d, prize=%lld, repair=%lld, money=%lld",
          summary.outcome == MatchOutcome::retire ? "retire"
          : summary.outcome == MatchOutcome::win  ? "win"
                                                  : "leave",
          summary.shootdowns, static_cast<long long>(receipt.prize),
          static_cast<long long>(receipt.repair_total),
          static_cast<long long>(receipt.total_money));
  return std::make_unique<MatchResultScreen>(textures_, font_, frame_, credits_, data, receipt,
                                             quest_, world_.get(), std::move(background),
                                             data.string_or("exit_to", "sleep"), battle->replay());
}

} // namespace phantomcrash
