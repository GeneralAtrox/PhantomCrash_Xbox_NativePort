// S.L.A.I.'s story counts: nights, enemies shot down, sponsors and the
// completion score (slai_story_rules.hpp).

#include "phantomcrash/slai_story_rules.hpp"

#include <algorithm>
#include <bit>
#include <string_view>

namespace phantomcrash {
namespace {

using json::JsonValue;

constexpr int max_days = 9999; // 0x4E8CF0's cap

int integer(const JsonValue &value, std::string_view key, int fallback = 0) {
  return static_cast<int>(value.number_or(key, fallback));
}

std::vector<int> integers(const JsonValue *items) {
  std::vector<int> out;
  if (items != nullptr && items->is_array()) {
    for (const auto &item : items->array()) {
      out.push_back(static_cast<int>(item.number()));
    }
  }
  return out;
}

// {"6": 1, ...}: keys are numbers written as strings.
std::map<int, int> number_map(const JsonValue *items) {
  std::map<int, int> out;
  if (items != nullptr && items->is_object()) {
    for (const auto &[key, value] : items->object()) {
      out[std::stoi(key)] = static_cast<int>(value.number());
    }
  }
  return out;
}

SlaiSponsorRules sponsor_rules(const JsonValue &value) {
  SlaiSponsorRules rules;
  rules.offer_flags = integers(value.find("offer_flags"));
  rules.offer_score_above = integer(value, "offer_score_above");
  rules.regulation_makers = number_map(value.find("regulation_makers"));
  rules.brand_regulations = integers(value.find("brand_regulations"));
  rules.brand_makers = number_map(value.find("brand_makers"));
  rules.keep_score_below = integer(value, "keep_score_below");
  rules.streak_limit = integer(value, "streak_limit");
  if (const auto *brands = value.find("sponsor_brands"); brands != nullptr && brands->is_object()) {
    for (const auto &[maker, list] : brands->object()) {
      rules.sponsor_brands[std::stoi(maker)] = integers(&list);
    }
  }
  rules.head_to_head_matches = integers(value.find("head_to_head_matches"));
  return rules;
}

bool contains(const std::vector<int> &list, int value) {
  return std::ranges::find(list, value) != list.end();
}

int records_cleared(const SlaiRumbleSave &rumble, int first, int count) {
  return static_cast<int>(std::ranges::count_if(rumble.records, [&](const auto &entry) {
    return entry.first >= first && entry.first < first + count && entry.second.cleared;
  }));
}

int part_count(const SlaiCompletionPart &part, const SlaiStorySave &save,
               const SlaiRumbleSave &rumble, const SlaiCollections &collections) {
  if (part.source == "records") {
    return records_cleared(rumble, part.first, part.count);
  }
  if (part.source == "collections") {
    return collections.collections;
  }
  if (part.source == "decals") {
    return collections.decals;
  }
  if (part.source == "music") {
    return collections.music;
  }
  if (part.source == "sponsors") {
    return std::popcount(static_cast<unsigned>(save.sponsors_had));
  }
  return 0;
}

// 0x24A9E0: the sponsor leaves on a low score, a run of battles shot down
// or a body not of its brands; head-to-head matches only check the body.
void lose_sponsor(SlaiStorySave &save, const SlaiSponsorRules &rules,
                  const SlaiBattleOutcome &outcome) {
  const auto leave = [&save](int reason) {
    save.sponsor = slai_no_sponsor;
    save.dismissal = reason;
  };
  constexpr int reason_body = 1, reason_shot_down = 2, reason_score = 3;
  if (!contains(rules.head_to_head_matches, outcome.match)) {
    if (outcome.score < rules.keep_score_below) {
      leave(reason_score);
    }
    if (!outcome.survived) {
      if (++save.shot_down_streak >= rules.streak_limit) {
        save.shot_down_streak = rules.streak_limit;
        leave(reason_shot_down);
      }
    } else {
      save.shot_down_streak = 0;
    }
  }
  const auto brands = rules.sponsor_brands.find(save.previous_sponsor);
  if (brands != rules.sponsor_brands.end() && !contains(brands->second, outcome.brand)) {
    leave(reason_body);
  }
}

// 0x25A4E0: a good score in a maker's rumble, or a body-type rumble with a
// branded body, brings that maker's offer.
void offer_sponsor(SlaiStorySave &save, const SlaiSponsorRules &rules,
                   const SlaiBattleOutcome &outcome) {
  if (outcome.score <= rules.offer_score_above) {
    return;
  }
  int maker = -1;
  if (const auto found = rules.regulation_makers.find(outcome.regulation);
      found != rules.regulation_makers.end()) {
    maker = found->second;
  } else if (contains(rules.brand_regulations, outcome.regulation)) {
    if (const auto branded = rules.brand_makers.find(outcome.brand);
        branded != rules.brand_makers.end()) {
      maker = branded->second;
    }
  }
  if (maker < 0 || maker >= slai_no_sponsor) {
    return;
  }
  save.sponsors_had |= 1 << maker;
  save.sponsor = maker;
  save.shot_down_streak = 0;
}

} // namespace

SlaiStoryRules slai_story_rules_from_json(const JsonValue &table) {
  SlaiStoryRules rules;
  if (const auto *milestones = table.find("defeat_milestones");
      milestones != nullptr && milestones->is_object()) {
    for (const auto &[name, value] : milestones->object()) {
      rules.milestones[name] = {static_cast<std::int64_t>(value.number_or("above", 0)),
                                integer(value, "superseded_by", -1), integer(value, "marks", -1)};
    }
  }
  if (const auto *sponsors = table.find("sponsor_rules"); sponsors != nullptr) {
    rules.sponsors = sponsor_rules(*sponsors);
  }
  if (const auto *score = table.find("completion_score"); score != nullptr) {
    rules.completion_divisor = std::max(1, integer(*score, "divisor", 1));
    if (const auto *parts = score->find("parts"); parts != nullptr && parts->is_array()) {
      for (const auto &part : parts->array()) {
        rules.completion.push_back({part.string_or("source", ""), integer(part, "first"),
                                    integer(part, "count"), integer(part, "points")});
      }
    }
  }
  if (const auto *pairs = table.find("overnight_flags"); pairs != nullptr && pairs->is_array()) {
    for (const auto &pair : pairs->array()) {
      const auto both = integers(&pair);
      if (both.size() == 2) {
        rules.overnight_flags.push_back({both[0], both[1]});
      }
    }
  }
  return rules;
}

bool slai_story_flag(const SlaiStorySave &save, int flag) {
  return flag >= 0 && static_cast<std::size_t>(flag) < save.flags.size() &&
         save.flags[static_cast<std::size_t>(flag)];
}

void slai_set_story_flag(SlaiStorySave &save, int flag) {
  if (flag >= 0 && static_cast<std::size_t>(flag) < save.flags.size()) {
    save.flags[static_cast<std::size_t>(flag)] = true;
  }
}

void slai_story_nights(SlaiStorySave &save, const SlaiStoryRules &rules, int total_days) {
  if (save.nights_seen < 0 || total_days < save.nights_seen) {
    save.nights_seen = total_days;
    return;
  }
  for (; save.nights_seen < total_days; ++save.nights_seen) {
    save.days = std::min(save.days + 1, max_days);
    for (const auto &[when, then] : rules.overnight_flags) {
      if (slai_story_flag(save, when)) {
        slai_set_story_flag(save, then);
      }
    }
  }
}

void slai_story_after_battle(SlaiStorySave &save, const SlaiStoryRules &rules,
                             const SlaiBattleOutcome &outcome) {
  save.defeated += std::max(0, outcome.shootdowns);
  save.survived = outcome.survived;
  save.previous_sponsor = save.sponsor;
  if (outcome.excluded) {
    return;
  }
  lose_sponsor(save, rules.sponsors, outcome);
  offer_sponsor(save, rules.sponsors, outcome);
}

int slai_completion_score(const SlaiStoryRules &rules, const SlaiStorySave &save,
                          const SlaiRumbleSave &rumble, const SlaiCollections &collections) {
  long long total = 0;
  for (const auto &part : rules.completion) {
    if (part.count <= 0) {
      continue;
    }
    const int count = std::clamp(part_count(part, save, rumble, collections), 0, part.count);
    total += static_cast<long long>(count) * part.points / part.count;
  }
  const long long divisor = rules.completion_divisor;
  return static_cast<int>(total / divisor + (total % divisor * 2 >= divisor ? 1 : 0));
}

} // namespace phantomcrash
