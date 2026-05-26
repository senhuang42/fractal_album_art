#include "palette.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <sstream>

namespace fractal {

namespace {

int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = static_cast<char>(std::tolower(c));
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

// Built-in palettes. Each is designed to wrap cleanly (the cyclic flag closes
// the loop) and to evoke the reference art: dark voids, jewel tones, warm
// coral/gold highlights and cool teal accents.
const std::map<std::string, std::vector<std::string>>& namedPalettes() {
    static const std::map<std::string, std::vector<std::string>> kPalettes = {
        // The default: deep void -> violet -> pink -> coral -> gold -> teal.
        // Balanced across hues so spread color doesn't skew to one family —
        // matches the multi-hue spirals in the references.
        {"aurora",      {"#0a0420", "#7b2ff7", "#e84393", "#ff6b4a", "#ffd45e", "#2ad4c8"}},
        // Warm volcanic.
        {"ember",       {"#0a0200", "#3d0c02", "#9a1f0b", "#e8540c", "#ffae42", "#fff1c1"}},
        // Cool glacial blues and whites.
        {"ice",         {"#01030f", "#0a2a5e", "#1f6fb2", "#56c4e8", "#b8f0ff", "#ffffff"}},
        // Saturated rainbow on near-black.
        {"psychedelic", {"#000010", "#ff006e", "#fb5607", "#ffbe0b", "#8ac926", "#3a86ff", "#8338ec"}},
        // Classic flame.
        {"fire",        {"#000000", "#420a00", "#a01a00", "#ff4d00", "#ffb000", "#ffffff"}},
        // Soft pastel pinks/golds on black (matches reference image 2).
        {"bloom",       {"#000000", "#5e2750", "#c8638f", "#f6b8c4", "#f0d8a8", "#cf9b6b", "#dff0d0"}},
        // Monochrome grayscale.
        {"mono",        {"#000000", "#ffffff"}},
    };
    return kPalettes;
}

} // namespace

bool parseHexColor(const std::string& in, Color& out) {
    std::string s = in;
    if (!s.empty() && s[0] == '#') s = s.substr(1);

    auto fromByte = [](int v) { return static_cast<float>(v) / 255.0f; };

    if (s.size() == 6) {
        int r1 = hexNibble(s[0]), r0 = hexNibble(s[1]);
        int g1 = hexNibble(s[2]), g0 = hexNibble(s[3]);
        int b1 = hexNibble(s[4]), b0 = hexNibble(s[5]);
        if ((r1|r0|g1|g0|b1|b0) < 0) return false;
        out = Color{fromByte(r1*16+r0), fromByte(g1*16+g0), fromByte(b1*16+b0)};
        return true;
    }
    if (s.size() == 3) {
        int r = hexNibble(s[0]), g = hexNibble(s[1]), b = hexNibble(s[2]);
        if ((r|g|b) < 0) return false;
        out = Color{fromByte(r*17), fromByte(g*17), fromByte(b*17)};
        return true;
    }
    return false;
}

bool parsePalette(const std::string& spec, std::vector<Color>& out) {
    out.clear();

    // Named palette?
    const auto& named = namedPalettes();
    auto it = named.find(spec);
    if (it != named.end()) {
        for (const auto& hex : it->second) {
            Color c;
            if (!parseHexColor(hex, c)) return false; // built-ins are always valid
            out.push_back(c);
        }
        return true;
    }

    // Otherwise treat as comma-separated hex list.
    std::stringstream ss(spec);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // Trim whitespace.
        size_t a = token.find_first_not_of(" \t");
        size_t b = token.find_last_not_of(" \t");
        if (a == std::string::npos) continue;
        token = token.substr(a, b - a + 1);
        Color c;
        if (!parseHexColor(token, c)) return false;
        out.push_back(c);
    }
    return out.size() >= 2; // need at least two stops to form a gradient
}

std::vector<std::string> builtinPaletteNames() {
    std::vector<std::string> names;
    for (const auto& kv : namedPalettes()) names.push_back(kv.first);
    return names;
}

std::vector<uint8_t> buildGradient(const std::vector<Color>& stops,
                                   int resolution, bool cyclic) {
    std::vector<uint8_t> data;
    if (stops.empty() || resolution <= 0) return data;

    // Working list of stops; when cyclic, append the first stop to close it.
    std::vector<Color> s = stops;
    if (cyclic && s.size() >= 1) s.push_back(s.front());

    data.resize(static_cast<size_t>(resolution) * 3);

    const int segments = static_cast<int>(s.size()) - 1;
    if (segments <= 0) {
        // Single color fill.
        for (int i = 0; i < resolution; ++i) {
            data[i*3+0] = static_cast<uint8_t>(std::lround(std::clamp(s[0].r,0.0f,1.0f) * 255.0f));
            data[i*3+1] = static_cast<uint8_t>(std::lround(std::clamp(s[0].g,0.0f,1.0f) * 255.0f));
            data[i*3+2] = static_cast<uint8_t>(std::lround(std::clamp(s[0].b,0.0f,1.0f) * 255.0f));
        }
        return data;
    }

    for (int i = 0; i < resolution; ++i) {
        // Position in [0, segments].
        double t = (resolution == 1) ? 0.0
                 : (static_cast<double>(i) / (resolution - 1)) * segments;
        int idx = std::min(static_cast<int>(t), segments - 1);
        double f = t - idx;
        const Color& a = s[idx];
        const Color& b = s[idx + 1];
        double r = a.r + (b.r - a.r) * f;
        double g = a.g + (b.g - a.g) * f;
        double bl = a.b + (b.b - a.b) * f;
        data[i*3+0] = static_cast<uint8_t>(std::lround(std::clamp(r,  0.0, 1.0) * 255.0));
        data[i*3+1] = static_cast<uint8_t>(std::lround(std::clamp(g,  0.0, 1.0) * 255.0));
        data[i*3+2] = static_cast<uint8_t>(std::lround(std::clamp(bl, 0.0, 1.0) * 255.0));
    }
    return data;
}

} // namespace fractal
