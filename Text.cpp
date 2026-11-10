#include "Text.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>

namespace {
int regularWeightRank(const char* styleName) {
    if (!styleName) return 100;
    std::string style(styleName);
    std::transform(style.begin(), style.end(), style.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (style.find("regular") != std::string::npos) return 0;
    if (style.find("semibold") != std::string::npos ||
        style.find("semi bold") != std::string::npos) return 1;
    return 100;
}
}

bool FontAtlas::load(const std::string& fontPath, unsigned int pixelHeight) {
    m_width = 0;
    m_height = 0;
    m_lineHeight = 0;
    m_styleName.clear();
    m_glyphs.clear();
    m_pixels.clear();

    if (pixelHeight == 0) {
        std::cerr << "Text: font pixel height must be greater than zero\n";
        return false;
    }

    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0) {
        std::cerr << "Text: FreeType initialization failed\n";
        return false;
    }

    FT_Long faceCount = 1;
    FT_Face probe = nullptr;
    if (FT_New_Face(library, fontPath.c_str(), -1, &probe) == 0) {
        faceCount = std::max<FT_Long>(probe->num_faces, 1);
        FT_Done_Face(probe);
    }

    FT_Face face = nullptr;
    int bestWeightRank = 100;
    for (FT_Long faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
        FT_Face candidate = nullptr;
        if (FT_New_Face(library, fontPath.c_str(), faceIndex, &candidate) != 0)
            continue;
        const int rank = regularWeightRank(candidate->style_name);
        if (rank < bestWeightRank) {
            if (face) FT_Done_Face(face);
            face = candidate;
            bestWeightRank = rank;
        } else {
            FT_Done_Face(candidate);
        }
        if (bestWeightRank == 0) break; // exact Regular face is preferred
    }

    if (!face || bestWeightRank >= 100) {
        std::cerr << "Text: no Regular or SemiBold face found in font: "
                  << fontPath << '\n';
        if (face) FT_Done_Face(face);
        FT_Done_FreeType(library);
        return false;
    }
    m_styleName = face->style_name ? face->style_name : "Regular";

    if (FT_Set_Pixel_Sizes(face, 0, pixelHeight) != 0) {
        std::cerr << "Text: could not set font pixel size to " << pixelHeight << '\n';
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return false;
    }

    struct RasterizedGlyph {
        unsigned char character = 0;
        FontGlyph metrics;
        std::vector<unsigned char> coverage;
    };
    std::vector<RasterizedGlyph> rasterized;
    rasterized.reserve(static_cast<size_t>(LAST_CHARACTER - FIRST_CHARACTER + 1));

    bool rasterizationFailed = false;
    for (unsigned int code = FIRST_CHARACTER; code <= LAST_CHARACTER; ++code) {
        if (FT_Load_Char(face, code, FT_LOAD_RENDER) != 0) {
            std::cerr << "Text: failed to rasterize ASCII glyph " << code << '\n';
            rasterizationFailed = true;
            break;
        }

        const FT_Bitmap& bitmap = face->glyph->bitmap;
        if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY &&
            bitmap.pixel_mode != FT_PIXEL_MODE_MONO) {
            std::cerr << "Text: unsupported FreeType bitmap mode for glyph "
                      << code << '\n';
            rasterizationFailed = true;
            break;
        }

        RasterizedGlyph glyph;
        glyph.character = static_cast<unsigned char>(code);
        glyph.metrics.layer = code - FIRST_CHARACTER;
        glyph.metrics.width = static_cast<int>(bitmap.width);
        glyph.metrics.height = static_cast<int>(bitmap.rows);
        glyph.metrics.bearingX = face->glyph->bitmap_left;
        glyph.metrics.bearingY = face->glyph->bitmap_top;
        glyph.metrics.advance = static_cast<float>(face->glyph->advance.x) / 64.0f;

        const size_t pixelCount = static_cast<size_t>(bitmap.width) * bitmap.rows;
        if (bitmap.rows != 0 && pixelCount / bitmap.rows != bitmap.width) {
            std::cerr << "Text: glyph bitmap size overflow\n";
            rasterizationFailed = true;
            break;
        }
        glyph.coverage.resize(pixelCount, 0);

        const int pitch = bitmap.pitch;
        const size_t absolutePitch = static_cast<size_t>(pitch < 0 ? -pitch : pitch);
        for (unsigned int y = 0; y < bitmap.rows; ++y) {
            const unsigned int sourceY = pitch < 0 ? bitmap.rows - 1 - y : y;
            const unsigned char* source = bitmap.buffer + sourceY * absolutePitch;
            for (unsigned int x = 0; x < bitmap.width; ++x) {
                unsigned char value = 0;
                if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
                    value = source[x];
                    if (bitmap.num_grays > 1 && bitmap.num_grays != 256) {
                        value = static_cast<unsigned char>(
                            (static_cast<unsigned int>(value) * 255u) /
                            (bitmap.num_grays - 1));
                    }
                } else {
                    value = (source[x >> 3] & (0x80u >> (x & 7u))) ? 255 : 0;
                }
                glyph.coverage[static_cast<size_t>(y) * bitmap.width + x] = value;
            }
        }

        m_width = std::max(m_width, glyph.metrics.width);
        m_height = std::max(m_height, glyph.metrics.height);
        rasterized.push_back(std::move(glyph));
    }

    m_lineHeight = static_cast<int>(face->size->metrics.height >> 6);
    if (m_lineHeight <= 0) m_lineHeight = static_cast<int>(pixelHeight);
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    if (rasterizationFailed || rasterized.empty()) return false;

    // Space and a few punctuation glyphs may have no bitmap; layers still need
    // non-zero dimensions, so use a one-texel minimum cell.
    m_width = std::max(m_width, 1);
    m_height = std::max(m_height, 1);
    const size_t layerPixels = static_cast<size_t>(m_width) * m_height;
    if (layerPixels > std::numeric_limits<size_t>::max() / rasterized.size()) {
        std::cerr << "Text: glyph array dimensions overflow\n";
        return false;
    }
    m_pixels.assign(layerPixels * rasterized.size(), 0);

    for (const RasterizedGlyph& glyph : rasterized) {
        m_glyphs.emplace(glyph.character, glyph.metrics);
        if (glyph.metrics.width <= 0 || glyph.metrics.height <= 0) continue;
        const size_t layerOffset = static_cast<size_t>(glyph.metrics.layer) * layerPixels;
        for (int y = 0; y < glyph.metrics.height; ++y) {
            const size_t sourceOffset = static_cast<size_t>(y) * glyph.metrics.width;
            const size_t destinationOffset = layerOffset + static_cast<size_t>(y) * m_width;
            std::copy_n(glyph.coverage.data() + sourceOffset, glyph.metrics.width,
                        m_pixels.data() + destinationOffset);
        }
    }

    std::cout << "Text: rasterized " << m_glyphs.size() << " " << m_styleName
              << " glyphs from " << fontPath << " at " << pixelHeight
              << " px (array " << m_width
              << "x" << m_height << " per layer)\n";
    return true;
}

const FontGlyph* FontAtlas::glyph(unsigned char character) const {
    if (character < FIRST_CHARACTER || character > LAST_CHARACTER)
        character = '?';
    const auto it = m_glyphs.find(character);
    return it == m_glyphs.end() ? nullptr : &it->second;
}
