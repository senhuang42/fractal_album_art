# CLAUDE.md

Guidance for working in this repo. Keep it current as the project evolves.

## What this is

A C++/OpenGL CLI that renders Julia/Mandelbrot fractals as stills (PNG) and
seamless loop videos (MP4). GPU does the real work via GLSL fragment shaders;
the CLI just builds a config, drives the renderer, and writes files.

## Build / test / run

```sh
brew install glfw ffmpeg     # one-time deps
make                         # -> ./fractal  (arm64, -O3 -flto, system OpenGL + GLFW)
make test                    # -> ./run_tests (GL-free unit tests)
./fractal help               # full CLI reference
```

- Targets macOS / Apple Silicon. Uses Apple's system OpenGL framework (no
  glad/glew) + Homebrew GLFW. GL 3.3 core profile, runs on Metal.
- Shaders are loaded from `shaders/` at runtime (searched next to the binary
  and in CWD; override with `FRACTAL_SHADER_DIR` or `--shader-dir`). You can
  edit a shader and re-run **without** rebuilding the C++.

## Architecture

- `src/config.h` — all parameters as plain data (`RenderConfig`,
  `VideoConfig`). No GL, no I/O. Everything flows from here.
- `src/cli.{h,cpp}` — argv -> config. Pure and fully unit-tested. Add new flags
  here *and* in `helpText()` *and* cover them in `tests/test_cli.cpp`.
- `src/palette.{h,cpp}` — hex/named palette parsing -> RGB gradient bytes.
- `src/renderer.{h,cpp}` + `src/gl.h` — the ONLY OpenGL code. Passes: render
  fractal into a supersampled FBO -> gamma-correct downsample to output FBO ->
  (optional) bloom: bright-pass + separable Gaussian blur (`bloom.frag`, run H
  then V) screen-composited back (`composite.frag`) -> `glReadPixels` from
  whichever FBO was last written (`read_fbo_`).
- Shaders: `fractal.frag` (math/coloring/lighting), `downsample.frag` (resolve
  + grade), `bloom.frag` (bright-pass + separable blur), `composite.frag`.
- `shaders/fractal.frag` — the math + coloring + shading + traps.
- `src/fractal_math.h` — CPU reference that MIRRORS `fractal.frag`. Kept in sync
  so the algorithm is unit-testable without a GL context. If you change the
  iteration/coloring math in the shader, update this too (and its tests).
- `src/main.cpp` — PNG via `stb_image_write`, MP4 by piping raw RGB frames to
  `ffmpeg`. Per-frame animation lives in `frameConfig()`.

## Adding a parameter (the full path)

1. Field + default in `config.h`.
2. Flag parsing in `cli.cpp` + a line in `helpText()`.
3. If it affects rendering: a `uniform` in the relevant shader + a
   `setUniform*` call in `renderer.cpp::render()`.
4. A test in `tests/test_cli.cpp`.

## Gotchas / hard-won lessons

- **Makefile header deps**: objects are rebuilt when headers change via
  `-MMD -MP` + `-include *.d`. This matters because `config.h` is included
  everywhere — without it, changing a struct field gives an ABI mismatch
  between `main.o` and the others and the program crashes (SIGABRT) with
  garbage config. If you ever see weird crashes after editing a header,
  `make clean && make`.
- **Coloring is two layers, gated (the philthompson overlay idea).** See the
  color block in `fractal.frag`:
  1. *Iteration layer*: `iterS = 1 - exp(-mu * color_density)`. Fast-escape
     exterior → ~0 (dark), slow-escape filaments → ~1 (bright). This draws the
     structure, the dendrite tendrils, and the dark negative space.
  2. *Stripe layer (SAC)*: average `½+½·sin(freq·arg z)` over the orbit,
     interpolate incl/excl the last point by `frac(mu)`. The de-banding
     interpolation is non-negotiable (no it → level-set seams; with it → smooth
     3D relief). Needs a LARGE bailout (default 10000).
  - Combine: `mix(iterCol, stripeCol * smoothstep(0.12,0.55,iterS), stripe_color)`.
    The smoothstep gate keeps gaps black while the structure shows FULL stripe
    relief (a plain multiply dims the structure; a plain hard-light leaks bright
    fur into the gaps — both were tried and rejected).
  - Layer selection: `color_density==0` → stripe only (image #10);
    `stripe_color==0` → iteration only (image #9); both > 0 → overlay (#8).
- **Tendril reach is a property of the fractal, not a setting.** Whole-set
  Julia views have smooth far-field exteriors (no tendrils → SAC makes smooth
  "fur" there). Real dendrite tendrils live near the boundary — more dendritic
  `c` (e.g. -0.7269+0.1889i) or Mandelbrot regions, plus high iterations.
- **Restrained ramp palettes are essential.** A full-spectrum hue wheel turns
  fine detail into rainbow noise. Built-ins are dark→bright ramps; `cyclic`
  defaults false (cycle-mode video forces it true). `magma`/`viridis` are the
  perceptually-uniform matplotlib maps.
- **Off by default** (they fight the clean look): `--shading` (slope shading
  adds "pointy" artifacts per the SAC article), `--angle-color`, `--trap-color`,
  `--falloff` (the iteration layer now supplies negative space). All still there.
- Video uses lower default SSAA (2) than stills (4) because it renders hundreds
  of frames. x264 + yuv420p needs even dimensions (handled in `runVideo`).

## Conventions

- Match the surrounding style: namespaced in `fractal`, comments explain *why*.
- Keep GL strictly inside `renderer.cpp`. Everything else stays testable.
- Default parameter values are tuned to look good with a bare `fractal render`;
  if you retune defaults, re-check the gallery in `assets/` and update README.
