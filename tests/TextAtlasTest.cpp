#include "Text.hpp"

#include <cassert>
#include <iostream>

int main() {
    FontAtlas atlas;
    if (!atlas.load(TEST_FONT_PATH, 48)) {
        std::cerr << "Could not rasterize the bundled Lyra font\n";
        return 1;
    }

    assert(atlas.layerCount() == 95);
    assert(atlas.width() > 0);
    assert(atlas.height() > 0);
    assert(atlas.lineHeight() > 0);
    assert(atlas.styleName().find("Light") == std::string::npos);
    assert(atlas.styleName() == "Regular" ||
           atlas.styleName() == "SemiBold" ||
           atlas.styleName() == "Semibold");
    assert(atlas.pixels().size() ==
           static_cast<size_t>(atlas.width()) * atlas.height() * atlas.layerCount());

    const FontGlyph* letterA = atlas.glyph('A');
    const FontGlyph* space = atlas.glyph(' ');
    const FontGlyph* fallback = atlas.glyph(0xff);
    const FontGlyph* questionMark = atlas.glyph('?');
    assert(letterA && letterA->width > 0 && letterA->height > 0);
    assert(letterA->advance > 0.0f);
    assert(space && space->width == 0 && space->height == 0);
    assert(space->advance > 0.0f);
    assert(fallback && questionMark && fallback->layer == questionMark->layer);

    bool hasCoverage = false;
    const size_t layerOffset = static_cast<size_t>(letterA->layer) *
                               atlas.width() * atlas.height();
    for (int y = 0; y < letterA->height; ++y) {
        for (int x = 0; x < letterA->width; ++x) {
            hasCoverage |= atlas.pixels()[layerOffset +
                static_cast<size_t>(y) * atlas.width() + x] != 0;
        }
    }
    assert(hasCoverage);
    std::cout << "Text atlas smoke test passed\n";
    return 0;
}
