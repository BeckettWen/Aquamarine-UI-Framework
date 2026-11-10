#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Font glyph rasterized to grayscale coverage. layer is stable within the
// font's dedicated Vulkan glyph texture array.
struct FontGlyph {
    uint32_t layer = 0;
    int width = 0;
    int height = 0;
    int bearingX = 0;
    int bearingY = 0;
    float advance = 0.0f; // pixels at the font's rasterization size
};

// CPU-side rasterized font atlas. Every supported glyph occupies one layer in
// a separate 2D array texture; each layer is padded to the same dimensions.
class FontAtlas {
public:
    bool load(const std::string& fontPath, unsigned int pixelHeight = 48);

    const FontGlyph* glyph(unsigned char character) const;
    const std::vector<unsigned char>& pixels() const { return m_pixels; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    int lineHeight() const { return m_lineHeight; }
    const std::string& styleName() const { return m_styleName; }
    uint32_t layerCount() const { return static_cast<uint32_t>(m_glyphs.size()); }
    bool valid() const { return !m_pixels.empty() && !m_glyphs.empty(); }

private:
    static constexpr unsigned char FIRST_CHARACTER = 32;
    static constexpr unsigned char LAST_CHARACTER = 126;

    int m_width = 0;
    int m_height = 0;
    int m_lineHeight = 0;
    std::string m_styleName;
    std::map<unsigned char, FontGlyph> m_glyphs;
    std::vector<unsigned char> m_pixels;
};

// Text label position is the baseline origin in logical window pixels, with
// (0,0) at the top-left and positive y downward. Scale 1.0 means the font's
// rasterization size (48 px by default). Only printable ASCII, newline, and
// tab are supported initially; unsupported bytes render as '?'.
struct TextLabel {
    std::string text;
    float x = 0.0f;
    float baselineY = 0.0f;
    float scale = 1.0f;
    std::array<float, 4> color = {1.0f, 1.0f, 1.0f, 1.0f};
    bool visible = true;
};
