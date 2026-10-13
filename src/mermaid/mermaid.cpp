// mermaid.cpp - entry point: cleans the source, picks the diagram family, renders it.
#include "mermaid/mermaid.h"
#include "mermaid/diagrams.h"
#include "text.h"

#include <cctype>

namespace mermaid {

std::optional<std::vector<std::string>> render(std::string_view source, const Options &options) {
    std::vector<std::string> lines;
    {
        auto raw = text::splitLines(std::string(source));
        size_t i = 0;
        if (!raw.empty() && text::trim(raw[0]) == "---") { // YAML front matter / config block
            i = 1;
            while (i < raw.size() && text::trim(raw[i]) != "---")
                i++;
            i++;
        }
        for (; i < raw.size(); i++) {
            std::string t = text::trim(raw[i]);
            if (t.empty() || text::startsWith(t, "%%"))
                continue;
            lines.push_back(t);
        }
    }
    if (lines.empty())
        return std::nullopt;

    size_t e = 0;
    while (e < lines[0].size() && !isspace((unsigned char)lines[0][e]) && lines[0][e] != ';')
        e++;
    const std::string kind = lines[0].substr(0, e);

    detail::Lines out;
    if (kind == "graph" || kind == "flowchart" || kind == "flowchart-elk")
        out = detail::renderFlowchart(lines, options);
    else if (kind == "sequenceDiagram")
        out = detail::renderSequence(lines, options);
    else if (kind == "stateDiagram" || kind == "stateDiagram-v2")
        out = detail::renderStateDiagram(lines, options);
    else if (kind == "pie")
        out = detail::renderPie(lines, options);

    if (out.empty())
        return std::nullopt;
    return out;
}

} // namespace mermaid
