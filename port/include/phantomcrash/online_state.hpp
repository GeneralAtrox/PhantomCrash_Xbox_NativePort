#pragma once
#include <array>
#include <cstdint>

namespace phantomcrash {
class Battle;
class RumblingWorld;
class BattleScene;
namespace online {
using StateDigest = std::array<std::uint8_t, 32>;
// Inspection is read-only and requires navigation preparation before hashing.
// Digests include exact numeric bits and pointed-to contents, never addresses.
[[nodiscard]] StateDigest state_digest(const Battle &battle);
[[nodiscard]] StateDigest state_digest(const RumblingWorld &world);
[[nodiscard]] StateDigest state_digest(const BattleScene &scene);
} // namespace online
} // namespace phantomcrash
