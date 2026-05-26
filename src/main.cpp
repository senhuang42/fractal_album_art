// main.cpp — CLI entry point. Ties together argument parsing, gradient
// generation, the GPU renderer, PNG output, and MP4 export via ffmpeg.

#include "cli.h"
#include "palette.h"
#include "renderer.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kGradientResolution = 1024;

const char* kShaderDirEnv = "FRACTAL_SHADER_DIR"; // optional override

std::string shaderDirFromEnv() {
    const char* e = std::getenv(kShaderDirEnv);
    return e ? std::string(e) : std::string();
}

int runRender(const fractal::RenderConfig& cfg) {
    fractal::Renderer r;
    std::string err;
    if (!r.init(cfg.width, cfg.height, cfg.ssaa, shaderDirFromEnv(), err)) {
        std::cerr << "error: " << err << "\n";
        return 1;
    }
    auto grad = fractal::buildGradient(cfg.palette, kGradientResolution, cfg.cyclic);
    r.setPalette(grad, kGradientResolution);
    r.render(cfg);
    auto pixels = r.readPixels();

    if (!stbi_write_png(cfg.output.c_str(), cfg.width, cfg.height, 3,
                        pixels.data(), cfg.width * 3)) {
        std::cerr << "error: failed to write " << cfg.output << "\n";
        return 1;
    }
    std::cout << "wrote " << cfg.output << " (" << cfg.width << "x" << cfg.height
              << ", ssaa " << cfg.ssaa << ")\n";
    return 0;
}

// Compute the per-frame config for a given animation frame in [0, total).
fractal::RenderConfig frameConfig(const fractal::VideoConfig& v, int frame) {
    fractal::RenderConfig c = static_cast<fractal::RenderConfig>(v);
    const int   N = v.total_frames();
    const double u = (N > 0) ? static_cast<double>(frame) / N : 0.0; // [0,1)

    switch (v.mode) {
        case fractal::AnimMode::Rotate: {
            // Julia constant orbits the origin -> seamless loop.
            const double theta = 2.0 * M_PI * u;
            c.type = fractal::FractalType::Julia;
            c.julia_cre = v.rotate_radius * std::cos(theta);
            c.julia_cim = v.rotate_radius * std::sin(theta);
            break;
        }
        case fractal::AnimMode::Zoom: {
            // Exponential zoom from start scale to zoom_end, easing the center.
            const double f = (N > 1) ? static_cast<double>(frame) / (N - 1) : 0.0;
            c.scale = v.scale * std::pow(v.zoom_end / v.scale, f);
            c.center_x = v.center_x + (v.zoom_target_x - v.center_x) * f;
            c.center_y = v.center_y + (v.zoom_target_y - v.center_y) * f;
            break;
        }
        case fractal::AnimMode::Cycle: {
            // Sweep the palette one full cycle -> seamless loop.
            c.color_offset = v.color_offset + u;
            break;
        }
    }
    return c;
}

int runVideo(const fractal::VideoConfig& cfg) {
    // x264 + yuv420p needs even dimensions.
    fractal::VideoConfig v = cfg;
    if (v.width  % 2) { v.width--;  std::cerr << "note: width rounded to "  << v.width  << " (even required)\n"; }
    if (v.height % 2) { v.height--; std::cerr << "note: height rounded to " << v.height << " (even required)\n"; }

    fractal::Renderer r;
    std::string err;
    if (!r.init(v.width, v.height, v.ssaa, shaderDirFromEnv(), err)) {
        std::cerr << "error: " << err << "\n";
        return 1;
    }
    auto grad = fractal::buildGradient(v.palette, kGradientResolution, v.cyclic);
    r.setPalette(grad, kGradientResolution);

    const int N = v.total_frames();
    if (N < 1) { std::cerr << "error: nothing to render (duration/fps too small)\n"; return 1; }

    // Pipe raw RGB frames straight into ffmpeg.
    std::string cmd =
        "ffmpeg -y -hide_banner -loglevel error "
        "-f rawvideo -pixel_format rgb24 "
        "-video_size " + std::to_string(v.width) + "x" + std::to_string(v.height) + " "
        "-framerate " + std::to_string(v.fps) + " -i - "
        "-c:v libx264 -pix_fmt yuv420p -crf " + std::to_string(v.crf) + " "
        "-movflags +faststart \"" + v.output + "\"";

    FILE* pipe = popen(cmd.c_str(), "w");
    if (!pipe) { std::cerr << "error: failed to launch ffmpeg (is it installed?)\n"; return 1; }

    std::cout << "rendering " << N << " frames @ " << v.fps << " fps -> " << v.output << "\n";
    for (int frame = 0; frame < N; ++frame) {
        fractal::RenderConfig fc = frameConfig(v, frame);
        r.render(fc);
        auto pixels = r.readPixels();
        if (fwrite(pixels.data(), 1, pixels.size(), pipe) != pixels.size()) {
            std::cerr << "\nerror: ffmpeg pipe closed early (encode failed)\n";
            pclose(pipe);
            return 1;
        }
        if (frame % 10 == 0 || frame == N - 1) {
            std::printf("\r  frame %d/%d (%.0f%%)", frame + 1, N, 100.0 * (frame + 1) / N);
            std::fflush(stdout);
        }
    }
    std::printf("\n");

    int status = pclose(pipe);
    if (status != 0) {
        std::cerr << "error: ffmpeg exited with status " << status << "\n";
        return 1;
    }
    std::cout << "wrote " << v.output << " (" << v.width << "x" << v.height
              << ", " << v.duration << "s, ssaa " << v.ssaa << ")\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    fractal::ParsedArgs parsed = fractal::parseArgs(args);

    if (!parsed.error.empty()) {
        std::cerr << "error: " << parsed.error << "\n\n"
                  << "Run 'fractal help' for usage.\n";
        return 2;
    }
    switch (parsed.kind) {
        case fractal::CommandKind::Help:
            std::cout << fractal::helpText();
            return 0;
        case fractal::CommandKind::Render:
            return runRender(parsed.render);
        case fractal::CommandKind::Video:
            return runVideo(parsed.video);
        default:
            std::cout << fractal::helpText();
            return 0;
    }
}
