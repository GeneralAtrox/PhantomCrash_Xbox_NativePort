// The screen pass (0x0b7900): once a frame, ahead of the cockpit's view
// layer, the original draws the scene back over itself through one of its
// post passes. Outside battles that is 0x0b6f00, the frame multiplied by the
// texture factor 0x25b490 (white); in battles it is the depth of field
// 0x0b5fb0: the frame softened twice by blur.xpu (the average of four taps)
// into two half-size targets, then dof.xpu mixing the soft copy in by the
// depth's top byte and multiplying by the same colour, the stage preset's
// tint. (0x0b6660, a shimmer over the blur, is never switched on.)
#include "desktop_world_renderer.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace phantomcrash {
namespace {

constexpr char source[] = R"hlsl(
cbuffer Screen : register(b0) {
  float2 tap;      // the blur's taps, in uv, out from the pixel on both axes
  float threshold; // the depth byte where the blur starts
  float ramp;
  float3 tint;
  float blur_on;
  float depth_a;   // the port's projection: distance = depth_b / (depth - depth_a)
  float depth_b;
  float original_scale; // the original's: depth = scale * (1 - near / distance)
  float original_near;
};

Texture2D picture : register(t0);
Texture2D blurred : register(t1);
#ifdef MULTISAMPLED
Texture2DMS<float> depth : register(t2);
#else
Texture2D<float> depth : register(t2);
#endif
SamplerState smooth : register(s0);

struct Screen {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

Screen vs_screen(uint id : SV_VertexID) {
  Screen o;
  float2 at = float2((id << 1) & 2, id & 2);
  o.position = float4(at * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
  o.uv = at;
  return o;
}

// The coverage pass's triangle lies at the far plane: it passes the depth
// test only where something was drawn.
Screen vs_far(uint id : SV_VertexID) {
  Screen o = vs_screen(id);
  o.position.z = 1.0;
  return o;
}

float4 ps_cover(Screen i) : SV_Target {
  return float4(0.0, 0.0, 0.0, 1.0);
}

float4 ps_blur(Screen i) : SV_Target {
  float4 sum = picture.Sample(smooth, i.uv + float2(-tap.x, -tap.y));
  sum += picture.Sample(smooth, i.uv + float2(tap.x, -tap.y));
  sum += picture.Sample(smooth, i.uv + float2(-tap.x, tap.y));
  sum += picture.Sample(smooth, i.uv + float2(tap.x, tap.y));
  return sum * 0.25;
}

float4 ps_screen(Screen i) : SV_Target {
  int2 pixel = int2(i.position.xy);
  float3 colour = picture.Load(int3(pixel, 0)).rgb;
  if (blur_on > 0.5) {
#ifdef MULTISAMPLED
    float d = depth.Load(pixel, 0);
#else
    float d = depth.Load(int3(pixel, 0));
#endif
    float distance = depth_b / min(d - depth_a, -1e-7);
    float original = saturate(original_scale * (1.0 - original_near / distance));
    // The top byte of the original's 24-bit depth, read as a colour's alpha.
    float top = min(floor(original * 256.0), 255.0) / 255.0;
    float amount = saturate((top - threshold) * ramp);
    colour = lerp(colour, blurred.Sample(smooth, i.uv).rgb, amount);
  }
  return float4(colour * tint, 1.0);
}
)hlsl";

// Byte-for-byte cbuffer Screen.
struct ScreenConstants {
  std::array<float, 2> tap;
  float threshold;
  float ramp;
  Vec3 tint;
  float blur_on;
  float depth_a;
  float depth_b;
  float original_scale;
  float original_near;
};
static_assert(sizeof(ScreenConstants) == 48);

// The frame the original's blur taps are measured in (0x25b6a8).
constexpr float original_height = 480.0F;

void create(ID3D11Device *device, ID3DBlob *code, Com<ID3D11VertexShader> &out) {
  if (FAILED(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
                                        out.ReleaseAndGetAddressOf()))) {
    throw std::runtime_error("Unable to create the screen pass's vertex shader");
  }
}

void create(ID3D11Device *device, ID3DBlob *code, Com<ID3D11PixelShader> &out) {
  if (FAILED(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
                                       out.ReleaseAndGetAddressOf()))) {
    throw std::runtime_error("Unable to create the screen pass's pixel shaders");
  }
}

} // namespace

