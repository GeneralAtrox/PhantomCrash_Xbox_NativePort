#include "phantomcrash/skeleton.hpp"

#include <algorithm>

namespace phantomcrash {

TrackSample MotionSource::sample(const int track) const {
  if (motion == nullptr) {
    return {};
  }
  const TrackSample current = motion->sample(track, frame);
  if (!blending()) {
    return current;
  }
  const auto as_sample = [&](const Motion &layer, const float at) {
    return layer.sample(track, at);
  };
  const auto mix = [](const TrackSample &a, const TrackSample &b, const float share) {
    return blend(a, b, share);
  };
  const auto from = blend_from<TrackSample>(as_sample, mix);
  return from ? blend(*from, current, std::max(weight, 0.0F)) : current;
}

} // namespace phantomcrash
