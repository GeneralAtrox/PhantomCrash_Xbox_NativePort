# Entry shield rendering

Validated against the European disc's XBE and shipped shader binaries on
2026-09-30. This is a static validation of the original rendering path; it
does not claim an emulator capture comparison or a corrected native renderer.

## Shader selection

`0x06f592` draws object slot `0x41b` (resource 197, mesh 30) with object flag
`0x100000`. The flagged branch in `0x0b3240` selects dispatch entry `0x19`.
The two dispatch tables resolve that entry to:

| Responsibility | Function | Evidence |
| --- | --- | --- |
| Bind programs | `0x0c5179` | Vertex handle `0xa112dc`, pixel handle `0xa11338` |
| Set constants and texture | `0x0c43ae` | Constants c0–c9 and texture stage 0 |

The shader loader `0x0beee0` maps those handles to vertex table entry 21
(`0x280a50`) and pixel table entry 18 (`0x280c90`):
`Media/Shaders/rainbow.xvu` and `Media/Shaders/rainbow.xpu`.
The previous attribution to `barrier.xvu` was incorrect.

## Colour and waves

The sphere contains 580 vertices, with object-space coordinates approximately
−0.5 to +0.5 on each axis. The rainbow vertex program has 48 instructions.
It calculates RGB at vertices from object-space height and three phase offsets,
then the rasterizer interpolates the colours across the sphere.

`0x0c43ae` supplies these constants:

| Constant | Value |
| --- | --- |
| c4.xyz | Negated lighting direction, normalized |
| c5.xyz, c6.xyz | Lighting colours from the object's lighting record |
| c7 | `(1, 0, scroll, 0.5)` |
| c8 | `(phase − 0.6666, phase, phase + 0.6666, 2)` |
| c9 | `(glow, 0, 0, 0)` |

The reciprocal-square-root/reciprocal pairs implement absolute values. For
each channel, the colour term is equivalent to:

```text
d = abs(vertex.y + channel_phase)
rainbow = abs((d >= 1 ? 2 : 0) - d)
light = c5.rgb * max(dot(c4.xyz, vertex.normal), 0) + c6.rgb
vertex.rgb = 0.75 * light + 0.5 * rainbow
vertex.alpha = glow
uv = (vertex.x + 0.5, vertex.y + 0.5 + scroll)
```

The phase at `0xa69c44` decreases by `0.03` per invocation of `0x0c43ae`,
resetting to `1` when below `−1`. The scroll at `0x281828` starts at `1`,
decreases by `0.01` per invocation, and adds `1` when below zero. These are
shared draw-path updates, not evidence of a separate per-robot simulation
timer. A native implementation must account for repeated rendering, including
monitor views, without accidentally advancing the effect for each extra pass.

`Media/systemdata/muteki01.tga` is a 64×64 texture. Every RGB texel is white;
its alpha ranges from 5 to 255. The pixel program copies the interpolated RGB
and multiplies texture alpha by interpolated vertex alpha. The texture supplies
waves, not the rainbow colours. The draw colour `0x64ffffff` is passed by the
caller, but the rainbow vertex program supplies alpha from c9.x instead of
consuming that diffuse alpha. `0x0c4373` clamps the supplied glow to [0, 1].

## Native discrepancy

`effect_convert._shield_mesh` currently generates twice-repeated planar UVs,
white vertices and an unlit material. `BattleView::add_shield` submits it through
the ordinary textured shader with white colour and an additional `100/255`
alpha multiplier. That path omits the original height-dependent animated RGB,
scrolling UV calculation and vertex lighting contribution, and reduces opacity.

This validation changes documentation only. The runtime still needs the
recovered rainbow shader path; changing the white texture to a rainbow image
would not reproduce the original mechanism.