void WorldRenderer::create_screen_states() {
  const std::string single(source, sizeof(source) - 1);
  const std::string multisampled = "#define MULTISAMPLED\n" + single;
  create(device_.Get(), compile_world_hlsl(single, "vs_screen", "vs_5_0").Get(), screen_vertex_);
  create(device_.Get(), compile_world_hlsl(single, "ps_blur", "ps_5_0").Get(), blur_pixel_);
  create(device_.Get(), compile_world_hlsl(single, "ps_screen", "ps_5_0").Get(), screen_pixel_);
  create(device_.Get(), compile_world_hlsl(multisampled, "ps_screen", "ps_5_0").Get(),
         screen_pixel_ms_);
  create(device_.Get(), compile_world_hlsl(single, "vs_far", "vs_5_0").Get(), far_vertex_);
  create(device_.Get(), compile_world_hlsl(single, "ps_cover", "ps_5_0").Get(), cover_pixel_);
  D3D11_BLEND_DESC blend{};
  blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALPHA;
  D3D11_DEPTH_STENCIL_DESC behind{};
  behind.DepthEnable = TRUE;
  behind.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  behind.DepthFunc = D3D11_COMPARISON_GREATER;
  if (FAILED(device_->CreateBlendState(&blend, &alpha_only_)) ||
      FAILED(device_->CreateDepthStencilState(&behind, &behind_drawn_))) {
    throw std::runtime_error("Unable to create the coverage pass's states");
  }
  D3D11_BUFFER_DESC desc{};
  desc.ByteWidth = sizeof(ScreenConstants);
  desc.Usage = D3D11_USAGE_DYNAMIC;
  desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (FAILED(device_->CreateBuffer(&desc, nullptr, &screen_constants_))) {
    throw std::runtime_error("Unable to create the screen pass's constants");
  }
}

void WorldRenderer::ensure_blur_targets() {
  const int width = std::max(width_ / 2, 1);
  const int height = std::max(height_ / 2, 1);
  if (blur_[0] && width == blur_width_ && height == blur_height_) {
    return;
  }
  D3D11_TEXTURE2D_DESC desc{};
  desc.Width = static_cast<UINT>(width);
  desc.Height = static_cast<UINT>(height);
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = format_;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  for (std::size_t i = 0; i < blur_.size(); ++i) {
    blur_[i].Reset();
    blur_targets_[i].Reset();
    blur_views_[i].Reset();
    if (FAILED(device_->CreateTexture2D(&desc, nullptr, &blur_[i])) ||
        FAILED(device_->CreateRenderTargetView(blur_[i].Get(), nullptr, &blur_targets_[i])) ||
        FAILED(device_->CreateShaderResourceView(blur_[i].Get(), nullptr, &blur_views_[i]))) {
      blur_[0].Reset();
      throw std::runtime_error("Unable to create the screen pass's blur targets");
    }
  }
  blur_width_ = width;
  blur_height_ = height;
}

void WorldRenderer::render_coverage() {
  const D3D11_VIEWPORT whole{0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_),
                             0.0F, 1.0F};
  context_->RSSetViewports(1, &whole);
  ID3D11RenderTargetView *target = colour_view_.Get();
  context_->OMSetRenderTargets(1, &target, depth_view_.Get());
  context_->IASetInputLayout(nullptr);
  context_->VSSetShader(far_vertex_.Get(), nullptr, 0);
  context_->PSSetShader(cover_pixel_.Get(), nullptr, 0);
  context_->RSSetState(cull_none_.Get());
  context_->OMSetBlendState(alpha_only_.Get(), nullptr, 0xffffffff);
  context_->OMSetDepthStencilState(behind_drawn_.Get(), 0);
  context_->Draw(3, 0);
  context_->IASetInputLayout(shaders_.layout.Get());
  context_->VSSetShader(shaders_.vertex.Get(), nullptr, 0);
  context_->PSSetShader(shaders_.pixel.Get(), nullptr, 0);
}

