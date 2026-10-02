#pragma once
#include "desktop_replay_overlay.hpp"
#include "desktop_world_renderer.hpp"
#include "phantomcrash/battle_camera.hpp"
#include "phantomcrash/world_sound.hpp"
#include "phantomcrash/cockpit.hpp"
#include <cstdio>
#include <memory>

namespace phantomcrash {
struct ReplaySubject {
  CameraSubject camera;
  CockpitInput cockpit;
  std::string name;
  bool alive{};
  bool in_play{}; // inside the arena and not waiting in a scripted entrance
};
struct ReplayFrame {
  double seconds{};
  WorldFrame player;
  std::optional<WorldFrame> monitor;
  std::vector<WorldItem> hidden_player;
  std::vector<ReplaySubject> subjects;
  ReplayOverlay overlay;
  std::vector<SoundCue> sounds;
  std::string music;
};
// A process-private, automatically deleted spool. Only offsets/timestamps stay
// in RAM, so a long match does not retain thousands of skeletal poses in memory.
class ReplayArchive final {
public:
  ReplayArchive();
  ~ReplayArchive();
  ReplayArchive(const ReplayArchive &) = delete;
  ReplayArchive &operator=(const ReplayArchive &) = delete;
  void append(const ReplayFrame &frame);
  ReplayFrame read(std::size_t index);
  std::size_t at(double seconds) const;
  double duration() const {
    return times_.empty() ? 0 : times_.back();
  }
  std::size_t size() const {
    return times_.size();
  }
  void hear(const std::vector<SoundCue> &sounds);
  std::vector<SoundCue> take_sounds();
  CameraViewSettings views;
  ChaseCameraSettings chase;
  std::vector<Vec3> fixed_points;
  StageCollision collision;
  const DataRoots *roots{};
  std::string player_name;

private:
  std::FILE *file_{};
  std::vector<std::int64_t> offsets_;
  std::vector<double> times_;
  std::vector<SoundCue> sounds_;
  double origin_{};
};
} // namespace phantomcrash
