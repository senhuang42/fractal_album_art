// cli.h — command-line argument parsing.
//
// Parsing is pure (no I/O, no OpenGL) so it can be unit tested by feeding in
// argument vectors and inspecting the resulting config.
#pragma once

#include "config.h"

#include <string>
#include <vector>

namespace fractal {

enum class CommandKind { None, Help, Render, Video };

struct ParsedArgs {
    CommandKind  kind = CommandKind::None;
    RenderConfig render; // valid when kind == Render
    VideoConfig  video;  // valid when kind == Video
    std::string  error;  // non-empty => parse failure (kind is unreliable)
};

// Parse argv (excluding program name). The first token selects the subcommand
// ("render" or "video"); "help"/"--help"/"-h" requests usage.
ParsedArgs parseArgs(const std::vector<std::string>& args);

// Full usage/help text.
std::string helpText();

} // namespace fractal
