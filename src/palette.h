// palette.h — color gradient parsing and 1-D gradient texture generation.
//
// No OpenGL dependency: building the gradient produces a flat RGB byte buffer
// that the renderer uploads as a texture. This keeps all color logic unit
// testable.
#pragma once

#include "config.h"

#include <string>
#include <vector>
#include <cstdint>

namespace fractal {

// Parse a single hex color: "#rrggbb", "rrggbb", "#rgb", or "rgb".
// Returns false on malformed input.
bool parseHexColor(const std::string& s, Color& out);

// Resolve a palette specification into a list of gradient stops.
// `spec` is either a named palette ("aurora", "ember", "ice", "psychedelic",
// "fire", "mono") or a comma-separated list of hex colors
// (e.g. "#000000,#ff0080,#ffd000"). Returns false if the spec is unknown or
// any color fails to parse.
bool parsePalette(const std::string& spec, std::vector<Color>& out);

// Names of the built-in palettes, for help text and tests.
std::vector<std::string> builtinPaletteNames();

// Build a horizontal RGB gradient by linearly interpolating between stops.
// `resolution` is the number of texels. When `cyclic` is true the gradient
// wraps from the last stop back to the first so it tiles seamlessly (used for
// color-cycling animations). The result is `resolution * 3` bytes, RGB8.
std::vector<uint8_t> buildGradient(const std::vector<Color>& stops,
                                   int resolution, bool cyclic);

} // namespace fractal
