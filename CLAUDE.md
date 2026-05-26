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
- `src/renderer.{h,cpp}` + `src/gl.h` — the ONLY OpenGL code. Two-pass: render
  fractal into a supersampled FBO, then gamma-correct downsample into the output
  FBO, then `glReadPixels`.
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
- **Coloring is the whole game, and SAC is the answer.** Stripe Average
  Coloring (`fractal.frag`, faithfully following the philthompson article) is
  the default and primary look: average `½+½·sin(freq·arg z)` over the orbit,
  then interpolate incl/excl the last point by `frac(mu)`. That de-banding
  interpolation is non-negotiable — without it you get level-set seams; with it
  the smooth field reads as 3D relief. Needs a LARGE bailout (default 10000).
- **Restrained ramp palettes are essential.** A full-spectrum hue wheel turns
  SAC's fine detail into rainbow noise ("smushed grey"). Built-ins are
  dark→bright ramps; default gradient is NOT looped (`cyclic=false`) so low
  stripe → dark/void, high → bright. Perceptually-uniform `magma`/`viridis`
  are reliable. Cycle-mode video forces `cyclic=true`.
- **`--falloff`** (distance estimate) carves the black negative space that
  makes the relief pop. It's back ON by default (0.014) precisely for that.
- **These are OFF by default because they fight the clean SAC look:**
  `--shading` (the SAC article warns slope shading adds "pointy" artifacts to
  smooth stripe areas), `--color-density` (iteration hue → rainbow),
  `--angle-color` (chaotic speckle deep in the body), `--trap-color` (its raw
  min can seam). They remain available for vibrant/experimental looks.
- Video uses lower default SSAA (2) than stills (4) because it renders hundreds
  of frames. x264 + yuv420p needs even dimensions (handled in `runVideo`).

## Conventions

- Match the surrounding style: namespaced in `fractal`, comments explain *why*.
- Keep GL strictly inside `renderer.cpp`. Everything else stays testable.
- Default parameter values are tuned to look good with a bare `fractal render`;
  if you retune defaults, re-check the gallery in `assets/` and update README.
