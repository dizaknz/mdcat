// Markdown -> styled terminal text, driven by md4c callbacks.
//
// md4c parses the Markdown and reports blocks/spans/text through callbacks. This class
// receives those events and lays the result out for a terminal (wrapping, list/quote
// prefixes, tables, ANSI styling).
//
// Fenced code blocks are routed by language to registered handlers, which is how Mermaid
// reaches the ASCII renderer without this class knowing anything about diagrams:
//
//     renderer.onFence("mermaid", [](const std::string& code, int width) {
//         return mermaid::render(code, {width, true});
//     });
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace md {

struct RenderOptions {
    int width = 80;
    bool color = true;
    bool unicode = true;  // box-drawing / bullets vs plain ASCII
    bool showUrls = true; // print "(url)" after link text
};

// Turns the source of a fenced code block into ready-made rows.
// Return std::nullopt to decline; the block is then shown as ordinary code.
using FenceHandler =
    std::function<std::optional<std::vector<std::string>>(const std::string &code, int availableWidth)>;

class TerminalRenderer {
  public:
    explicit TerminalRenderer(RenderOptions options);
    ~TerminalRenderer();
    TerminalRenderer(const TerminalRenderer &) = delete;
    TerminalRenderer &operator=(const TerminalRenderer &) = delete;

    // Route fenced code blocks whose info string starts with `language` (case-insensitive).
    void onFence(std::string language, FenceHandler handler);

    // Render a whole document; the result ends with a newline unless it is empty.
    std::string render(std::string_view markdown);

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace md
