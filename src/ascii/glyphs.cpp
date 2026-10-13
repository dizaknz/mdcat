#include "ascii/glyphs.h"

namespace ascii {

const Glyphs &Glyphs::ascii() {
    static const Glyphs g = [] {
        Glyphs g{};
        g.hline = U'-';
        g.vline = U'|';
        g.heavyHline = U'=';
        g.dashedH = 0;
        g.dottedH = U'.';
        g.roundTL = U'.';
        g.roundTR = U'.';
        g.roundBL = U'\'';
        g.roundBR = U'\'';
        g.arrowUp = U'^';
        g.arrowDown = U'v';
        g.arrowLeft = U'<';
        g.arrowRight = U'>';
        g.crossHead = U'x';
        g.barFull = U'#';
        g.barEmpty = U'.';
        g.startMark = U'o';
        g.endMark = U'@';
        g.bullets[0] = U'*';
        g.bullets[1] = U'-';
        g.bullets[2] = U'+';
        for (int m = 1; m < 16; m++) {
            bool v = m & (kUp | kDown), h = m & (kLeft | kRight);
            g.junctions[m] = (v && h) ? U'+' : (v ? U'|' : U'-');
        }
        g.junctions[0] = U' ';
        return g;
    }();
    return g;
}

const Glyphs &Glyphs::unicode() {
    static const Glyphs g = [] {
        Glyphs g{};
        g.hline = U'\u2500';
        g.vline = U'\u2502';
        g.heavyHline = U'\u2550';
        g.dashedH = U'\u254C';
        g.dottedH = U'\u254C';
        g.roundTL = U'\u256D';
        g.roundTR = U'\u256E';
        g.roundBL = U'\u2570';
        g.roundBR = U'\u256F';
        g.arrowUp = U'\u25B2';
        g.arrowDown = U'\u25BC';
        g.arrowLeft = U'\u25C0';
        g.arrowRight = U'\u25B6';
        g.crossHead = U'\u2715';
        g.barFull = U'\u2588';
        g.barEmpty = U'\u2591';
        g.startMark = U'\u25CF';
        g.endMark = U'\u25C9';
        g.bullets[0] = U'\u2022';
        g.bullets[1] = U'\u25E6';
        g.bullets[2] = U'\u25AA';
        const char32_t t[16] = {U' ',      U'\u2502', U'\u2502', U'\u2502', U'\u2500', U'\u2518', U'\u2510', U'\u2524',
                                U'\u2500', U'\u2514', U'\u250C', U'\u251C', U'\u2500', U'\u2534', U'\u252C', U'\u253C'};
        for (int i = 0; i < 16; i++)
            g.junctions[i] = t[i];
        return g;
    }();
    return g;
}

} // namespace ascii
