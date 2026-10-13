# Mermaid → ASCII Art Renderer

Pure C++ renderer that converts [Mermaid](https://mermaid.js.org/) diagram source into terminal-friendly text art. No external dependencies — no Node.js, no browser, no graphviz.

## Supported Diagrams

| Type | Syntax Example |
|------|---------------|
| **Flowchart / Graph** | `graph TD; A --> B;` |
| **Flowchart directions** | `flowchart LR`, `flowchart BT`, `flowchart RL` |
| **State Diagram** | `stateDiagram-v2; [*] --> Ready;` |
| **Sequence Diagram** | `sequenceDiagram; Alice->>Bob: Hello;` |
| **Pie Chart** | `pie showData; "Dogs" : 386; "Cats" : 85;` |

### Flowchart Shapes

| Syntax | Shape |
|--------|-------|
| `[text]` | Rectangle |
| `(text)` | Rounded |
| `[(text)]` | Stadium / Pill |
| `((text))` | Circle |
| `{text}` | Diamond |
| `{{text}}` | Hexagon |
| `[[text]]` | Subroutine |
| `[/(text/)]` | Cylinder |
| `>[text]` | Asymmetric |

### Edge Styles

`-->`, `-->>`, `-x`, `->>`, `--x`, `=>>`, `===`, `-.->`, etc. with optional labels: `A -->|message| B`.

## API

```cpp
#include "mermaid/mermaid.h"

using namespace mermaid;

Options opt;
opt.maxWidth = 120;   // auto-try alternate orientation if wider
opt.unicode = true;    // box-drawing glyphs (true) or plain ASCII (false)

auto result = render("graph TD;\n  A --> B;", opt);
if (result) {
    for (auto& line : *result)
        std::cout << line << '\n';
}
```

### Options

| Field | Default | Description |
|-------|---------|-------------|
| `maxWidth` | `80` | Width threshold — if a TD/TB flowchart exceeds this, the renderer tries LR/BT and picks whichever fits (or is narrower) |
| `unicode` | `true` | Use Unicode box-drawing glyphs (`╭`, `─`, `├`, etc.) vs plain ASCII (`+`, `-`, `|`) |

### Supported Options

- `glyphs()` returns a reference to the active `ascii::Glyphs` table, which controls characters used for boxes, arrows, arrowheads, pie bars, and start/end marks.

## Internals

```
mermaid.h          ← public API: render(source, options)
diagrams.h         ← detail namespace: per-family renderers + labelLines()

flowchart.cpp      ← parser → cycle-breaking → layered layout → barycenter ordering → orthogonal routing
sequence.cpp       ← participants → lifelines → messages / notes / alt-opt-loop blocks
pie.cpp            ← horizontal bar chart from key-value pairs
labels.cpp         ← <br>, HTML entities, quote stripping for node/edge labels
```

### Flowchart Layout Pipeline

1. **Parse** — extract nodes (id, shape, label) and edges (from, to, label, arrow type)
2. **Cycle breaking** — DFS detects back-edges; reverses them for an acyclic DAG
3. **Layering** — longest-path assignment places each node into a layer
4. **Dummy nodes** — placeholder nodes inserted for edges that skip layers
5. **Barycenter sweep** — 4 forward/backward passes minimize edge crossings per layer
6. **Centering** — 8-iteration coordinate adjustment aligns parents with children
7. **Port assignment** — input/output ports distributed evenly on each node's boundary
8. **Channel tracks** — vertical lanes between layers so overlapping labels don't collide
9. **Render** — `ascii::Canvas` draws boxes, lines, arrows, and labels

### Sequence Diagram Layout Pipeline

1. **Parse participants** — assign IDs and labels
2. **Compute widths** — widest participant + message text determines inter-participant spacing
3. **Satisfy requirements** — long messages/notes expand gaps greedily (sorted by span)
4. **Draw** — header row → events (messages, notes, alt/opt blocks) → footer row with lifelines

## Dependencies

- `ascii/canvas.h` — low-level drawing primitives (`box`, `line`, `hline`, `arrowHead`, `crossHead`, `lifeline`, `text`)
- `ascii/glyphs.h` — character tables for Unicode vs ASCII rendering
- `text.h` — utilities (`trim`, `displayWidth`, `repeat`, `toLower`, `utf8`)
