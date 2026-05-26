# fractal

A GPU fractal visualizer for stunning Julia & Mandelbrot art, with fully
customizable color and a one-command path to seamless 20-second loop videos.

Renders on the GPU via OpenGL/GLSL (Apple's Metal-backed OpenGL on macOS), so a
4K still at 16× supersampling renders in well under a second on Apple Silicon.

![grayscale stripe-average relief](assets/hero_noir.png)

| | |
|---|---|
| ![frost blue spiral](assets/julia_frost.png) | ![magma spiral](assets/julia_magma.png) |
| ![ember seahorse](assets/mandel_ember.png) | *…and whatever you can dial in* |

## Why it looks good

Pretty fractals are almost entirely about *coloring* and edge quality, not the
iteration loop. The centerpiece here is **Stripe Average Coloring**:

- **Stripe Average Coloring (SAC)** — the heart of the look. During iteration
  it averages `½ + ½·sin(s·arg z)` over the orbit, then interpolates between the
  averages including/excluding the last point using the fractional escape time.
  That de-banding interpolation is the magic: the result is a perfectly smooth
  scalar field whose contours follow the fractal's flow, so it reads as **3D
  relief** with no explicit lighting and **no level-set seams**. It also fills
  the "featureless" areas that plain iteration coloring leaves flat.
  (Härkönen 2007; faithfully following [philthompson.me](https://philthompson.me/2023/Stripe-Average-Coloring.html).)
- **Restrained, designed palettes** — the built-ins are dark→bright *ramps*
  within a harmonious hue family (plus the perceptually-uniform `magma`/
  `viridis`), so fine detail becomes coherent light/dark texture instead of
  rainbow noise. This is what keeps dense regions from "smushing into grey."
- **Distance-estimate negative space** — `d = √(|z|²/|dz|²)·½·log|z|²`
  ([Inigo Quilez](https://iquilezles.org/articles/distancefractals/)) fades the
  exterior to black, carving the dark voids that make the relief pop.
- **Gamma-correct supersampling** — rendered at up to 8× per axis and resolved
  with a box filter that averages in *linear* light, so edges are smooth and
  colors don't darken at boundaries.

Also available but **off by default** (they fight the clean SAC look): orbit
traps (`--trap-color`), escape-angle hue (`--angle-color`), iteration-band hue
(`--color-density`), and normal-map "fake 3D" shading (`--shading`, which the
SAC reference warns adds "pointy" artifacts to smooth stripe areas).

## Build (macOS / Apple Silicon)

Requires the Xcode command-line tools (clang), [Homebrew](https://brew.sh), and
GLFW. Video export needs ffmpeg.

```sh
brew install glfw ffmpeg
make            # builds ./fractal (arch arm64, -O3 -flto)
make test       # builds and runs the unit tests (no GPU needed)
```

The build links Apple's system OpenGL framework (no GL loader required) plus
GLFW. OpenGL on Apple Silicon runs on top of Metal and supports the GL 3.3 core
profile this project targets.

## Usage

```sh
fractal render [options]      # single still image -> PNG
fractal video  [options]      # seamless animation -> MP4 (needs ffmpeg)
fractal help                  # full option reference
```

### Stills

```sh
# The default look: grayscale stripe-average relief
fractal render -o spiral.png

# Same relief in blue, zoomed into a spiral
fractal render --center-x 0.27 --center-y -0.05 --zoom 2.4 -p frost --ssaa 6 -o frost.png

# Mandelbrot seahorse valley, warm palette
fractal render --type mandelbrot --center-x -0.74364388703 --center-y 0.13182590421 \
               --zoom 350 -i 2000 -p ember -o seahorse.png

# A vibrant, deliberately busy look (orbit traps + iteration hue, looped palette)
fractal render -p psychedelic --cyclic --trap-color 1.5 --color-density 0.1 --stripe-color 0 -o vivid.png
```

### Videos (20s loops)

Three animation modes, all built to loop seamlessly:

```sh
# rotate: the Julia constant orbits the origin -> morphs through forms
fractal video --mode rotate -d 20 --fps 30 -o loop.mp4

# zoom: smooth dive toward a target (great for Mandelbrot)
fractal video --type mandelbrot --mode zoom \
              --zoom-target-x -0.743 --zoom-target-y 0.131 --zoom-end 0.0005 -o dive.mp4

# cycle: static fractal, palette sweeps one full cycle (auto-loops the gradient)
fractal video --mode cycle -p magma -o cycle.mp4
```

### Color

Built-in palettes — all designed as dark→bright ramps so stripe detail stays
coherent: `noir` (default, grayscale), `frost` (blue), `magma`, `viridis`,
`ember`, `ice`, `fire`, `aurora`, `bloom`, `psychedelic`, `mono`. Or pass your
own comma-separated hex list (two or more stops):

```sh
fractal render -p "#00040c,#2f6fb0,#eaf7ff" -o custom.png
```

Key tuning knobs (see `fractal help` for the full list and defaults):

| Flag | Effect |
|---|---|
| `--cre`, `--cim` | Julia constant — the single biggest lever on the shape |
| `--zoom` / `--scale`, `--center-x/y` | Framing |
| `-i, --iterations` | Detail vs. speed; raise it for deep zooms |
| `--stripe-freq` | Stripe density (4/6/8 best); the texture frequency |
| `--stripe-contrast` | Stretch the stripe value across the gradient |
| `--falloff` | How tightly the image hugs the set before fading to black |
| `--ssaa` | Supersampling per axis (1–8); 4 = 16 samples/px |
| `--saturation`, `--gamma`, `--inside` | Final grade and interior color |
| `--cyclic` | Loop the gradient (for looped palettes / cycle videos) |
| `--color-density`, `--trap-color`, `--angle-color`, `--shading` | Optional extra coloring (off by default) |

## How it's organized

```
src/
  config.h         Plain-data params (RenderConfig, VideoConfig) — no GL
  fractal_math.h   CPU reference of the iteration (mirrors the shader; testable)
  palette.{h,cpp}  Hex/named palette parsing -> RGB gradient texture data
  cli.{h,cpp}      Argument parsing -> config (no I/O, fully testable)
  renderer.{h,cpp} The only OpenGL code: context, FBOs, shaders, readback
  gl.h             Single point of GL inclusion (macOS core profile)
  main.cpp         Wires it together; PNG via stb, MP4 via an ffmpeg pipe
shaders/
  fullscreen.vert  Full-screen triangle (no vertex buffer)
  fractal.frag     The fractal math, coloring, shading, traps
  downsample.frag  Gamma-correct supersampling resolve + final grade
tests/             Dependency-free unit tests for the GL-free units
```

The GPU is the production renderer; `fractal_math.h` re-implements the same
escape-time and distance-estimate math on the CPU purely so the algorithm can
be unit tested deterministically without a graphics context.

## Tests

```sh
make test
```

Covers hex/palette parsing and gradient generation, the escape-time and
distance-estimation math (membership, smoothness, monotonicity), and CLI
parsing including every error path. The renderer itself is validated by
rendering (see the gallery) since it needs a live GL context.

## License

MIT — see [LICENSE](LICENSE).
