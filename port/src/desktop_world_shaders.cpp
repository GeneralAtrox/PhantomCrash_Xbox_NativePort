#include "desktop_world_shaders.hpp"

#include <d3dcompiler.h>

#include <stdexcept>
#include <string>

namespace phantomcrash {
namespace {

// Approximates the original's fixed-function path (0x0b3240): the vertex
// colour is the diffuse colour, lit by an ambient term and one directional
// light unless the batch is unlit (flag 0x100), times the base texture; a
// bump batch perturbs the normal with its normal map. Fog is linear by
// distance. Matrices are row-major for row vectors, as on the CPU side.
//
// Optic Camouflage follows multi_optical.xvu and Blend.xpu: per vertex, how
// much is seen (inside the sphere) and where the frame behind is picked:
// the vertex's clip position moved by its normal, pushed and projected
// without the translation (the original adds P * n to the position before
// the divide); per pixel, the lit robot over that pick.
constexpr char source[] = R"hlsl(
cbuffer Frame : register(b0) {
  row_major float4x4 view_projection;
  float3 eye;           float fog_start;
  float3 ambient;       float fog_end;
  float3 sun_direction; float fog_enabled;
  float3 sun_colour;    float shadow_length;
  float3 fog_colour;    float frame_pad1;
};

cbuffer Draw : register(b1) {
  row_major float4x4 world;
  float4 base_colour;
  float alpha_cutoff;
  float unlit;
  float has_base;
  float has_normal;
  float alpha_test;
  float fogged;
  float item_alpha;
  float flash;
  float2 uv_offset;
  float optical;         // 1: Optic Camouflage
  float draw_pad;
  float4 optical_sphere; // centre, radius
  float4 optical_shape;  // edge, reach, least, push
  float4 uv_transform;   // UV scale and offset (TV atlas rectangle -> complete live feed)
};

struct VertexIn {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float4 colour : COLOR;
  float2 uv : TEXCOORD0;
};

struct VertexOut {
  float4 position : SV_Position;
  float3 world_position : WORLDPOS;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float4 colour : COLOR;
  float2 uv : TEXCOORD0;
  float4 behind : TEXCOORD1;
  float seen : TEXCOORD2;
};

VertexOut vs_main(VertexIn input) {
  VertexOut output;
  float4 placed = mul(float4(input.position, 1.0), world);
  output.position = mul(placed, view_projection);
  output.world_position = placed.xyz;
  output.normal = mul(input.normal, (float3x3)world);
  output.tangent = float4(mul(input.tangent.xyz, (float3x3)world), input.tangent.w);
  output.colour = input.colour;
  output.uv = input.uv * uv_transform.xy + uv_transform.zw + uv_offset;
  output.behind = output.position;
  output.seen = 1.0;
  if (optical > 0.5) {
    float d = distance(placed.xyz, optical_sphere.xyz);
    output.seen = saturate((optical_sphere.w - d) / optical_shape.x);
    float share = clamp((optical_sphere.w + optical_shape.y - d) / optical_shape.y,
                        optical_shape.z, 1.0);
    float3 n = normalize(output.normal);
    output.behind.xy += mul(float4(n * share * optical_shape.w, 0.0), view_projection).xy;
  }
  return output;
}

Texture2D base_texture : register(t0);
Texture2D normal_texture : register(t1);
Texture2D scene_texture : register(t2);
SamplerState base_sampler : register(s0);
SamplerState normal_sampler : register(s1);
SamplerState scene_sampler : register(s2);

float4 ps_main(VertexOut input, bool front : SV_IsFrontFace) : SV_Target {
  float4 colour = input.colour * base_colour;
  if (has_base > 0.5) {
    colour *= base_texture.Sample(base_sampler, input.uv);
  }
  if (alpha_test > 0.5) {
    clip(colour.a - alpha_cutoff);
  }
  if (unlit < 0.5) {
    float3 n = normalize(input.normal) * (front ? 1.0 : -1.0);
    if (has_normal > 0.5) {
      float3 t = input.tangent.xyz - n * dot(n, input.tangent.xyz);
      t = normalize(t + 1e-6);
      float3 b = cross(n, t) * input.tangent.w;
      float3 m = normal_texture.Sample(normal_sampler, input.uv).xyz * 2.0 - 1.0;
      n = normalize(t * m.x + b * m.y + n * m.z);
    }
    float diffuse = saturate(dot(n, -sun_direction));
    colour.rgb *= saturate(ambient + sun_colour * diffuse);
  }
  if (fog_enabled > 0.5 && fogged > 0.5) {
    float distance_to_eye = distance(input.world_position, eye);
    float amount = saturate((distance_to_eye - fog_start) / max(fog_end - fog_start, 1e-3));
    colour.rgb = lerp(colour.rgb, fog_colour, amount);
  }
  colour.rgb = lerp(colour.rgb, float3(1.0, 1.0, 1.0), flash * 0.6);
  if (optical > 0.5) {
    float2 at = input.behind.xy / input.behind.w * float2(0.5, -0.5) + 0.5;
    float3 behind = scene_texture.Sample(scene_sampler, at).rgb;
    colour.rgb = lerp(behind, colour.rgb, input.seen);
  }
  colour.a *= item_alpha;
  return colour;
}
)hlsl";

} // namespace

Microsoft::WRL::ComPtr<ID3DBlob> compile_world_hlsl(const std::string_view hlsl, const char *entry,
                                                    const char *target) {
  Microsoft::WRL::ComPtr<ID3DBlob> code;
  Microsoft::WRL::ComPtr<ID3DBlob> errors;
  const HRESULT result =
      D3DCompile(hlsl.data(), hlsl.size(), "phantomcrash_world", nullptr, nullptr, entry, target,
                 D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
  if (FAILED(result)) {
    std::string message = "World shader ";
    message += entry;
    message += " failed to compile";
    if (errors) {
      message += ": ";
      message += static_cast<const char *>(errors->GetBufferPointer());
    }
    throw std::runtime_error(message);
  }
  return code;
}

WorldShaders create_world_shaders(ID3D11Device *device) {
  WorldShaders shaders;
  const std::string_view hlsl(source, sizeof(source) - 1);
  const auto vertex = compile_world_hlsl(hlsl, "vs_main", "vs_5_0");
  const auto pixel = compile_world_hlsl(hlsl, "ps_main", "ps_5_0");
  if (FAILED(device->CreateVertexShader(vertex->GetBufferPointer(), vertex->GetBufferSize(),
                                        nullptr, &shaders.vertex)) ||
      FAILED(device->CreatePixelShader(pixel->GetBufferPointer(), pixel->GetBufferSize(), nullptr,
                                       &shaders.pixel))) {
    throw std::runtime_error("Unable to create the world shaders");
  }
  const D3D11_INPUT_ELEMENT_DESC layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };
  if (FAILED(device->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)),
                                       vertex->GetBufferPointer(), vertex->GetBufferSize(),
                                       &shaders.layout))) {
    throw std::runtime_error("Unable to create the world vertex layout");
  }
  return shaders;
}

} // namespace phantomcrash
