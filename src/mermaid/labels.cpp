// labels.cpp - cleans Mermaid node/edge label text (<br>, entities, quotes) into lines.
#include <cstring>
#include <regex>
#include <string>
#include <vector>

#include "text.h"
#include "mermaid/diagrams.h"

namespace mermaid::detail {

using std::string;
using std::vector;
using text::trim;
static const size_t npos = string::npos;

Lines labelLines(string t) {
    static const std::regex br(R"re(<br\s*/?>)re", std::regex::icase);
    static const std::regex tag(
        R"re(</?(?:b|i|u|em|strong|small|span|code|sub|sup)(?:\s[^>]*)?>)re", std::regex::icase
    );
    t = std::regex_replace(t, br, "\n");
    t = std::regex_replace(t, tag, "");
    size_t p;
    while ((p = t.find("\\n")) != npos)
        t.replace(p, 2, "\n");
    t = trim(t);
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"')
        t = trim(t.substr(1, t.size() - 2));
    if (t.size() >= 2 && t.front() == '`' && t.back() == '`')
        t = trim(t.substr(1, t.size() - 2));
    static const std::pair<const char *, const char *> ents[] = {{"&quot;", "\""}, {"#quot;", "\""}, {"&amp;", "&"},
                                                                 {"&lt;", "<"},    {"&gt;", ">"},    {"&nbsp;", " "}};
    for (auto &e : ents) {
        size_t q = 0;
        while ((q = t.find(e.first, q)) != npos) {
            t.replace(q, strlen(e.first), e.second);
            q += strlen(e.second);
        }
    }
    vector<string> v;
    size_t s = 0;
    for (;;) {
        size_t e = t.find('\n', s);
        v.push_back(trim(t.substr(s, e == npos ? npos : e - s)));
        if (e == npos)
            break;
        s = e + 1;
    }
    if (v.empty())
        v.push_back("");
    return v;
}

} // namespace mermaid::detail
