// flowchart.cpp - flowchart/graph and stateDiagram: parse -> layered layout -> orthogonal routing.
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "ascii/canvas.h"
#include "text.h"
#include "mermaid/diagrams.h"

namespace mermaid::detail {

using std::string;
using std::vector;
using text::displayWidth;
using text::repeat;
using text::startsWith;
using text::toLower;
using text::trim;
using text::utf8;
static const size_t npos = string::npos;

struct FNode {
    string id;
    vector<string> lines;
    int shape = 0; // 0 rect 1 round 2 stadium 3 circle 4 diamond 5 hex 6 subroutine 7 cylinder 8 asym
};
struct FEdge {
    int a, b;
    string label;
    bool arrowEnd = true, arrowStart = false;
};
struct Flow {
    char dir = 'T'; // T (TD/TB), B (BT), L (LR), R (RL)
    vector<FNode> nodes;
    std::map<string, int> idx;
    vector<FEdge> edges;
    int node(const string &id) {
        auto it = idx.find(id);
        if (it != idx.end())
            return it->second;
        idx[id] = (int)nodes.size();
        FNode n;
        n.id = id;
        n.lines = {id};
        nodes.push_back(n);
        return (int)nodes.size() - 1;
    }
};

static void skipWs(const string &s, size_t &i) {
    while (i < s.size() && isspace((unsigned char)s[i]))
        i++;
}
static bool isIdCh(unsigned char c) { return isalnum(c) || c == '_' || c >= 0x80; }

static int parseNode(const string &s, size_t &i, Flow &f) {
    size_t st = i;
    while (i < s.size() && isIdCh((unsigned char)s[i]))
        i++;
    if (i == st)
        return -1;
    string id = s.substr(st, i - st);
    int shape = 0;
    string text;
    bool has = false;
    struct Sh {
        const char *o;
        const char *c;
        int shape;
    };
    static const Sh shs[] = {{"[[", "]]", 6}, {"[(", ")]", 7}, {"([", "])", 2}, {"((", "))", 3}, {"{{", "}}", 5},
                             {"[", "]", 0},   {"(", ")", 1},   {"{", "}", 4},   {">", "]", 8}};
    for (const Sh &h : shs) {
        size_t ol = strlen(h.o);
        if (s.compare(i, ol, h.o) != 0)
            continue;
        size_t p = i + ol, c;
        if (p < s.size() && s[p] == '"') {
            size_t q = s.find('"', p + 1);
            c = (q == npos) ? npos : s.find(h.c, q + 1);
        } else {
            c = s.find(h.c, p);
        }
        if (c == npos)
            continue;
        text = s.substr(p, c - p);
        i = c + strlen(h.c);
        shape = h.shape;
        has = true;
        break;
    }
    if (s.compare(i, 3, ":::") == 0) {
        i += 3;
        while (i < s.size() && isIdCh((unsigned char)s[i]))
            i++;
    }
    int n = f.node(id);
    if (has) {
        if (shape == 0 && text.size() >= 2 && (text[0] == '/' || text[0] == '\\') &&
            (text.back() == '/' || text.back() == '\\'))
            text = text.substr(1, text.size() - 2);
        f.nodes[n].lines = labelLines(text);
        f.nodes[n].shape = shape;
    }
    return n;
}

static vector<int> parseGroup(const string &s, size_t &i, Flow &f) {
    vector<int> g;
    for (;;) {
        skipWs(s, i);
        int n = parseNode(s, i, f);
        if (n < 0)
            break;
        g.push_back(n);
        skipWs(s, i);
        if (i < s.size() && s[i] == '&') {
            i++;
            continue;
        }
        break;
    }
    return g;
}

static void parseFlowStmt(Flow &f, const string &line) {
    string s = trim(line);
    if (s.empty())
        return;
    {
        size_t e = 0;
        while (e < s.size() && !isspace((unsigned char)s[e]))
            e++;
        string w = s.substr(0, e);
        static const std::set<string> skip = {"subgraph",  "end",   "classDef",  "class",    "style",
                                              "linkStyle", "click", "direction", "accTitle", "accDescr"};
        if (skip.count(w))
            return;
    }
    static const std::regex R1(R"re((<)?((?:-\.+->|-\.+-|-{2,}>|-{3,}|={2,}>|={3,}|--[ox]))(?:\s*\|([^|]*)\|)?\s*)re");
    static const std::regex R2(R"re((<)?(?:--|==|-\.)\s+(.+?)\s+((?:-{2,}>|-{3,}|={2,}>|={3,}|\.+->|\.+-))\s*)re");
    size_t i = 0;
    vector<int> prev = parseGroup(s, i, f);
    if (prev.empty())
        return;
    for (;;) {
        skipWs(s, i);
        if (i >= s.size())
            break;
        std::smatch m;
        string label, op;
        bool aStart = false;
        auto b = s.cbegin() + i;
        if (std::regex_search(b, s.cend(), m, R1, std::regex_constants::match_continuous)) {
            aStart = m[1].matched;
            op = m[2].str();
            label = m[3].matched ? m[3].str() : "";
        } else if (std::regex_search(b, s.cend(), m, R2, std::regex_constants::match_continuous)) {
            aStart = m[1].matched;
            label = m[2].str();
            op = m[3].str();
        } else {
            break;
        }
        i += (size_t)m.length(0);
        bool aEnd = !op.empty() && (op.back() == '>' || op.back() == 'o' || op.back() == 'x');
        string lab;
        if (!label.empty()) {
            auto ls = labelLines(label);
            for (size_t k = 0; k < ls.size(); k++)
                lab += (k ? " " : "") + ls[k];
        }
        vector<int> next = parseGroup(s, i, f);
        if (next.empty())
            break;
        for (int a : prev)
            for (int b2 : next) {
                FEdge e;
                e.a = a;
                e.b = b2;
                e.label = lab;
                e.arrowEnd = aEnd;
                e.arrowStart = aStart;
                f.edges.push_back(e);
            }
        prev = next;
    }
}

static Flow parseFlow(const vector<string> &L) {
    Flow f;
    {
        std::istringstream is(L[0]);
        string w1, w2;
        is >> w1 >> w2;
        w2 = toLower(w2);
        while (!w2.empty() && w2.back() == ';')
            w2.pop_back();
        if (w2 == "bt")
            f.dir = 'B';
        else if (w2 == "lr")
            f.dir = 'L';
        else if (w2 == "rl")
            f.dir = 'R';
        else
            f.dir = 'T';
    }
    for (size_t k = 1; k < L.size(); k++) {
        size_t s0 = 0;
        const string &ln = L[k];
        for (;;) {
            size_t e = ln.find(';', s0);
            parseFlowStmt(f, ln.substr(s0, e == npos ? npos : e - s0));
            if (e == npos)
                break;
            s0 = e + 1;
        }
    }
    return f;
}

static vector<string> renderFlowDir(const Flow &f, const Options &opt) {
    const int N = (int)f.nodes.size();
    if (N == 0)
        return {};
    const bool horiz = f.dir == 'L' || f.dir == 'R';
    const bool mirror = f.dir == 'B' || f.dir == 'R';

    // ---- 1. break cycles, collect forward edges --------------------------------
    struct E {
        int from, to, ei;
        bool aFrom, aTo;
    };
    vector<E> es;
    {
        vector<vector<int>> adj(N);
        for (size_t k = 0; k < f.edges.size(); k++)
            if (f.edges[k].a != f.edges[k].b)
                adj[f.edges[k].a].push_back((int)k);
        vector<int> state(N, 0);
        vector<char> rev(f.edges.size(), 0);
        std::function<void(int)> dfs = [&](int u) {
            state[u] = 1;
            for (int ei : adj[u]) {
                int v = f.edges[ei].b;
                if (state[v] == 1)
                    rev[ei] = 1;
                else if (state[v] == 0)
                    dfs(v);
            }
            state[u] = 2;
        };
        for (int u = 0; u < N; u++)
            if (!state[u])
                dfs(u);
        std::set<std::tuple<int, int, string>> seen;
        for (size_t k = 0; k < f.edges.size(); k++) {
            const FEdge &e = f.edges[k];
            if (e.a == e.b)
                continue;
            if (!seen.insert(std::make_tuple(e.a, e.b, e.label)).second)
                continue;
            if (rev[k])
                es.push_back({e.b, e.a, (int)k, e.arrowEnd, e.arrowStart});
            else
                es.push_back({e.a, e.b, (int)k, e.arrowStart, e.arrowEnd});
        }
    }

    // ---- 2. layering (longest path) ---------------------------------------------
    vector<vector<int>> preds(N);
    for (auto &e : es)
        preds[e.to].push_back(e.from);
    vector<int> layer(N, -1);
    std::function<int(int)> lf = [&](int v) -> int {
        if (layer[v] >= 0)
            return layer[v];
        layer[v] = 0;
        int l = 0;
        for (int p : preds[v])
            l = std::max(l, lf(p) + 1);
        return layer[v] = l;
    };
    for (int v = 0; v < N; v++)
        lf(v);

    // ---- 3. layered nodes with dummy nodes for long edges -----------------------
    struct LN {
        int real = -1, layer = 0, sz = 1, th = 1, pos = 0, a0 = 0;
        vector<int> ins, outs;
    };
    struct Sg {
        int from = 0, to = 0, ei = 0;
        bool first = false, last = false, aFrom = false, aTo = false;
        int xs = 0, xt = 0, track = 0, pOut = 0, pIn = 0;
    };
    vector<LN> ln;
    for (int v = 0; v < N; v++) {
        LN x;
        x.real = v;
        x.layer = layer[v];
        ln.push_back(x);
    }
    vector<Sg> sg;
    for (auto &e : es) {
        if (layer[e.to] <= layer[e.from])
            continue;
        int prev = e.from;
        for (int l = layer[e.from] + 1; l < layer[e.to]; l++) {
            LN d;
            d.real = -1;
            d.layer = l;
            ln.push_back(d);
            int di = (int)ln.size() - 1;
            Sg s;
            s.from = prev;
            s.to = di;
            s.ei = e.ei;
            s.first = (prev == e.from);
            s.last = false;
            s.aFrom = s.first && e.aFrom;
            sg.push_back(s);
            prev = di;
        }
        Sg s;
        s.from = prev;
        s.to = e.to;
        s.ei = e.ei;
        s.first = (prev == e.from);
        s.last = true;
        s.aFrom = s.first && e.aFrom;
        s.aTo = e.aTo;
        sg.push_back(s);
    }
    for (size_t k = 0; k < sg.size(); k++) {
        ln[sg[k].from].outs.push_back((int)k);
        ln[sg[k].to].ins.push_back((int)k);
    }
    int nl = 0;
    for (auto &x : ln)
        nl = std::max(nl, x.layer + 1);
    vector<vector<int>> lay(nl);
    for (size_t i = 0; i < ln.size(); i++)
        lay[ln[i].layer].push_back((int)i);

    // ---- 4. order nodes inside layers (barycenter sweeps) -----------------------
    vector<int> ord(ln.size(), 0);
    auto reindex = [&](int l) {
        for (size_t i = 0; i < lay[l].size(); i++)
            ord[lay[l][i]] = (int)i;
    };
    for (int l = 0; l < nl; l++)
        reindex(l);
    auto sortLayer = [&](int l, bool useIns) {
        vector<std::pair<double, int>> key;
        for (int n : lay[l]) {
            double s = 0;
            int c = 0;
            if (useIns)
                for (int k : ln[n].ins) {
                    s += ord[sg[k].from];
                    c++;
                }
            else
                for (int k : ln[n].outs) {
                    s += ord[sg[k].to];
                    c++;
                }
            key.push_back({c ? s / c : (double)ord[n], n});
        }
        std::stable_sort(key.begin(), key.end(), [](const std::pair<double, int> &a, const std::pair<double, int> &b) {
            return a.first < b.first;
        });
        for (size_t i = 0; i < key.size(); i++)
            lay[l][i] = key[i].second;
        reindex(l);
    };
    for (int it = 0; it < 4; it++) {
        for (int l = 1; l < nl; l++)
            sortLayer(l, true);
        for (int l = nl - 2; l >= 0; l--)
            sortLayer(l, false);
    }

    // ---- 5. node sizes ------------------------------------------------------------
    for (auto &x : ln) {
        if (x.real < 0)
            continue;
        const FNode &nd = f.nodes[x.real];
        int lw = 0;
        for (auto &s : nd.lines)
            lw = std::max(lw, displayWidth(s));
        int w = lw + 4 + (nd.shape == 6 ? 2 : 0), h = (int)nd.lines.size() + 2;
        int need = 2 * (int)std::max(x.ins.size(), x.outs.size()) + 1;
        if (horiz) {
            x.sz = std::max(h, need);
            x.th = w;
        } else {
            x.sz = std::max(w, need);
            x.th = h;
        }
    }

    // ---- 6. positions across the layers -------------------------------------------
    auto gapBetween = [&](int a, int b) { return (ln[a].real < 0 || ln[b].real < 0) ? 2 : (horiz ? 2 : 4); };
    for (int l = 0; l < nl; l++) {
        int p = 0;
        for (size_t i = 0; i < lay[l].size(); i++) {
            int n = lay[l][i];
            ln[n].pos = p;
            p += ln[n].sz + (i + 1 < lay[l].size() ? gapBetween(n, lay[l][i + 1]) : 0);
        }
    }
    auto center = [&](int n) { return ln[n].pos + ln[n].sz / 2; };
    for (int it = 0; it < 8; it++) {
        bool down = it % 2 == 0;
        for (int s = 0; s < nl; s++) {
            int l = down ? s : nl - 1 - s;
            vector<int> want(lay[l].size());
            for (size_t i = 0; i < lay[l].size(); i++) {
                int n = lay[l][i];
                double sum = 0;
                int c = 0;
                if (down)
                    for (int k : ln[n].ins) {
                        sum += center(sg[k].from);
                        c++;
                    }
                else
                    for (int k : ln[n].outs) {
                        sum += center(sg[k].to);
                        c++;
                    }
                want[i] = c ? (int)std::lround(sum / c) - ln[n].sz / 2 : ln[n].pos;
            }
            for (size_t i = 0; i < lay[l].size(); i++) {
                int n = lay[l][i];
                int p = want[i];
                if (i > 0) {
                    int pm = lay[l][i - 1];
                    p = std::max(p, ln[pm].pos + ln[pm].sz + gapBetween(pm, n));
                }
                ln[n].pos = p;
            }
        }
    }
    {
        int mn = INT_MAX;
        for (auto &x : ln)
            mn = std::min(mn, x.pos);
        for (auto &x : ln)
            x.pos -= mn;
    }

    // ---- 7. ports -------------------------------------------------------------------
    auto portPos = [&](int n, int i, int k) {
        int sz = ln[n].sz;
        if (sz == 1 || k <= 1)
            return sz / 2;
        return sz * (2 * i + 1) / (2 * k);
    };
    for (size_t n = 0; n < ln.size(); n++) {
        auto outs = ln[n].outs, ins = ln[n].ins;
        std::sort(outs.begin(), outs.end(), [&](int a, int b) {
            int ca = center(sg[a].to), cb = center(sg[b].to);
            return ca != cb ? ca < cb : a < b;
        });
        std::sort(ins.begin(), ins.end(), [&](int a, int b) {
            int ca = center(sg[a].from), cb = center(sg[b].from);
            return ca != cb ? ca < cb : a < b;
        });
        for (size_t i = 0; i < outs.size(); i++) {
            sg[outs[i]].pOut = (int)i;
            sg[outs[i]].xs = ln[n].pos + portPos((int)n, (int)i, (int)outs.size());
        }
        for (size_t i = 0; i < ins.size(); i++) {
            sg[ins[i]].pIn = (int)i;
            sg[ins[i]].xt = ln[n].pos + portPos((int)n, (int)i, (int)ins.size());
        }
    }

    // ---- 8. channel tracks between layers --------------------------------------------
    vector<int> T(std::max(nl - 1, 1), 1), LZ(std::max(nl - 1, 1), 1);
    int maxLab = 0;
    for (auto &e : f.edges)
        maxLab = std::max(maxLab, displayWidth(e.label));
    for (int g = 0; g + 1 < nl; g++) {
        vector<int> idxs, hz;
        for (size_t k = 0; k < sg.size(); k++)
            if (ln[sg[k].from].layer == g) {
                idxs.push_back((int)k);
                if (sg[k].xs != sg[k].xt)
                    hz.push_back((int)k);
            }
        std::sort(hz.begin(), hz.end(), [&](int a, int b) {
            int la = std::min(sg[a].xs, sg[a].xt), lb = std::min(sg[b].xs, sg[b].xt);
            if (la != lb)
                return la < lb;
            return std::max(sg[a].xs, sg[a].xt) < std::max(sg[b].xs, sg[b].xt);
        });
        vector<int> tend;
        for (int k : hz) {
            int lo = std::min(sg[k].xs, sg[k].xt), hi = std::max(sg[k].xs, sg[k].xt);
            size_t t = 0;
            while (t < tend.size() && tend[t] >= lo)
                t++;
            if (t == tend.size())
                tend.push_back(hi);
            else
                tend[t] = hi;
            sg[k].track = (int)t;
        }
        T[g] = std::max(1, (int)tend.size());
        if (horiz) {
            int ml = 0;
            for (int k : idxs)
                if (sg[k].first)
                    ml = std::max(ml, displayWidth(f.edges[sg[k].ei].label));
            LZ[g] = std::max(1, ml + 1);
        }
    }

    // ---- 9. coordinates along the layer axis -------------------------------------------
    vector<int> thL(nl, 1), start(nl), endA(nl);
    for (auto &x : ln)
        thL[x.layer] = std::max(thL[x.layer], x.th);
    {
        int a = 0;
        for (int l = 0; l < nl; l++) {
            start[l] = a;
            endA[l] = a + thL[l] - 1;
            a = endA[l] + 1 + (l + 1 < nl ? T[l] + LZ[l] + 1 : 0);
        }
    }
    const int A = endA[nl - 1];
    for (auto &x : ln)
        x.a0 = start[x.layer] + (thL[x.layer] - x.th) / 2;
    const int cOff = horiz ? 1 : maxLab + 1;
    auto MX = [&](int a, int c) { return horiz ? (mirror ? A - a : a) : c; };
    auto MY = [&](int a, int c) { return horiz ? c : (mirror ? A - a : a); };

    // ---- 10. draw ---------------------------------------------------------------------------
    ascii::Canvas cv(opt.glyphs());
    auto styleOf = [](int shape) {
        switch (shape) {
        case 1:
        case 2:
        case 3:
        case 7:
            return ascii::BoxStyle::Rounded;
        case 4:
        case 5:
            return ascii::BoxStyle::Diamond;
        case 6:
            return ascii::BoxStyle::Subroutine;
        default:
            return ascii::BoxStyle::Square;
        }
    };
    for (auto &x : ln) {
        if (x.real < 0)
            continue;
        const FNode &nd = f.nodes[x.real];
        int a1 = x.a0 + x.th - 1, c0 = x.pos + cOff, c1 = c0 + x.sz - 1;
        int x1 = MX(x.a0, c0), y1 = MY(x.a0, c0), x2 = MX(a1, c1), y2 = MY(a1, c1);
        cv.box(
            std::min(x1, x2), std::min(y1, y2), std::abs(x2 - x1) + 1, std::abs(y2 - y1) + 1, styleOf(nd.shape),
            nd.lines
        );
    }
    auto heading = [](int dx, int dy) {
        if (dy > 0)
            return ascii::Heading::Down;
        if (dy < 0)
            return ascii::Heading::Up;
        return dx > 0 ? ascii::Heading::Right : ascii::Heading::Left;
    };
    for (auto &s : sg) {
        const LN &fr = ln[s.from];
        const LN &to = ln[s.to];
        int gl = fr.layer;
        int fa = fr.real >= 0 ? fr.a0 + fr.th - 1 : start[gl] - 1;
        int trackA = endA[gl] + LZ[gl] + 1 + s.track;
        int ta = to.real >= 0 ? to.a0 - 1 : start[to.layer] - 1;
        int cs = s.xs + cOff, ct = s.xt + cOff;
        cv.line(MX(fa, cs), MY(fa, cs), MX(trackA, cs), MY(trackA, cs));
        if (cs != ct)
            cv.line(MX(trackA, cs), MY(trackA, cs), MX(trackA, ct), MY(trackA, ct));
        cv.line(MX(trackA, ct), MY(trackA, ct), MX(ta, ct), MY(ta, ct));
        if (s.last) {
            int px = MX(ta, ct), py = MY(ta, ct);
            if (s.aTo)
                cv.arrowHead(px, py, heading(px - MX(ta - 1, ct), py - MY(ta - 1, ct)));
            else
                cv.line(px, py, MX(ta + 1, ct), MY(ta + 1, ct));
        }
        if (s.first && s.aFrom) {
            int px = MX(fa + 1, cs), py = MY(fa + 1, cs);
            cv.arrowHead(px, py, heading(MX(fa, cs) - px, MY(fa, cs) - py));
        }
    }
    for (auto &s : sg) { // labels last so they stay readable
        const FEdge &e = f.edges[s.ei];
        if (!s.first || e.label.empty())
            continue;
        const LN &fr = ln[s.from];
        int len = displayWidth(e.label), cs = s.xs + cOff, la = endA[fr.layer] + 1;
        if (!horiz) {
            bool left = fr.outs.size() > 1 && s.pOut * 2 + 1 < (int)fr.outs.size();
            int c = left ? cs - len : cs + 1;
            cv.text(MX(la, c), MY(la, c), e.label);
        } else {
            int c = cs - 1, xx = MX(la, c);
            if (mirror)
                xx -= (len - 1);
            cv.text(xx, MY(la, c), e.label);
        }
    }
    return cv.render();
}
static Flow parseState(const vector<string> &L, const Options &opt) {
    Flow f;
    f.dir = 'T';
    auto sid = [&](const string &x, bool asSrc) -> int {
        if (x == "[*]") {
            int n = f.node(asSrc ? "__start" : "__end");
            f.nodes[n].lines = {text::utf8(asSrc ? opt.glyphs().startMark : opt.glyphs().endMark)};
            f.nodes[n].shape = 3;
            return n;
        }
        string id = x;
        if (id.size() >= 2 && id.front() == '"' && id.back() == '"')
            id = id.substr(1, id.size() - 2);
        int n = f.node(id);
        if (f.nodes[n].shape == 0)
            f.nodes[n].shape = 1;
        return n;
    };
    static const std::regex reTr(
        R"re(^(\[\*\]|"[^"]+"|[^\s:]+)\s*(<?-->)\s*(\[\*\]|"[^"]+"|[^\s:]+)\s*(?::\s*(.*))?$)re"
    );
    static const std::regex reDesc(R"re(^state\s+"([^"]+)"\s+as\s+(\S+)\s*$)re");
    static const std::regex reId(R"re(^([^\s:]+)\s*:\s*(.+)$)re");
    static const std::regex reDir(R"re(^direction\s+(TB|TD|BT|LR|RL)\s*$)re");
    bool inNote = false;
    for (size_t k = 1; k < L.size(); k++) {
        const string &s = L[k];
        std::smatch m;
        if (inNote) {
            if (startsWith(s, "end note"))
                inNote = false;
            continue;
        }
        if (startsWith(s, "note ") && s.find(':') == npos) {
            inNote = true;
            continue;
        }
        if (startsWith(s, "note ") || s == "}" || s == "--" || startsWith(s, "classDef") || startsWith(s, "class ") ||
            startsWith(s, "style ") || (startsWith(s, "state ") && s.back() == '{'))
            continue;
        if (std::regex_match(s, m, reDir)) {
            string d = m[1].str();
            f.dir = d == "BT" ? 'B' : d == "LR" ? 'L' : d == "RL" ? 'R' : 'T';
        } else if (std::regex_match(s, m, reDesc)) {
            int n = f.node(m[2].str());
            f.nodes[n].lines = labelLines(m[1].str());
            f.nodes[n].shape = 1;
        } else if (std::regex_match(s, m, reTr)) {
            int a = sid(m[1].str(), true), b = sid(m[3].str(), false);
            FEdge e;
            e.a = a;
            e.b = b;
            e.label = m[4].matched ? trim(m[4].str()) : "";
            f.edges.push_back(e);
        } else if (std::regex_match(s, m, reId)) {
            int n = sid(m[1].str(), true);
            if (f.nodes[n].lines.size() == 1 && f.nodes[n].lines[0] == f.nodes[n].id)
                f.nodes[n].lines = labelLines(m[2].str());
            else
                for (auto &x : labelLines(m[2].str()))
                    f.nodes[n].lines.push_back(x);
        }
    }
    return f;
}

// Render a flowchart; if it is wider than the limit, try the other orientation
// (TD <-> LR) and keep whichever fits (or is narrowest).
static vector<string> renderFit(const Flow &f, const Options &opt) {
    auto a = renderFlowDir(f, opt);
    auto widest = [](const vector<string> &v) {
        int w = 0;
        for (auto &l : v)
            w = std::max(w, displayWidth(l));
        return w;
    };
    if (a.empty() || widest(a) <= opt.maxWidth)
        return a;
    Flow g = f;
    g.dir = f.dir == 'L' ? 'T' : f.dir == 'R' ? 'B' : f.dir == 'T' ? 'L' : 'R';
    auto b = renderFlowDir(g, opt);
    return widest(b) < widest(a) ? b : a;
}

Lines renderFlowchart(const Lines &src, const Options &opt) { return renderFit(parseFlow(src), opt); }
Lines renderStateDiagram(const Lines &src, const Options &opt) { return renderFit(parseState(src, opt), opt); }

} // namespace mermaid::detail
