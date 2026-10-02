#pragma once

#include "desktop_world_gpu.hpp"
#include <array>
#include <span>

namespace phantomcrash {
struct WorldLight {
  Vec3 position;
  float range{200};
  Vec3 direction{0, -1, 0};
  float outer_degrees{30};
  Vec3 colour{1, 1, 1};
  float intensity{2};
  float inner_degrees{20};
};

// A bounded set of spotlights shared by the main camera and monitor feed.
class DynamicLightPass final {
public:
  static constexpr unsigned capacity = 8;
  explicit DynamicLightPass(ID3D11Device *device);
  unsigned prepare(ID3D11DeviceContext *context, std::span<const WorldLight> lights);
  void begin_shadow(ID3D11DeviceContext *context, unsigned index);
  void caster(ID3D11DeviceContext *context, const GpuModel &model, const ModelDraw &draw,
              const Mat4 &world);
  void bind(ID3D11DeviceContext *context);

private:
  static constexpr unsigned resolution = 1024;
  std::array<Mat4, capacity> matrices_;
  Mat4 current_;
  Com<ID3D11Texture2D> depth_;
  std::array<Com<ID3D11DepthStencilView>, capacity> targets_;
  Com<ID3D11ShaderResourceView> view_;
  Com<ID3D11Buffer> lights_, draw_;
  Com<ID3D11VertexShader> vertex_;
  Com<ID3D11PixelShader> pixel_;
  Com<ID3D11InputLayout> layout_;
  Com<ID3D11RasterizerState> raster_;
  Com<ID3D11DepthStencilState> depth_state_;
  Com<ID3D11SamplerState> comparison_;
};
} // namespace phantomcrash
