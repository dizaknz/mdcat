# mdcat — Agent Guidelines

`mdcat` is a `cat` for Markdown: renders Markdown in the terminal with colours, wrapping, tables, lists, quotes, task lists, code blocks, and draws ```` ```mermaid ```` blocks as ASCII/Unicode art.

Parsed by [md4c](https://github.com/mity/md4c) (CommonMark + GitHub dialect). Built with CMake, C++17, single-header dependency-free modules.

---

## Project Layout

```
include/                          # Public headers — the API surface
├── text.h                        # UTF-8, display-width, ANSI-aware wrap
├── ascii/
│   ├── glyphs.h                  # Swappable character table (ASCII / Unicode)
│   └── canvas.h                  # 2D grid with auto-joining line primitives
├── md/
│   └── terminal_renderer.h       # Markdown → ANSI terminal text
└── mermaid/
    └── mermaid.h                 # Diagram source → ASCII rows (no ANSI)

src/                              # Implementations
├── text.cpp                      # UTF-8 helpers, displayWidth, wrap(), sanitize()
├── ascii/
│   ├── glyphs.cpp                # Glyphs::ascii() + Glyphs::unicode() tables
│   └── canvas.cpp                # Canvas: box(), line(), hline(), lifeline(), render()
├── md/
│   └── terminal_renderer.cpp     # md4c callbacks → styled output
└── mermaid/
    ├── mermaid.cpp               # Parser dispatch: flowchart / sequence / state / pie
    ├── diagrams.h                # Shared types: Node, Edge, Diagram variants
    ├── labels.cpp                # Multi-line label rendering
    ├── flowchart.cpp             # Layered layout + orthogonal edge routing
    ├── sequence.cpp              # Sequence diagram renderer
    └── pie.cpp                   # Pie chart renderer

app/
└── main.cpp                      # CLI: flags, tty detection, fence registration
```

---

## Architecture Overview

```
main.cpp (CLI)
    │
    ├── md::TerminalRenderer(opt)     ← Markdown parser + layout engine
    │       onFence("mermaid", ...)   ← routes fenced blocks by language
    │
    └── mermaid::render(code, opts)  ← diagram source → vector<string> (no ANSI)
            │
            ├── flowchart.cpp       ← Canvas primitives only
            ├── sequence.cpp        ← "draw, don't compute" philosophy
            ├── state.cpp           ← no raw cell access
            └── pie.cpp             ← all glyphs from Glyphs table
