#include "desktop_online_match.hpp"
#include "online/state_writer.hpp"

namespace phantomcrash::online {
StateDigest state_digest(const CoopMatch &match) {
  StateWriter out;
  // Transport, prediction history and GPU/resource owners are outside simulation.
  out(std::uint32_t{1}, match.day, match.area, match.contest, match.world, match.battle,
      match.simulation_save, match.entrances, match.rider_scenes, match.simulation_looks,
      match.players, match.settlements, match.look_revision, match.elapsed, match.options);
  return out.finish();
}
} // namespace phantomcrash::online
