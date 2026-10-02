#pragma once

namespace phantomcrash {
inline constexpr char dynamic_light_hlsl[] = R"hlsl(
struct LocalLight {
  row_major float4x4 projection;
  float4 position_range;
  float4 direction_outer;
  float4 colour_inner;
};
cbuffer LocalLights : register(b2) {
  LocalLight lights[8];
  float4 light_count;
};
Texture2DArray<float> light_depth : register(t3);
SamplerComparisonState light_sampler : register(s3);
float3 local_lighting(float3 position, float3 normal) {
  float3 result = 0;
  [loop] for (int i = 0; i < (int)light_count.x; ++i) {
    LocalLight light = lights[i];
    float3 delta = light.position_range.xyz - position;
    float d = length(delta);
    float3 toward = delta / max(d, 0.0001);
    float cone = smoothstep(light.direction_outer.w, light.colour_inner.w,
                            dot(-toward, light.direction_outer.xyz));
    float falloff = saturate(1.0 - d / light.position_range.w);
    float diffuse = saturate(dot(normal, toward));
    if (cone * falloff * diffuse <= 0) continue;
    float4 clip_position = mul(float4(position, 1), light.projection);
    if (clip_position.w <= 0) continue;
    float3 projected = clip_position.xyz / clip_position.w;
    float2 uv = projected.xy * float2(0.5, -0.5) + 0.5;
    if (any(uv < 0) || any(uv > 1) || projected.z < 0 || projected.z > 1) continue;
    float visibility = 0;
    [unroll] for (int y = -1; y <= 1; ++y) {
      [unroll] for (int x = -1; x <= 1; ++x) {
        visibility += light_depth.SampleCmpLevelZero(light_sampler,
          float3(uv + float2(x,y) * light_count.y, i), projected.z - 0.00005);
      }
    }
    result += light.colour_inner.rgb * (cone * falloff * falloff * diffuse * visibility / 9);
  }
  return result;
}
)hlsl";
} // namespace phantomcrash
