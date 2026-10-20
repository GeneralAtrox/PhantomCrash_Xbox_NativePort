#include "test_support.hpp"
#include "phantomcrash/source_muzzle_flash.hpp"
#include <cmath>

using namespace phantomcrash;

namespace {
SourceMuzzleFlashes flashes() {
  return parse_source_muzzle_flashes(json::parse(std::string_view(R"({
    "model": "slai/assets/models/mazl_01.gltf",
    "layers": [{"alpha": 0.4}, {"alpha": 0.8}], "colour": [1, 1, 1],
    "visible_updates": 1, "update_hz": 30, "roll": [-3.14159265, 3.14159265],
    "weapons": [
      {"mount": "arm", "model": 40, "flash_nodes": [2, 3], "barrel_flash": true, "flash_scale": 1.17},
      {"mount": "arm", "model": 41, "flash_nodes": [2, 3, 4], "barrel_flash": false, "flash_scale": 0.9},
      {"mount": "shoulder", "model": 40, "flash_nodes": [], "flash_scale": 1.1}]})")));
}
bool near(float a, float b) {
  return std::abs(a - b) < 1e-5F;
}
} // namespace

TEST_CASE("source muzzle flash data keys rules by weapon model and mount") {
  const auto data = flashes();
  CHECK(data.alphas.size() == 2 && near(data.alphas[0], 0.4F) && near(data.alphas[1], 0.8F));
  CHECK(near(data.seconds, 1.0F / 30.0F));
  CHECK(data.find(40, false) && data.find(40, false)->barrel_flash);
  CHECK(data.find(40, true) && data.find(40, true)->nodes.empty());
  CHECK(data.find(42, false) == nullptr);
}

TEST_CASE("source muzzle flash follows the barrel only for barrel-flash weapons") {
  const auto data = flashes();
  const auto &paired = *data.find(40, false);
  CHECK(source_flash_node(paired, 0) == 2);
  CHECK(source_flash_node(paired, 1) == 3);
  CHECK(source_flash_node(paired, 2) == 3); // 0x511630 tests barrel != 0
  const auto &fixed = *data.find(41, false);
  CHECK(source_flash_node(fixed, 1) == 2);
  CHECK(source_flash_node(*data.find(40, true), 0) == -1);
}

TEST_CASE("source muzzle flash rolls about z and scales uniformly") {
  const auto data = flashes();
  const auto &rule = *data.find(40, false);
  const Mat4 m = source_flash_transform(rule, pi / 2);
  const Vec3 x = transform_point({1, 0, 0}, m);
  const Vec3 z = transform_point({0, 0, 1}, m);
  CHECK(near(length(x), 1.17F) && near(x.z, 0.0F) && near(std::abs(x.y), 1.17F));
  CHECK(near(z.z, 1.17F));
}
