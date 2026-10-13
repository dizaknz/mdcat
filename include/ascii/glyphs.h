// Every character the ASCII renderer can draw, in one swappable table.
//
// Nothing else in the program decides between "Unicode" and "ASCII": callers hold a
// `const Glyphs&` and ask it for characters, so adding a third style (e.g. double lines)
// means adding one table here.
#pragma once

#include <cstdint>

namespace ascii {

// Direction bits for line cells; a cell's mask selects the junction glyph.
enum : uint8_t { kUp = 1, kDown = 2, kLeft = 4, kRight = 8 };

struct Glyphs {
    char32_t hline, vline;
    char32_t heavyHline; // under level-1 headings
    char32_t dashedH;    // 0 = alternate hline/space (used when no dashed glyph exists)
    char32_t dottedH;
    char32_t roundTL, roundTR, roundBL, roundBR;
    char32_t arrowUp, arrowDown, arrowLeft, arrowRight, crossHead;
    char32_t barFull, barEmpty;
    char32_t startMark, endMark;
    char32_t bullets[3];
    char32_t junctions[16]; // indexed by direction mask

    char32_t junction(uint8_t mask) const { return junctions[mask & 15]; }

    static const Glyphs &ascii();
    static const Glyphs &unicode();
    static const Glyphs &get(bool unicode) { return unicode ? Glyphs::unicode() : Glyphs::ascii(); }
};

} // namespace ascii
