// sequence.cpp - sequenceDiagram: participants as boxes with lifelines, messages as arrows.
#include <algorithm>
#include <map>
#include <regex>
#include <string>
#include <vector>

#include "ascii/canvas.h"
#include "text.h"
#include "mermaid/diagrams.h"

namespace mermaid::detail {

using std::string;
using std::vector;
using text::displayWidth;
using text::startsWith;
using text::toLower;
using text::trim;
static const size_t npos = string::npos;

Lines renderSequence(const Lines &L, const Options &opt) {
    vector<string> names;
    std::map<string, int> idx;
    auto part = [&](const string &id, const string &label) -> int {
        auto it = idx.find(id);
        if (it == idx.end()) {
            idx[id] = (int)names.size();
            names.push_back(label.empty() ? id : label);
            return idx[id];
        }
        if (!label.empty())
            names[it->second] = label;
        return it->second;
    };
    struct Ev {
        int type = 0;
        int a = -1, b = -1;
        string text;
        vector<string> lines;
        bool dashed = false;
        char head = '>';
        int npos = 0;
    };
    vector<Ev> ev;
    bool autonum = false;
    int num = 0;
    static const std::regex rePart(R"re(^(?:participant|actor)\s+(\S+?)(?:\s+as\s+(.+))?$)re");
    static const std::regex reMsg(R"re(^(\S+?)\s*(-->>|->>|--x|-x|--\)|-\)|-->|->)\s*([+-]?)(\S+?)\s*:\s*(.*)$)re");
    static const std::regex reNote(R"re(^[Nn]ote\s+(over|right of|left of)\s+([^:]+):\s*(.*)$)re");
    static const std::regex reBlk(R"re(^(loop|alt|opt|par|critical|break|rect)\b\s*(.*)$)re");
    static const std::regex reElse(R"re(^(else|and|option)\b\s*(.*)$)re");
    for (size_t k = 1; k < L.size(); k++) {
        const string &s = L[k];
        std::smatch m;
        if (s == "autonumber") {
            autonum = true;
            continue;
        }
        if (std::regex_match(s, m, rePart)) {
            part(m[1].str(), m[2].matched ? trim(m[2].str()) : "");
            continue;
        }
        if (std::regex_match(s, m, reNote)) {
            Ev e;
            e.type = 1;
            string w = m[1].str();
            e.npos = w == "over" ? 0 : (w == "right of" ? 1 : 2);
            string who = m[2].str();
            size_t c = who.find(',');
            e.a = part(trim(who.substr(0, c)), "");
            e.b = c == npos ? e.a : part(trim(who.substr(c + 1)), "");
            e.lines = labelLines(m[3].str());
            ev.push_back(e);
            continue;
        }
        if (std::regex_match(s, m, reMsg)) {
            Ev e;
            e.type = 0;
            e.a = part(m[1].str(), "");
            e.b = part(m[4].str(), "");
            string op = m[2].str();
            e.dashed = op.find("--") == 0;
            e.head = op.find('x') != npos ? 'x' : '>';
            auto ls = labelLines(m[5].str());
            string t;
            for (size_t i = 0; i < ls.size(); i++)
                t += (i ? " " : "") + ls[i];
            e.text = (autonum ? std::to_string(++num) + ". " : "") + t;
            ev.push_back(e);
            continue;
        }
        if (std::regex_match(s, m, reBlk)) {
            Ev e;
            e.type = 2;
            e.text = trim(m[1].str() + " " + m[2].str());
            ev.push_back(e);
            continue;
        }
        if (std::regex_match(s, m, reElse)) {
            Ev e;
            e.type = 3;
            e.text = trim(m[1].str() + " " + m[2].str());
            ev.push_back(e);
            continue;
        }
        if (s == "end") {
            Ev e;
            e.type = 4;
            ev.push_back(e);
            continue;
        }
    }
    const int P = (int)names.size();
    if (!P)
        return {};
    vector<int> bw(P), dist(std::max(P - 1, 0));
    for (int i = 0; i < P; i++)
        bw[i] = displayWidth(names[i]) + 4;
    for (int k = 0; k + 1 < P; k++)
        dist[k] = (bw[k] - bw[k] / 2) + bw[k + 1] / 2 + 2;
    struct Req {
        int lo, hi, need;
    };
    vector<Req> reqs;
    int leftMargin = bw[0] / 2;
    for (auto &e : ev) {
        if (e.type == 0) {
            int tw = displayWidth(e.text);
            if (e.a != e.b)
                reqs.push_back({std::min(e.a, e.b), std::max(e.a, e.b), tw + 4});
            else if (e.a + 1 < P)
                dist[e.a] = std::max(dist[e.a], tw + 8);
        } else if (e.type == 1) {
            int mw = 0;
            for (auto &l : e.lines)
                mw = std::max(mw, displayWidth(l));
            int nw = mw + 4;
            if (e.npos == 1) {
                if (e.a + 1 < P)
                    dist[e.a] = std::max(dist[e.a], nw + 3);
            } else if (e.npos == 2) {
                if (e.a > 0)
                    dist[e.a - 1] = std::max(dist[e.a - 1], nw + 3);
                else
                    leftMargin = std::max(leftMargin, nw + 3);
            } else if (e.a != e.b)
                reqs.push_back({std::min(e.a, e.b), std::max(e.a, e.b), nw - 5});
            else if (e.a == 0)
                leftMargin = std::max(leftMargin, nw / 2 + 1);
        }
    }
    std::stable_sort(reqs.begin(), reqs.end(), [](const Req &a, const Req &b) { return a.hi - a.lo < b.hi - b.lo; });
    for (auto &r : reqs) {
        int sum = 0;
        for (int k = r.lo; k < r.hi; k++)
            sum += dist[k];
        if (sum < r.need)
            dist[r.hi - 1] += r.need - sum;
    }
    vector<int> cx(P);
    cx[0] = leftMargin;
    for (int k = 0; k + 1 < P; k++)
        cx[k + 1] = cx[k] + dist[k];
    const int W = cx[P - 1] + (bw[P - 1] - bw[P - 1] / 2);

    ascii::Canvas cv(opt.glyphs());
    auto box = [&](int i, int y) {
        int x = cx[i] - bw[i] / 2;
        cv.box(x, y, bw[i], 3, ascii::BoxStyle::Square, {names[i]});
    };
    for (int i = 0; i < P; i++)
        box(i, 0);
    int y = 4;
    for (auto &e : ev) {
        if (e.type == 0) {
            if (e.a != e.b) {
                int lo = std::min(e.a, e.b), hi = std::max(e.a, e.b);
                int xl = cx[lo] + 1, xr = cx[hi] - 1;
                if (!e.text.empty()) {
                    int tw = displayWidth(e.text);
                    cv.text(xl + ((xr - xl + 1) - tw) / 2, y, e.text);
                    y++;
                }
                cv.hline(xl, xr, y, e.dashed ? ascii::Stroke::Dashed : ascii::Stroke::Solid);
                bool ltr = e.a < e.b;
                if (e.head == 'x')
                    cv.crossHead(ltr ? xr : xl, y);
                else
                    cv.arrowHead(ltr ? xr : xl, y, ltr ? ascii::Heading::Right : ascii::Heading::Left);
                y++;
            } else {
                int x0 = cx[e.a]; // self-message: a small loop to the right of the lifeline
                cv.line(x0 + 1, y, x0 + 4, y);
                cv.line(x0 + 4, y, x0 + 4, y + 2);
                cv.line(x0 + 4, y + 2, x0 + 2, y + 2);
                cv.arrowHead(x0 + 1, y + 2, ascii::Heading::Left);
                if (!e.text.empty())
                    cv.text(x0 + 6, y, e.text);
                y += 3;
            }
        } else if (e.type == 1) {
            int mw = 0;
            for (auto &l : e.lines)
                mw = std::max(mw, displayWidth(l));
            int w = mw + 4, h = (int)e.lines.size() + 2, x;
            int lo = std::min(e.a, e.b), hi = std::max(e.a, e.b);
            if (e.npos == 1)
                x = cx[e.a] + 2;
            else if (e.npos == 2)
                x = cx[e.a] - 1 - w;
            else if (lo != hi) {
                w = std::max(w, cx[hi] - cx[lo] + 5);
                x = (cx[lo] + cx[hi]) / 2 - w / 2;
            } else
                x = cx[e.a] - w / 2;
            cv.box(x, y, w, h, ascii::BoxStyle::Square, e.lines, ascii::Align::Left);
            y += h;
        } else {
            cv.hline(0, W - 1, y, e.type == 3 ? ascii::Stroke::Dotted : ascii::Stroke::Solid);
            if (!e.text.empty())
                cv.text(2, y, " " + e.text + " ");
            y++;
        }
    }
    for (int i = 0; i < P; i++)
        cv.lifeline(cx[i], 3, y);
    for (int i = 0; i < P; i++)
        box(i, y + 1);
    return cv.render();
}

} // namespace mermaid::detail