void WorldRenderer::render_screen(const WorldFrame &frame) {
  const ScreenPass &pass = frame.lighting.screen;
  if (!pass.active() || !scene_view_ || !depth_read_view_) {
    return;
  }
  const bool blur = pass.depth_of_field;
  if (blur) {
    try {
      ensure_blur_targets();
    } catch (const std::exception &error) {
      SDL_Log("%s", error.what());
      return;
    }
  }
  if (samples_ > 1) {
    context_->ResolveSubresource(scene_.Get(), 0, colour_.Get(), 0, format_);
  } else {
    context_->CopyResource(scene_.Get(), colour_.Get());
  }

  const float depth_span = pass.depth_far - pass.depth_near;
  ScreenConstants constants{};
  constants.threshold = pass.focus * pass.focus_scale + pass.focus_base;
  constants.ramp = pass.ramp;
  constants.tint = pass.tint;
  constants.blur_on = blur ? 1.0F : 0.0F;
  constants.depth_a = frame.projection.at(2, 2);
  constants.depth_b = frame.projection.at(3, 2);
  constants.original_scale = depth_span > 0.0F ? pass.depth_far / depth_span : 1.0F;
  constants.original_near = pass.depth_near;
  // One of the original's texels, the same size on both axes of any window.
  const float tap_v = pass.blur_texels / original_height;
  const float tap_u = tap_v * static_cast<float>(height_) / static_cast<float>(width_);

  ID3D11SamplerState *sampler = scene_sampler_.Get();
  ID3D11Buffer *buffer = screen_constants_.Get();
  context_->IASetInputLayout(nullptr);
  context_->VSSetShader(screen_vertex_.Get(), nullptr, 0);
  context_->PSSetConstantBuffers(0, 1, &buffer);
  context_->PSSetSamplers(0, 1, &sampler);
  context_->RSSetState(cull_none_.Get());
  context_->OMSetBlendState(opaque_.Get(), nullptr, 0xffffffff);
  context_->OMSetDepthStencilState(depth_off_.Get(), 0);
  const auto set_constants = [&](const float scale) {
    constants.tap = {tap_u * scale, tap_v * scale};
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(context_->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
      std::memcpy(mapped.pData, &constants, sizeof(constants));
      context_->Unmap(buffer, 0);
    }
  };
  ID3D11ShaderResourceView *none[3] = {};
  if (blur) {
    // The frame into the first half, then the first half (whose texels are
    // twice the size) into the second.
    const D3D11_VIEWPORT half{
        0.0F, 0.0F, static_cast<float>(blur_width_), static_cast<float>(blur_height_), 0.0F, 1.0F};
    context_->RSSetViewports(1, &half);
    context_->PSSetShader(blur_pixel_.Get(), nullptr, 0);
    ID3D11ShaderResourceView *sources[] = {scene_view_.Get(), blur_views_[0].Get()};
    for (std::size_t i = 0; i < blur_.size(); ++i) {
      ID3D11RenderTargetView *target = blur_targets_[i].Get();
      context_->OMSetRenderTargets(1, &target, nullptr);
      context_->PSSetShaderResources(0, 1, &sources[i]);
      set_constants(static_cast<float>(i + 1));
      context_->Draw(3, 0);
      context_->PSSetShaderResources(0, 1, none);
    }
  } else {
    set_constants(1.0F);
  }
  const D3D11_VIEWPORT whole{0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_),
                             0.0F, 1.0F};
  context_->RSSetViewports(1, &whole);
  ID3D11RenderTargetView *target = colour_view_.Get();
  context_->OMSetRenderTargets(1, &target, nullptr);
  context_->PSSetShader((samples_ > 1 ? screen_pixel_ms_ : screen_pixel_).Get(), nullptr, 0);
  ID3D11ShaderResourceView *reads[] = {
      scene_view_.Get(), blur ? blur_views_[1].Get() : scene_view_.Get(), depth_read_view_.Get()};
  context_->PSSetShaderResources(0, 3, reads);
  context_->Draw(3, 0);

  // Back to the world's bindings for the cockpit.
  context_->PSSetShaderResources(0, 3, none);
  context_->OMSetRenderTargets(1, &target, depth_view_.Get());
  ID3D11Buffer *world_constants[] = {frame_constants_.Get(), draw_constants_.Get()};
  context_->PSSetConstantBuffers(0, 2, world_constants);
  context_->IASetInputLayout(shaders_.layout.Get());
  context_->VSSetShader(shaders_.vertex.Get(), nullptr, 0);
  context_->PSSetShader(shaders_.pixel.Get(), nullptr, 0);
}

} // namespace phantomcrash
