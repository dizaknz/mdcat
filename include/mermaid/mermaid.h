// Mermaid diagram source -> ASCII/Unicode art.
//
// Supported: flowchart/graph (TD TB BT LR RL), sequenceDiagram, stateDiagram(-v2), pie.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ascii/glyphs.h"

namespace mermaid {

struct Options {
    int maxWidth = 80;   // wide flowcharts try the other orientation to fit this
    bool unicode = true; // box-drawing glyphs vs plain ASCII

    const ascii::Glyphs &glyphs() const { return ascii::Glyphs::get(unicode); }
};

// Returns the rendered rows (plain text, no ANSI), or std::nullopt when the diagram type is
// unsupported or the source could not be understood.
std::optional<std::vector<std::string>> render(std::string_view source, const Options &options = {});

} // namespace mermaid
