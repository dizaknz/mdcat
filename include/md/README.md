# TerminalRenderer

**Markdown → styled terminal text**, driven by [md4c](https://github.com/mity/md4c) callbacks.

## Overview

`TerminalRenderer` parses Markdown and produces ANSI-colored, wrapped terminal output with proper formatting — list/quote prefixes, box-drawing tables, headings with underlines, and more.

Fenced code blocks are routed by language to registered `FenceHandler` callbacks, allowing custom renderers (e.g. Mermaid → ASCII) without the core class knowing about them:

```cpp
TerminalRenderer renderer({.width = 120});
renderer.onFence("mermaid", [](const std::string& code, int width) {
    return mermaid::render(code, {width, true});
});
std::string output = renderer.render(markdown);
```

## API

### `RenderOptions`

| Field | Default | Description |
|-------|---------|-------------|
| `width` | `80` | Terminal column width for wrapping. |
| `color` | `true` | Emit ANSI SGR escape codes (`\x1b[...m`). Set `false` for plain-text markers like `**bold**`. |
| `unicode` | `true` | Use Unicode box-drawing / bullet characters (`─ │ ●`) vs plain ASCII (`- | *`). |
| `showUrls` | `true` | Print `(url)` after link text. |

### `FenceHandler`

```cpp
using FenceHandler = std::function<
    std::optional<std::vector<std::string>>(const std::string& code, int availableWidth)
>;
```

Called for fenced code blocks whose info string matches the registered language (case-insensitive). Return `std::nullopt` to decline — the block falls back to an ordinary code box.

### `TerminalRenderer`

| Method | Description |
|--------|-------------|
| `explicit TerminalRenderer(RenderOptions options)` | Construct with rendering options. |
| `~TerminalRenderer()` | Destructor. |
| `void onFence(std::string language, FenceHandler handler)` | Register a custom code-block renderer for `language`. |
| `std::string render(std::view markdown)` | Render a complete Markdown document. Result ends with `\n` unless empty. |

## Supported Markdown (GitHub Flavour)

- **Headings** — H1–H6, each with a distinct color; H1/H2 get an underline rule.
- **Emphasis** — `*italic*`, `**bold**`, `~~strikethrough~~`, `_underline_`.
- **Lists** — Ordered, unordered, and task lists (`[x]` / `[ ]`). Tight vs loose mode preserved.
- **Blockquotes** — Nested with `│ ` prefix.
- **Code blocks** — Box-drawn fence with language label; custom handlers via `onFence()`.
- **Inline code** — Light-blue styling, spaces replaced with NBSP to prevent mid-word wrapping.
- **Tables** — Auto-sized columns, alignment (`l`/`c`/`r`), box-drawing borders.
- **Links & Images** — Underlined blue links; images rendered as `[image: alt]`. URLs shown in parens if `showUrls` is enabled.
- **Horizontal rules** — Dashed line using box-drawing characters.
- **HTML blocks** — Tags/comments stripped; `<br>` → newline, `<img>` → `[image: alt]`.
- **HTML entities** — Named (`&amp;`, `&hellip;`) and numeric (`&#123;`, `&#x7F;`) decoded to UTF-8.

## Design Notes

### Color vs Plain Toggle
Setting `color = false` replaces ANSI codes with literal Markdown markers (`**bold**`, `*italic*`), making output suitable for plain-text viewers or copy-paste into rich editors.

### Nested Scope Prefixes
Quotes inside list items (or vice versa) stack their prefixes correctly via the `Scope` chain, each contributing its indentation and marker.

### Tight Lists
No blank lines are emitted between blocks inside tight list items, matching GitHub's rendering behavior.

### Implicit Paragraphs
md4c omits `<p>` tags in tight list items; `ensureLeaf()` creates one on the first text event so inline formatting still works.

### Table Width Budgeting
Column widths start at their natural (longest-line) size and shrink from widest to narrowest until they fit the available terminal width, respecting a per-word minimum of 10 characters.

## File Structure

```
include/md/
├── terminal_renderer.h    # Public API header
└── README.md              # This file
src/md/
└── terminal_renderer.cpp  # Implementation (md4c callbacks, styling, output assembly)
```
