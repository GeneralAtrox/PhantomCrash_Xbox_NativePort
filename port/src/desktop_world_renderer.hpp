#pragma once

#include "desktop_world_gpu.hpp"
#include "desktop_world_shaders.hpp"
#include "phantomcrash/math3d.hpp"
#include "phantomcrash/optical_camouflage.hpp"
#include "phantomcrash/training_ground.hpp"

#include <SDL3/SDL.h>

#include <d3d11_1.h>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

// One model placed in the world. Nodes named in `sky_nodes` are drawn first,
// around the camera (the view without its translation) and without depth,
// as the original draws a stage's sky with its own view (layer 1 of the
// stage object list at 0x2212ec).
//
// An animated item (a robot part) gives every node its world matrix in
// `node_worlds` instead, and `paint` (from WorldRenderer::texture) replaces
// texture 0's image wherever the model uses it (keeping each texture's wrap
// modes): the robot's own paint, which the original binds per robot.
struct WorldItem {
  int model{-1};
  Mat4 world;
  std::vector<std::string> sky_nodes;
  std::vector<Mat4> node_worlds;
  int paint{-1};
  float alpha{1.0F}; // below 1 the item is drawn see-through
  float flash{};     // 0..1 toward white (a hit)
  int node{-1};      // draws only this node of the model, when set
  // HUD models (the cockpit): a colour in place of the materials' own, a
  // shift of the texture coordinates, a stretch along the nodes' own x and
  // additive blending.
  std::optional<std::array<float, 4>> colour;
  std::array<float, 2> uv_offset{};
  float scale_x{1.0F};
  bool additive{};
  // Optic Camouflage: outside the sphere the item shows the opaque world
  // behind it, bent along its normals (optical_camouflage.hpp). Drawn after
  // the opaque world, which is copied for it.
  std::optional<OpticalSphere> optical;
  // Casts a stencil shadow volume from its nodes (WorldRenderer::
  // prepare_shadow builds it) and is itself never shadowed.
  bool casts_shadow{};
  // Drawn from both sides whatever its materials say (the flat ground
  // shadows, whose quads face down).
  bool double_sided{};
};

// A picture in the world that faces the camera, or runs along a streak from
// `position` to `end`: the particles and the pickups' glow. `size` is half
// its width, `uv` its corners in `texture` (u0, v0, u1, v1) and `colour`
// straight (not multiplied by its alpha). An additive sprite is added to
// what is behind it; the others are laid over it.
struct WorldSprite {
  int texture{-1};
  Vec3 position;
  Vec3 end;
  bool streak{};
  float size{};
  float rotation{};
  std::array<float, 4> uv{0.0F, 0.0F, 1.0F, 1.0F};
  std::array<float, 4> colour{1.0F, 1.0F, 1.0F, 1.0F};
  bool additive{};
};

struct WorldFrame {
  int width{}, height{};
  Mat4 view;
  Mat4 projection;
  Vec3 eye;
  StageLighting lighting;
  std::vector<WorldItem> items;
  // Drawn last, in order, in the eye's space (+z ahead) with the same
  // projection, without fog and over everything: the cockpit (view layer
  // 0xb).
  std::vector<WorldItem> overlay;
  // Drawn in order after the see-through items, depth tested and not
  // written.
  std::vector<WorldSprite> sprites;
  float shadow_length{60.0F}; // how far the shadow volumes reach
  // Cleared see-through and laid over what the frame has drawn so far (a
  // menu's robot over its backdrop); the stage's clear colour and the
  // screen pass are left out.
  bool transparent{};
};

// Draws the 3D world with Direct3D 11 on the device of SDL's renderer, so
// the 2D screens keep drawing through SDL on top. The world renders into its
// own multisampled targets inside its own device-context state (SDL's state
// is swapped back untouched) and resolves into an SDL texture, drawn under
// the frame's 2D layers.
class WorldRenderer final {
public:
  WorldRenderer(SDL_Renderer *renderer, const DataRoots &roots);
  ~WorldRenderer();
  WorldRenderer(const WorldRenderer &) = delete;
  WorldRenderer &operator=(const WorldRenderer &) = delete;

  // False when SDL's renderer is not Direct3D 11 or setup failed (logged).
  [[nodiscard]] bool available() const noexcept {
    return device_ != nullptr;
  }
  // The model at `asset_path`, loaded on first use; -1 when it cannot load.
  [[nodiscard]] int model(const std::string &asset_path);

  // Builds `model`'s shadow volume for items that cast one (at load time,
  // so the first frame with the caster does not wait for it).
  void prepare_shadow(int model);
  // The CPU description of a loaded model, or null.
  [[nodiscard]] const ModelData *model_data(int model) const;
  // The texture at `asset_path`, loaded on first use; -1 when it cannot load.
  [[nodiscard]] int texture(const std::string &asset_path);
  // Renders `frame` and draws it over the whole window.
  void draw(const WorldFrame &frame);

private:
  struct Draw {
    const GpuModel *model;
    const ModelDraw *draw;
    Mat4 world;
    float distance;
    ID3D11ShaderResourceView *paint;
    float alpha;
    float flash;
    int group; // the see-through item an opaque batch belongs to, or -1
    const WorldItem *item;
    bool sky_node;
    bool blend;
  };

