#include "phantomcrash/enemy_path.hpp"

#include <stdexcept>
#include <string>

namespace phantomcrash {

std::vector<int> EnemyPath::entries() const {
  std::vector<int> out;
  for (std::size_t index = 0; index < segments.size(); ++index) {
    if (segments[index].kind == 0) {
      out.push_back(static_cast<int>(index));
    }
  }
  if (out.empty()) {
    for (std::size_t index = 0; index < segments.size(); ++index) {
      out.push_back(static_cast<int>(index));
    }
  }
  return out;
}

EnemyPath parse_enemy_path(const json::JsonValue &document) {
  EnemyPath path;
  const json::JsonValue *segments = document.find("segments");
  if (segments == nullptr || !segments->is_array()) {
    throw std::runtime_error("Enemy path has no segments");
  }
  for (const json::JsonValue &value : segments->array()) {
    EnemyPath::Segment segment;
    segment.kind = static_cast<int>(value.number_or("kind", 0));
    if (const json::JsonValue *points = value.find("points"); points && points->is_array()) {
      for (const json::JsonValue &point : points->array()) {
        const auto &xyz = point.array();
        if (xyz.size() != 3) {
          throw std::runtime_error("Enemy path point needs 3 numbers");
        }
        segment.points.push_back({static_cast<float>(xyz[0].number()),
                                  static_cast<float>(xyz[1].number()),
                                  static_cast<float>(xyz[2].number())});
      }
    }
    if (segment.points.empty()) {
      throw std::runtime_error("Enemy path segment " + std::to_string(path.segments.size()) +
                               " has no points");
    }
    if (const json::JsonValue *next = value.find("next"); next && next->is_array()) {
      for (const json::JsonValue &link : next->array()) {
        segment.next.push_back(static_cast<int>(link.number()));
      }
    }
    path.segments.push_back(std::move(segment));
  }
  for (const auto &segment : path.segments) {
    for (const int link : segment.next) {
      if (link < 0 || link >= static_cast<int>(path.segments.size())) {
        throw std::runtime_error("Enemy path links to missing segment " + std::to_string(link));
      }
    }
  }
  return path;
}

void PathFollower::start(const EnemyPath &path, const int segment) {
  segment_ = segment >= 0 && segment < static_cast<int>(path.segments.size()) ? segment : -1;
  point_ = 0;
}

Vec3 PathFollower::update(const EnemyPath &path, const Vec3 position, const float reach,
                          std::minstd_rand &random) {
  if (segment_ < 0) {
    return position;
  }
  const auto *segment = &path.segments[static_cast<std::size_t>(segment_)];
  Vec3 target = segment->points[point_];
  Vec3 offset = target - position;
  offset.y = 0.0F;
  if (length(offset) > reach) {
    return target;
  }
  if (point_ + 1 < segment->points.size()) {
    ++point_;
  } else if (!segment->next.empty()) {
    std::uniform_int_distribution<std::size_t> pick(0, segment->next.size() - 1);
    segment_ = segment->next[pick(random)];
    point_ = 0;
    segment = &path.segments[static_cast<std::size_t>(segment_)];
  } else {
    point_ = 0; // a dead end: walk it again from its start
  }
  return segment->points[point_];
}

} // namespace phantomcrash
