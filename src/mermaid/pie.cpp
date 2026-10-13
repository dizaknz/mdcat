// pie.cpp - pie chart rendered as horizontal bars.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <regex>
#include <string>
#include <vector>

#include "text.h"
#include "mermaid/diagrams.h"

namespace mermaid::detail {

using std::string;
using std::vector;
using text::displayWidth;
using text::startsWith;
using text::trim;
static const size_t npos = string::npos;

Lines renderPie(const Lines &L, const Options &opt) {
    string title;
    vector<std::pair<string, std::pair<double, string>>> items;
    {
        string h = L[0];
        size_t p = h.find("title ");
        if (p != npos)
            title = trim(h.substr(p + 6));
    }
    static const std::regex reItem(R"re(^"([^"]+)"\s*:\s*([0-9]*\.?[0-9]+)\s*$)re");
    for (size_t k = 1; k < L.size(); k++) {
        std::smatch m;
        if (startsWith(L[k], "title "))
            title = trim(L[k].substr(6));
        else if (std::regex_match(L[k], m, reItem))
            items.push_back({m[1].str(), {atof(m[2].str().c_str()), m[2].str()}});
    }
    if (items.empty())
        return {};
    double total = 0;
    int maxLab = 0;
    for (auto &it : items) {
        total += it.second.first;
        maxLab = std::max(maxLab, displayWidth(it.first));
    }
    if (total <= 0)
        return {};
    int barW = std::max(10, std::min(40, opt.maxWidth - maxLab - 20));
    vector<string> out;
    if (!title.empty()) {
        out.push_back(title);
        out.push_back("");
    }
    for (auto &it : items) {
        int filled = (int)std::lround(it.second.first / total * barW);
        string bar;
        for (int i = 0; i < barW; i++)
            bar += text::utf8(i < filled ? opt.glyphs().barFull : opt.glyphs().barEmpty);
        char buf[32];
        snprintf(buf, sizeof buf, "%5.1f%%", it.second.first / total * 100.0);
        out.push_back(
            it.first + string(maxLab - displayWidth(it.first), ' ') + " " + bar + " " + buf + " (" + it.second.second +
            ")"
        );
    }
    return out;
}

} // namespace mermaid::detail