  void create_states();
  void ensure_targets(int width, int height);
  void render(const WorldFrame &frame);
  // Appends the batches of `item` (item `index` of the frame) to `out`.
  void item_draws(const WorldItem &item, int index, Vec3 eye, std::vector<Draw> &out) const;
  void render_overlay(const WorldFrame &frame);
  // Copies the world drawn so far for the camouflaged items to bend, and
  // draws them.
  void render_optical(const std::vector<Draw> &draws, const StageLighting &lighting);
  void create_shadow_states();
  // Counts the casters' volumes into the stencil; false when there were none.
  [[nodiscard]] bool render_volumes(const WorldFrame &frame);
  // Lays the stage's shadow colour where the count is not 0.
  void cover_shadows(const StageLighting &lighting);
  void create_sprite_states();
  void render_sprites(const WorldFrame &frame);
  void set_frame_constants(const WorldFrame &frame, const Mat4 &view);
  void create_screen_states();
  // The stage's screen pass (ScreenPass) over the world drawn so far, ahead
  // of the cockpit (desktop_world_screen.cpp).
  void render_screen(const WorldFrame &frame);
  // Marks a transparent frame's drawn pixels opaque (desktop_world_screen.cpp).
  void render_coverage();
  void ensure_blur_targets();
  // `depth_prepass` writes only depth, so that a see-through item blends
  // just its nearest surface instead of its inner faces.
  void draw_one(const Draw &draw, bool sky, const StageLighting &lighting,
                bool depth_prepass = false, bool overlay = false);

  SDL_Renderer *renderer_;
  const DataRoots &roots_;
  Com<ID3D11Device1> device_;
  Com<ID3D11DeviceContext1> context_;
  Com<ID3DDeviceContextState> state_;
  WorldShaders shaders_;
  ShadowShaders shadow_shaders_;
  std::unique_ptr<GpuResources> resources_;
  std::vector<std::unique_ptr<GpuModel>> models_;
  std::map<std::string, int> model_ids_;
  std::vector<Com<ID3D11ShaderResourceView>> textures_;
  std::map<std::string, int> texture_ids_;
  Com<ID3D11Buffer> frame_constants_;
  Com<ID3D11Buffer> draw_constants_;
  Com<ID3D11RasterizerState> cull_back_;
  Com<ID3D11RasterizerState> cull_none_;
  Com<ID3D11BlendState> opaque_;
  Com<ID3D11BlendState> alpha_;
  Com<ID3D11BlendState> additive_;
  Com<ID3D11BlendState> depth_only_;
  Com<ID3D11DepthStencilState> depth_write_;
  Com<ID3D11DepthStencilState> depth_read_;
  Com<ID3D11DepthStencilState> depth_off_;
  Com<ID3D11RasterizerState> volume_raster_;
  Com<ID3D11DepthStencilState> volume_depth_;  // counts faces behind the stage
  Com<ID3D11DepthStencilState> clear_stencil_; // the casters: depth as usual, count to 0
  Com<ID3D11DepthStencilState> shadowed_;      // where the count is not 0
  Com<ID3D11VertexShader> sprite_vertex_;
  Com<ID3D11PixelShader> sprite_pixel_;
  Com<ID3D11InputLayout> sprite_layout_;
  Com<ID3D11BlendState> premultiplied_; // ONE, INVSRCALPHA
  Com<ID3D11SamplerState> sprite_sampler_;
  Com<ID3D11Buffer> sprite_vertices_;
  UINT sprite_capacity_{}; // sprites the buffer holds
  // Targets at the window's size.
  int width_{}, height_{};
  UINT samples_{1};
  DXGI_FORMAT format_{DXGI_FORMAT_R8G8B8A8_UNORM};
  Com<ID3D11Texture2D> colour_;
  Com<ID3D11RenderTargetView> colour_view_;
  Com<ID3D11Texture2D> depth_;
  Com<ID3D11DepthStencilView> depth_view_;
  Com<ID3D11Texture2D> scene_; // the copy Optic Camouflage samples
  Com<ID3D11ShaderResourceView> scene_view_;
  Com<ID3D11SamplerState> scene_sampler_;
  Com<ID3D11ShaderResourceView> depth_read_view_; // the depth, for the screen pass
  // The screen pass: its shaders, and the two half-size targets it blurs into.
  Com<ID3D11VertexShader> screen_vertex_;
  Com<ID3D11PixelShader> blur_pixel_;
  Com<ID3D11PixelShader> screen_pixel_;    // reads a single-sampled depth
  Com<ID3D11PixelShader> screen_pixel_ms_; // reads sample 0 of a multisampled one
  // A transparent frame's coverage: alpha 1 wherever depth was written.
  Com<ID3D11VertexShader> far_vertex_;
  Com<ID3D11PixelShader> cover_pixel_;
  Com<ID3D11BlendState> alpha_only_;
  Com<ID3D11DepthStencilState> behind_drawn_;
  Com<ID3D11Buffer> screen_constants_;
  std::array<Com<ID3D11Texture2D>, 2> blur_;
  std::array<Com<ID3D11RenderTargetView>, 2> blur_targets_;
  std::array<Com<ID3D11ShaderResourceView>, 2> blur_views_;
  int blur_width_{}, blur_height_{};
  SDL_Texture *output_{};
  ID3D11Texture2D *output_texture_{}; // owned by output_
};

} // namespace phantomcrash
