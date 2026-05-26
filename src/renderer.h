// renderer.h — offscreen GPU fractal renderer.
//
// Creates a hidden GLFW/OpenGL context, renders the fractal into a supersampled
// framebuffer, resolves it down with a gamma-correct box filter, and hands back
// RGB8 pixels. All GL state lives here; the rest of the program is GL-free.
#pragma once

#include "config.h"

#include <cstdint>
#include <string>
#include <vector>

namespace fractal {

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Create the context and GPU resources for a `width`x`height` output with
    // the given supersampling factor. `shader_dir` may be empty to auto-search.
    // Returns false and fills `err` on failure.
    bool init(int width, int height, int ssaa,
              const std::string& shader_dir, std::string& err);

    // Upload the gradient (RGB8, `resolution` texels) as the palette texture.
    void setPalette(const std::vector<uint8_t>& rgb, int resolution);

    // Render one frame. Image dimensions must match init().
    void render(const RenderConfig& cfg);

    // Resolved image as RGB8, row-major with the top row first (PNG order).
    std::vector<uint8_t> readPixels();

    int width()  const { return width_; }
    int height() const { return height_; }

private:
    int width_ = 0, height_ = 0, ssaa_ = 1;

    unsigned int fractal_prog_   = 0;
    unsigned int downsample_prog_ = 0;
    unsigned int vao_            = 0;
    unsigned int palette_tex_    = 0;

    unsigned int fbo_hi_ = 0, tex_hi_ = 0; // supersampled render target
    unsigned int fbo_lo_ = 0, tex_lo_ = 0; // resolved output

    void* window_ = nullptr; // GLFWwindow*
};

} // namespace fractal