```

**Key principle: separation of concerns.** The Markdown renderer knows nothing about diagrams. Diagram renderers know nothing about ANSI colour — they produce plain text rows. `main.cpp` wires them together.

---

## Module Responsibilities

| Module | Owns | Must NOT |
|--------|------|----------|
| `text` | UTF-8 encoding/decoding, display width, word wrapping, sanitisation | Know about terminal output or Markdown syntax |
| `ascii/glyphs` | Every character the project can draw (one swappable table) | Hard-code `'-'`, `'+'`, etc. anywhere else |
| `ascii/canvas` | 2D grid, drawing primitives, auto-joining lines, final render() | Produce ANSI codes or know about Markdown |
| `md/terminal_renderer` | md4c callback impl, style stack, scope prefixes, table layout | Draw diagrams or parse Mermaid syntax |
| `mermaid/*` | Parsing diagram DSL → Canvas drawing | Emit ANSI codes or touch Markdown parsing |
| `main` | CLI flags, tty detection, file I/O, fence registration | Contain rendering logic |

---

## Coding Conventions

### Style

- **Indent:** 4 spaces (see `.clang-format`)
- **Namespace:** All code in a named namespace (`text::`, `ascii::`, `md::`, `mermaid::`). Implementation details go in an anonymous `namespace` inside the `.cpp` file.
- **Naming:** `snake_case` for functions/variables, `PascalCase` for types, `kPrefix` for constants.
- **Headers:** `#pragma once`. Include guards not needed. Forward-declare when possible; include only what's used.
- **Includes order:** 1) header being implemented, 2) project headers, 3) third-party, 4) C/C++ standard library.

### Memory & Ownership

- **No raw pointers.** Use `std::unique_ptr`, `std::shared_ptr`, or stack objects.
- **`const&` for reads.** Pass large structs by `const&`. Pass small structs (`Glyphs` is ~60 bytes) by `const&` too — callers hold a reference, not a copy.
- **Static locals for singleton tables.** `Glyphs::ascii()` and `Glyphs::unicode()` use static local variables (Meyers' singleton).

### String Handling

- **`std::string_view`** for read-only parameters (especially in public APIs).
- **UTF-8 throughout.** Internally store everything as UTF-8 `std::string`. Use `char32_t` only for individual codepoint manipulation.
- **Never trust input.** Always call `text::sanitize()` on untrusted text before it reaches the terminal.

### Error Handling

- **Return codes / `std::nullopt`** for recoverable failures (e.g. unsupported Mermaid diagram type).
- **Exit with code 1** for I/O errors in `main.cpp`. Code 2 for usage errors.
- **No exceptions.** This project avoids `try`/`catch` entirely.

### Platform Compatibility

- Windows and POSIX supported. Use `#ifdef _WIN32` guards sparingly — isolate platform code in small functions (`outIsTty()`, `terminalWidth()`).
- On Windows: `SetConsoleOutputCP(CP_UTF8)` and `ENABLE_VIRTUAL_TERMINAL_PROCESSING` for proper UTF-8 + ANSI output.

---

## Adding a New Mermaid Diagram Type

1. **Add types** to `include/mermaid/diagrams.h` (new `struct` variants, enums).
2. **Add parser** in `src/mermaid/mermaid.cpp` — dispatch on the diagram keyword.
3. **Implement renderer** as a new `src/mermaid/<type>.cpp` — draw **only via Canvas primitives** (`box()`, `line()`, `text()`, `arrowHead()`). Never touch raw cells or hard-code glyphs.
4. **Register in CMakeLists.txt** if it's a new file.

The renderer must return `std::optional<std::vector<std::string>>` — rows of plain text, no ANSI codes.

---

## Adding a New Code Block Language Handler

```cpp
renderer.onFence("graphviz", [](const std::string& code, int width) {
    auto rows = graphviz::render(code, width);  // your implementation
    return rows.empty() ? std::optional<std::vector<std::string>>{} : rows;
});
```

Return `std::nullopt` to decline — the block falls back to an ordinary code box.

---

## Adding a New Glyph Style

1. Add fields to `struct Glyphs` in `include/ascii/glyphs.h`.
2. Populate them in both `Glyphs::ascii()` and `Glyphs::unicode()` in `src/ascii/glyphs.cpp`.
3. Callers hold `const Glyphs&` — no other changes needed.

---

## Table & Code Box Border Rules (from experience)

- **Total width = `rule`** for every border line (top, bottom). Account for corner characters in your repeat count.
- **Top border:** `┌─ label ─┐` → left corner + hline + space + label + space + dashes + right corner. Dash count = `rule - 1 - displayWidth(head)`.
- **Bottom border:** `└─┘` → left corner + `(rule - 2)` dashes + right corner.
- **Body lines:** pad to `widest`, close with ` │`.
- **Empty label:** use full dash line `┌──────┐` (no `─ ` prefix).

---

## Build

```bash
cmake -S . -B build && cmake --build build
./build/mdcat test/demo.md
```

md4c is fetched automatically via CMake `FetchContent`. For offline builds, place `md4c.c` and `md4c.h` in `third_party/md4c/`.

---

## Testing / Visual Validation

Since this is a visual terminal renderer, **always verify output** by running:

```bash
./build/mdcat test/demo.md | cat -v    # show control chars
./build/mdcat -a test/demo.md          # ASCII mode
./build/mdcat -C test/demo.md          # no colour
./build/mdcat -w 40 test/demo.md       # narrow width (tests wrapping)
```

Pay special attention to:
- Table borders closing correctly at all corners
- Code box frames with and without language labels
- Nested quotes inside list items (prefix stacking)
- Multi-byte UTF-8 in wrapped lines
- ANSI escape sequences properly closed at line boundaries
