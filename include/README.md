# text — UTF-8, display-width & ANSI-aware helpers

Shared utilities used by all modules: Markdown renderer, ASCII canvas, Mermaid backends.

## API Reference

### UTF-8 Encoding / Decoding

| Function | Signature | Description |
|----------|-----------|-------------|
| `utf8` | `string utf8(char32_t cp)` | Encode a Unicode codepoint → UTF-8 bytes |
| `decodeUtf8` | `size_t decodeUtf8(const string &s, size_t i, char32_t &cp)` | Decode one UTF-8 char at offset `i`. Returns byte count consumed. Invalid sequences return `U+FFFD` |
| `fromU32` | `string fromU32(const u32string &s)` | Convert a `u32string` → UTF-8 `string` |

### Display Width

| Function | Signature | Description |
|----------|-----------|-------------|
| `codepointWidth` | `int codepointWidth(char32_t cp)` | Terminal columns occupied by one codepoint: **0** (control/combining/ZWJ), **1**, or **2** (CJK, Hangul, emoji, full-width) |
| `displayWidth` | `int displayWidth(const string &s)` | Visible column count of a string. **Ignores ANSI SGR escape sequences** — safe to pass already-styled text |

#### Width classification

| Range | Width | Examples |
|-------|-------|----------|
| `0x00–0x1F`, `0x7F`, `0x80–0x9F` | 0 | C0/C1 controls |
| `0x300–0x36F`, `0x200B–0x200F`, `0xFE00–0xFE0F`, `0x20D0–0x20FF`, `0xFEFF` | 0 | Combining marks, ZWJ, BOM |
| CJK, Hangul, full-width, emoji (`0x1100+`) | 2 | `汉`, `가`, `🎉` |
| Everything else | 1 | ASCII, Latin, Cyrillic, etc. |

### String Utilities

| Function | Signature | Description |
|----------|-----------|-------------|
| `trim` | `string trim(const string &s)` | Strip leading/trailing whitespace |
| `toLower` | `string toLower(string s)` | Case-fold to lowercase (ASCII) |
| `startsWith` | `bool startsWith(const string &s, const string &prefix)` | Prefix check |
| `repeat` | `string repeat(const string &s, int n)` | Concatenate `s`, `n` times |
| `splitLines` | `vector<string> splitLines(const string &s)` | Split on `\n` |
| `expandTabs` | `string expandTabs(const string &line)` | Replace `\t` with spaces (4-space tab stops, width-aware) |

### Security

| Function | Signature | Description |
|----------|-----------|-------------|
| `sanitize` | `string sanitize(string_view s)` | Strip bytes that could drive the terminal: all C0 controls (`0x00–0x1F`), DEL (`0x7F`), and C1 controls (`0x80–0x9F`). Keeps `\n`; converts `\t` → space. Use on **untrusted input** before rendering |

### Word Wrapping (ANSI-aware)

| Function | Signature | Description |
|----------|-----------|-------------|
| `wrap` | `vector<string> wrap(const string &text, int width)` | Wrap text to `width` columns. Handles embedded ANSI SGR codes and `\n` hard breaks |

#### How `wrap()` works

1. **Splits** on `\n` into "hard lines"
2. **Tokenizes** each line by spaces
3. If a token exceeds `width`, cuts it at column boundaries (ANSI-safe — escapes are never split mid-stream)
4. **Tracks open styles** via an internal `SgrState`: bold, dim, italic, underline, strikethrough, fg/bg colors (including 256-color and 24-bit)
5. At line breaks: **closes** all open styles (`\x1b[0m`), then **re-opens** them on the next line so formatting spans across wrapped lines

#### Example

```cpp
// Input contains ANSI codes + soft wrap point
string input = "\x1b[1;31mHello \x1b[0m\x1b[3;34mworld of terminal rendering";
auto lines = text::wrap(input, 20);
// Output:
//   "\x1b[1;31mHello \x1b[0m\x1b[3;34mworld"
//   "\x1b[3;34m of terminal rendering\x1b[0m"
```

Each returned line is **self-contained** — it opens any styles that were active and closes them at the end.

## File Structure

```
include/
├── text.h                 # Public API header
└── README.md              # This file
src/
└── text.cpp               # Implementation
```
