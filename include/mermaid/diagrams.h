// One renderer per Mermaid diagram family.
//
// Each takes the cleaned source lines (comments/front matter removed, trimmed) and returns
// rows, or an empty vector if it could not render anything.
#pragma once

#include <string>
#include <vector>

#include "mermaid.h"

namespace mermaid::detail {

using Lines = std::vector<std::string>;

// Cleans a label (<br>, entities, quotes) into display lines.
Lines labelLines(std::string raw);

Lines renderFlowchart(const Lines &src, const Options &opt);
Lines renderStateDiagram(const Lines &src, const Options &opt);
Lines renderSequence(const Lines &src, const Options &opt);
Lines renderPie(const Lines &src, const Options &opt);

} // namespace mermaid::detail
