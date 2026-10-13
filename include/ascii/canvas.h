// An off-screen character grid with drawing primitives.
//
// The Canvas owns the grid and the line-joining logic. Callers describe *what* to draw
// (a box, a connector, an arrow head) and never touch raw cells or pick glyphs; every
// character comes from the injected Glyphs table.
//
// Line cells record which directions they connect to, so where two connectors cross or
// meet a box border the right junction glyph (+, ┬, ┼ ...) appears automatically.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "glyphs.h"

namespace ascii {

enum class BoxStyle { Square, Rounded, Diamond, Subroutine };
enum class Align { Center, Left };
enum class Heading { Up, Down, Left, Right }; // direction an arrow head points
enum class Stroke { Solid, Dashed, Dotted };

class Canvas {
  public:
    explicit Canvas(const Glyphs &glyphs) : g_(glyphs) {}

    const Glyphs &glyphs() const { return g_; }

    // Text (UTF-8). Wide characters occupy two cells.
    void text(int x, int y, const std::string &utf8);

    // Bordered box; interior is cleared and `lines` are drawn inside (centered vertically).
    void box(int x, int y, int w, int h, BoxStyle style, const std::vector<std::string> &lines, Align align = Align::Center);

    // Orthogonal connector between two points (inclusive); crossings/joins merge automatically.
    void line(int x0, int y0, int x1, int y1);

    // Plain horizontal run of a stroke style (does not join with other lines).
    void hline(int x0, int x1, int y, Stroke stroke = Stroke::Solid);

    // Vertical line that only fills cells nothing else has drawn on (e.g. sequence lifelines).
    void lifeline(int x, int y0, int y1);

    void arrowHead(int x, int y, Heading toward);
    void crossHead(int x, int y);

    // Rows with the common left margin and blank top/bottom rows removed.
    std::vector<std::string> render() const;

  private:
    void put(int x, int y, char32_t cp);
    void link(int x, int y, uint8_t bits);
    bool isEmpty(int x, int y) const;
    void grow(int x, int y);

    const Glyphs &g_;
    std::vector<std::u32string> ch_;         // 0 = no character; the mask decides the glyph
    std::vector<std::vector<uint8_t>> mask_; // direction bits per cell
};

} // namespace ascii
