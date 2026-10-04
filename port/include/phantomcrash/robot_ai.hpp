#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/nav_graph.hpp"
#include "phantomcrash/robot_mover.hpp"
#include <array>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace phantomcrash {

// Research labels for the original callback families (0x2061e0).
enum class AiMode { idle, route, stationary, engage, stationary_alt, withdraw, route_special };

struct AiLevel {
  std::array<float, 7> durations{};
  std::array<int, 3> movement_chances{};
  float memory_seconds{}, control_scale{}, sight_yaw{}, sight_pitch{};
  int max_weapon_requests{};
  float fire_probability{}, fire_interval{}, fire_delay{};
  float camouflage_interval{}, camouflage_probability{}, camouflage_duration{};
};

struct OriginalAiData {
  std::vector<std::array<int, 25>> profiles;
  std::vector<std::array<int, 5>> decisions;
  std::array<int, 5> state_order{}, ranker_weapon_masks{};
  std::vector<int> search_weights, ranker_entrants;
  std::array<AiLevel, 100> levels;
  std::map<std::string, float, std::less<>> parameters;
  [[nodiscard]] float value(std::string_view name) const;
};

[[nodiscard]] std::shared_ptr<const OriginalAiData> parse_original_ai(const json::JsonValue &data);

struct RobotAiState {
  // Content identity survives a respawn; runtime state is replaced wholesale.
  int ability{}, profile{4}, entrant{-1};
  bool entered{};
  bool initialized{}, visible{}, camouflage_toggle{}, route_rejoin{true};
  bool ranker_defensive{}, ranker_orbit{};
  unsigned forced_weapons{};
  bool ranker_engage{}, cloak_detection{}, threat{};
  // A ranker turned on its attacker (0x05d2bd) fights as any robot does until
  // ranker_left (+0x132) runs out and 0x05de0f chooses again.
  bool ranker_provoked{};
  AiMode mode{AiMode::route};
  int pickup{-1};
  float pickup_left{};
  // 0x059799: 1 jump pressed, 2 in the air, 3 aiming after landing.
  int jump_phase{};
  float jump_aim_left{};
  std::vector<float> damage_seen; // the robot's damage_from at the last AI tick
  unsigned pickup_selections{};
  int target{-1}, submode{}, search{-1}, dodge_side{1};
  float accumulator{}, memory{}, state_left{}, guard{}, action_left{}, previous_hp{};
  float route_stalled{}, blocked{}, camouflage_left{}, camouflage_wait{}, detect_left{};
  float ranker_left{}, diagnostics_left{};
  Vec3 last_seen{}, last_position{}, route_target{};
  RobotControl control;
  NavAgent nav; // port navigation (nav_graph.hpp)
  std::array<bool, 4> request{}, fire{};
  std::array<float, 4> fire_wait{}, fire_delay{};
  unsigned ticks{}, transitions{}, detections{}, dodges{}, shots{}, attacks{}, recoveries{};

  void reset();
};

[[nodiscard]] bool ai_chance(std::minstd_rand &random, float threshold, float scale);
[[nodiscard]] float ai_random(std::minstd_rand &random);
void ai_enter(RobotAiState &state, AiMode mode, const OriginalAiData &data,
              std::minstd_rand &random);
void ai_decide(RobotAiState &state, const OriginalAiData &data, float hp, float max_hp, bool armed,
               std::minstd_rand &random);

} // namespace phantomcrash
