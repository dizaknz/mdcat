# mdcat

`cat` for Markdown: renders Markdown in the terminal (colours, wrapping, tables, lists, quotes,
task lists, code blocks) and draws ```` ```mermaid ```` blocks as ASCII / Unicode art.

Markdown is parsed by [md4c](https://github.com/mity/md4c) (CommonMark + GitHub tables,
strikethrough, task lists, autolinks).

## Build

    cmake -S . -B build && cmake --build build
    ./build/mdcat demo.md

md4c is fetched with CMake `FetchContent`. For an offline build copy `md4c.c` and `md4c.h`
from the md4c repository's `src/` into `third_party/md4c/` and it is picked up automatically.
Requires a C++17 compiler (GCC, Clang, MSVC).

## Usage

    mdcat README.md
    mdcat -a notes.md          # plain ASCII instead of Unicode box drawing
    mdcat -w 100 a.md b.md     # wrap at 100 columns
    cat doc.md | mdcat -       # stdin
    mdcat flow.mmd             # a bare Mermaid file

Flags: `-w N`, `-a`, `-c/-C` (force / disable colour), `-m` (don't render mermaid), `-u` (hide URLs).

## Layout

    src/text.*                  UTF-8, display width, ANSI-aware word wrap, input sanitising
    src/ascii/glyphs.*          every glyph in one table (ASCII / Unicode)
    src/ascii/canvas.*          Canvas: box(), line(), hline(), arrowHead() ... joins lines itself
    src/mermaid/mermaid.hpp     mermaid::render(source, options) -> rows
    src/mermaid/*.cpp           flowchart, state, sequence, pie renderers (draw only via Canvas)
    src/md/terminal_renderer.*  md4c callbacks -> terminal layout
    src/main.cpp                CLI wiring

## How Mermaid is routed

`TerminalRenderer` knows nothing about diagrams. It exposes `onFence(language, handler)`;
`main.cpp` registers one handler for `mermaid` that calls `mermaid::render`. A handler may return
`std::nullopt` to decline, in which case the block is shown as ordinary code. Register more
handlers the same way (e.g. a Graphviz or PlantUML converter).

## Mermaid support

flowchart / graph (TD TB BT LR RL), sequenceDiagram, stateDiagram(-v2), pie. Wide flowcharts
switch orientation if the other one fits the terminal width.

## Safety

Text from the document is stripped of ESC and other control bytes before it reaches the terminal.
