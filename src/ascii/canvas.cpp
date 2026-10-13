#include "ascii/canvas.h"
#include "text.h"

#include <algorithm>


namespace ascii {

namespace {
const char32_t kSecondHalf = 0x7FFFFFFF; // trailing cell of a double-width glyph
}

void Canvas::grow(int x, int y) {
    if ((int)ch_.size() <= y) {
        ch_.resize(y + 1);
        mask_.resize(y + 1);
    }
    if ((int)ch_[y].size() <= x) {
        ch_[y].resize(x + 1, 0);
        mask_[y].resize(x + 1, 0);
    }
}

void Canvas::put(int x, int y, char32_t cp) {
    if (x < 0 || y < 0)
        return;
    grow(x, y);
    ch_[y][x] = cp;
}

void Canvas::link(int x, int y, uint8_t bits) {
    if (x < 0 || y < 0)
        return;
    grow(x, y);
    mask_[y][x] |= bits;
}

bool Canvas::isEmpty(int x, int y) const {
    if (x < 0 || y < 0 || y >= (int)ch_.size() || x >= (int)ch_[y].size())
        return true;
    return ch_[y][x] == 0 && mask_[y][x] == 0;
}

void Canvas::text(int x, int y, const std::string &s) {
    for (size_t i = 0; i < s.size();) {
        char32_t cp;
        i += text::decodeUtf8(s, i, cp);
        int w = text::codepointWidth(cp);
        if (w == 0)
            continue;
        put(x, y, cp);
        if (w == 2)
            put(x + 1, y, kSecondHalf);
        x += w;
    }
}

void Canvas::line(int x0, int y0, int x1, int y1) {
    int dx = (x1 > x0) - (x1 < x0), dy = (y1 > y0) - (y1 < y0);
    uint8_t fwd = dx > 0 ? kRight : dx < 0 ? kLeft : dy > 0 ? kDown : kUp;
    uint8_t bwd = dx > 0 ? kLeft : dx < 0 ? kRight : dy > 0 ? kUp : kDown;
    int x = x0, y = y0;
    while (x != x1 || y != y1) {
        link(x, y, fwd);
        x += dx;
        y += dy;
        link(x, y, bwd);
    }
}

void Canvas::hline(int x0, int x1, int y, Stroke stroke) {
    for (int x = x0; x <= x1; x++) {
        char32_t c = g_.hline;
        if (stroke == Stroke::Dashed)
            c = g_.dashedH ? g_.dashedH : ((x & 1) ? g_.hline : U' ');
        else if (stroke == Stroke::Dotted)
            c = g_.dottedH;
        put(x, y, c);
    }
}

void Canvas::lifeline(int x, int y0, int y1) {
    for (int y = y0; y <= y1; y++)
        if (isEmpty(x, y))
            put(x, y, g_.vline);
}

void Canvas::arrowHead(int x, int y, Heading toward) {
    switch (toward) {
    case Heading::Up:
        put(x, y, g_.arrowUp);
        break;
    case Heading::Down:
        put(x, y, g_.arrowDown);
        break;
    case Heading::Left:
        put(x, y, g_.arrowLeft);
        break;
    case Heading::Right:
        put(x, y, g_.arrowRight);
        break;
    }
}

void Canvas::crossHead(int x, int y) { put(x, y, g_.crossHead); }

void Canvas::box(int x, int y, int w, int h, BoxStyle style, const std::vector<std::string> &lines, Align align) {
    for (int yy = y + 1; yy < y + h - 1; yy++)
        for (int xx = x + 1; xx < x + w - 1; xx++)
            put(xx, yy, U' ');

    // Border as joinable line cells, so connectors that touch it produce ┬ ┴ ├ ┤ ...
    line(x, y, x + w - 1, y);
    line(x + w - 1, y, x + w - 1, y + h - 1);
    line(x + w - 1, y + h - 1, x, y + h - 1);
    line(x, y + h - 1, x, y);

    char32_t tl = 0, tr = 0, bl = 0, br = 0;
    if (style == BoxStyle::Rounded) {
        tl = g_.roundTL;
        tr = g_.roundTR;
        bl = g_.roundBL;
        br = g_.roundBR;
    } else if (style == BoxStyle::Diamond) {
        tl = U'/';
        tr = U'\\';
        bl = U'\\';
        br = U'/';
    }
    if (tl) {
        put(x, y, tl);
        put(x + w - 1, y, tr);
        put(x, y + h - 1, bl);
        put(x + w - 1, y + h - 1, br);
    }
    if (style == BoxStyle::Subroutine)
        for (int yy = y + 1; yy < y + h - 1; yy++) {
            put(x + 1, yy, g_.vline);
            put(x + w - 2, yy, g_.vline);
        }

    int n = (int)lines.size();
    int top = y + 1 + (h - 2 - n) / 2;
    for (int k = 0; k < n; k++) {
        int lw = text::displayWidth(lines[k]);
        int tx = align == Align::Left ? x + 2 : x + 1 + (w - 2 - lw) / 2;
        text(tx, top + k, lines[k]);
    }
}

std::vector<std::string> Canvas::render() const {
    std::vector<std::u32string> rows;
    for (size_t y = 0; y < ch_.size(); y++) {
        std::u32string r;
        for (size_t x = 0; x < ch_[y].size(); x++) {
            char32_t c = ch_[y][x];
            if (c == kSecondHalf)
                continue;
            if (!c)
                c = g_.junction(mask_[y][x]);
            r += c;
        }
        while (!r.empty() && r.back() == U' ')
            r.pop_back();
        rows.push_back(r);
    }
    size_t lead = std::u32string::npos;
    for (auto &r : rows)
        if (!r.empty()) {
            size_t k = 0;
            while (k < r.size() && r[k] == U' ')
                k++;
            lead = std::min(lead, k);
        }
    std::vector<std::string> out;
    if (lead == std::u32string::npos)
        return out;
    size_t a = 0, b = rows.size();
    while (a < b && rows[a].empty())
        a++;
    while (b > a && rows[b - 1].empty())
        b--;
    for (size_t y = a; y < b; y++)
        out.push_back(rows[y].empty() ? std::string() : text::fromU32(rows[y].substr(lead)));
    return out;
}

} // namespace ascii
