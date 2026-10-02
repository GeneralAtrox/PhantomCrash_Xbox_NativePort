# Optional dynamic lighting

Settings > Visuals > Dynamic lighting and shadows enables the native spotlight
pass. It defaults off, including for older settings files. The modeless Visuals
panel also contains the existing post effects, SMAA and display/frame-rate
controls. Changes affect the next rendered frame and save immediately without
saving the current match. The panel remains open for comparison; the game
continues rendering and simulating while it has focus.

The preference is `post_process.dynamic_lighting` in the desktop settings file.
The group retains its existing name for compatibility with the other rendering
preferences. Alt+Enter and Alt+Numpad Enter both toggle fullscreen; F11 remains
available in the game window.

## Light sources

`data/world/dynamic_lighting.json` contains port-authored enhancements, not
recovered Xbox local-light values. `mods/data/world/dynamic_lighting.json` can
override them. Night-stage entrances have fixed overhead spotlights; the night
video ship has four lamps below its authored rotor pivots. Their positions
follow the ship's route and their directions rotate with its rotors.

- `stages` keys are scene-stage numbers, with world-space positions/directions.
- `ship` keys are model parity (0 day, 1 night), with local positions/directions.
- `colour` is linear RGB, `intensity` a multiplier, and `range` world units.
- `inner_degrees`/`outer_degrees` are cone half-angles, with a smooth edge.
- Range attenuation falls to zero at `range`.

The defaults add lights to night stages only. Enabling the option does not
replace the original ambient/directional lighting or brighten the entire sky.

## Rendering and replay

Up to eight spotlights use 1024x1024 D32 shadow-map slices, with alpha-tested
cut-outs and two-sided solid casters. Robots, wrecks, doors and buildings block
the light. Sky, monitor faces, particles, transparent glow layers, optical
camouflage and flat-shadow decals do not cast these shadows. Local lighting
uses surface normals/normal maps and distance/cone attenuation; shadow lookup
uses a filtered 3x3 comparison kernel and depth bias.

The main view and its live monitor feed share the maps, generated before either
view. World-space lights are recorded with replay frames, so replay cameras and
live toggling use the recorded moving-light positions. Cockpit/HUD and live TV
faces are not illuminated by the spotlight pass.

Disabled means no shadow-map draws; maps allocate lazily on first enable.
Allocation/shader failures log and disable the pass until the option is changed.
The fixed budget bounds cost but this is an additional geometry pass per light,
so enabling it can reduce frame rate. This implements surface illumination and
shadows, not volumetric light shafts.

## Verification

Release build completed. Separate night-scene captures with the option off/on
confirmed local illumination on the robot and ground. A bounded native UI run
opened Visuals, toggled lighting on/off/on, saved the four existing effect
preferences, and switched fullscreen with Alt+keypad Enter and back with
Alt+Enter. Reload and replay playback/reset/return completed with lighting and
SMAA enabled and no shader/resource errors. No new automated tests were added.
