# ascii — Glyphs & Canvas

A lightweight 2D ASCII-art subsystem used by the Mermaid renderer (and any future diagram backends). Two cooperating pieces:

| Component | Role |
|-----------|------|
| [`Glyphs`](glyphs.h) | Swappable character table — every symbol the renderer can draw. |
| [`Canvas`](canvas.h) | Off-screen grid with drawing primitives; auto-joins lines at intersections. |

## How They Work Together

```
Caller (e.g. Mermaid renderer)
    │
    ├── holds const Glyphs&  →  asks for characters (no hard-coded symbols)
    └── draws on Canvas      →  describes shapes, not raw cells
                                │
                                ▼
                         Canvas.merge() → junction mask selects glyph
                                │
                                ▼
                         Canvas.render() → vector<string> output
```

## Glyphs Table

Every character lives in one `Glyphs` struct. Callers never hard-code `'-'`, `'+'`, or `U'─'`; they always go through the table, so switching between ASCII and Unicode is a single constructor parameter.

| Field | ASCII | Unicode | Purpose |
|-------|-------|---------|---------|
| `hline` | `-` | `─` | Horizontal box edge / rule |
| `vline` | `\|` | `│` | Vertical box edge |
| `heavyHline` | `=` | `═` | H1/H2 heading underlines |
| `dashedH` | alt `-`/` ` | `╌` | Dashed horizontal stroke |
| `dottedH` | `.` | `⋯` | Dotted horizontal stroke |
| `roundTL..BR` | `. ' ` | `ⷭⷮⷰⷯ` | Rounded box corners |
| `arrowUp/Down/Left/Right` | `^ v < >` | `▲ ▼ ◀ ▶` | Directed edges |
| `crossHead` | `x` | `✕` | ER relationship symbol |
| `barFull / barEmpty` | `# .` | `█ ▓` | Progress / state bars |
| `startMark / endMark` | `o @` | `● ◉` | Flowchart terminals |
| `bullets[3]` | `* - +` | `● ○ ▋` | List item markers (cycled) |
| `junctions[16]` | `+ \| -` | `┌┐└┘├┤┬┴┼...` | 4-bit mask → corner/tee/cross |

### Junction Mask

`junctions[]` is indexed by a bitmask of directions. Any combination selects the correct character:

| Mask | Bits | ASCII | Unicode | Use |
|------|------|-------|---------|-----|
| `0001` | Up | `\|` | `│` | Line end |
| `0100` | Left | `-` | `─` | Line end |
| `0101` | Up+Left | `+` | `├` | T-junction (up) |
| `0110` | Up+Down | `\|` | `│` | Vertical pass-through |
| `1001` | Up+Right | `+` | `┤` | T-junction (up) |
| `1100` | Left+Right | `-` | `─` | Horizontal pass-through |
| `1101` | Up+Left+Right | `+` | `┬` | T-junction (down) |
| `1110` | Up+Down+Left | `+` | `├` | T-junction (right) |
| `1111` | All four | `+` | `┼` | Cross intersection |

## Canvas API

### Drawing Primitives

| Method | Description |
|--------|-------------|
| `text(x, y, utf8)` | Write UTF-8 string. Double-width chars occupy two cells. |
| `box(x, y, w, h, style, lines, align)` | Draw a bordered rectangle with centered/multi-aligned text inside. |
| `line(x0,y0,x1,y1)` | Bresenham orthogonal line. Sets direction bits for auto-joining. |
| `hline(x0,x1,y,stroke)` | Horizontal run (Solid / Dashed / Dotted). Does **not** join. |
| `lifeline(x,y0,y1)` | Vertical line that skips already-filled cells (sequence lifelines). |
| `arrowHead(x,y,heading)` | Place an arrowhead glyph at the given position/direction. |
| `crossHead(x,y)` | Place a cross/✕ symbol (ER relationships). |

### Output

| Method | Description |
|--------|-------------|
| `render()` | Assemble final rows: merges mask bits into junction glyphs, trims trailing spaces and empty leading/trailing rows. Returns `vector<string>`. |

### Usage Example

```cpp
Glyphs const& g = Glyphs::get(true);  // Unicode
Canvas canvas(g);

canvas.box(1, 1, 20, 5, BoxStyle::Square, {"Start", "Process"});
canvas.line(10, 3, 10, 7);
canvas.arrowHead(10, 8, Heading::Down);

auto rows = canvas.render();  // vector<string>, ready to emit
```

## Design Notes

- **No raw cell access** — `put()`, `link()`, and `isEmpty()` are private. Callers use the public primitives only.
- **Line joining via masks** — each cell stores both a character and a direction bitmask. Where two lines cross, the mask selects the correct junction glyph automatically.
- **`kSecondHalf` sentinel** — double-width characters fill the second cell with `0x7FFFFFFF` so `render()` skips it during output assembly.
- **Automatic trimming** — `render()` strips leading empty rows/columns and trailing spaces, so the output is tight around the drawn content.

## File Structure

```
include/ascii/
├── glyphs.h               # Glyph table + junction mask constants
├── canvas.h               # Canvas class declaration
└── README.md              # This file
src/ascii/
├── glyphs.cpp             # ASCII & Unicode table definitions
└── canvas.cpp             # Canvas implementation
```
