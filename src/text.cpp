// text.cpp - UTF-8, display-width and ANSI-aware word wrapping helpers.
#include "text.h"

#include <cctype>
#include <cstdlib>

namespace text {

using std::string;
using std::u32string;
using std::vector;
static const size_t npos = string::npos;

// ============================================================================
// UTF-8 / string helpers
// ============================================================================
string utf8(char32_t c) {
    string o;
    if (c < 0x80) {
        o += (char)c;
    } else if (c < 0x800) {
        o += (char)(0xC0 | (c >> 6));
        o += (char)(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        o += (char)(0xE0 | (c >> 12));
        o += (char)(0x80 | ((c >> 6) & 0x3F));
        o += (char)(0x80 | (c & 0x3F));
    } else {
        o += (char)(0xF0 | (c >> 18));
        o += (char)(0x80 | ((c >> 12) & 0x3F));
        o += (char)(0x80 | ((c >> 6) & 0x3F));
        o += (char)(0x80 | (c & 0x3F));
    }
    return o;
}

size_t decodeUtf8(const string &s, size_t i, char32_t &cp) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) {
        cp = c;
        return 1;
    }
    int n = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
    if (n == 1 || i + n > s.size()) {
        cp = 0xFFFD;
        return 1;
    }
    cp = c & (0xFF >> (n + 1));
    for (int k = 1; k < n; k++)
        cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
    return (size_t)n;
}

string fromU32(const u32string &s) {
    string o;
    for (char32_t c : s)
        o += utf8(c);
    return o;
}

int codepointWidth(char32_t c) {
    if (c == 0)
        return 0;
    if (c < 0x20 || (c >= 0x7F && c < 0xA0))
        return 0;
    if ((c >= 0x300 && c <= 0x36F) || (c >= 0x200B && c <= 0x200F) || (c >= 0xFE00 && c <= 0xFE0F) ||
        (c >= 0x20D0 && c <= 0x20FF) || c == 0xFEFF)
        return 0;
    if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) ||
        (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE6F) || (c >= 0xFF00 && c <= 0xFF60) ||
        (c >= 0xFFE0 && c <= 0xFFE6) || (c >= 0x1F300 && c <= 0x1F64F) || (c >= 0x1F900 && c <= 0x1F9FF) ||
        (c >= 0x20000 && c <= 0x3FFFD))
        return 2;
    return 1;
}

// Visible width of a string, ignoring ANSI escape sequences.
int displayWidth(const string &s) {
    int w = 0;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
            i += 2;
            while (i < s.size() && !((unsigned char)s[i] >= 0x40 && (unsigned char)s[i] <= 0x7E))
                i++;
            i++;
            continue;
        }
        char32_t cp;
        i += decodeUtf8(s, i, cp);
        w += codepointWidth(cp);
    }
    return w;
}

string trim(const string &s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a]))
        a++;
    while (b > a && isspace((unsigned char)s[b - 1]))
        b--;
    return s.substr(a, b - a);
}
string toLower(string s) {
    for (auto &c : s)
        c = (char)tolower((unsigned char)c);
    return s;
}
bool startsWith(const string &s, const string &p) { return s.compare(0, p.size(), p) == 0; }
string repeat(const string &s, int n) {
    string r;
    for (int i = 0; i < n; i++)
        r += s;
    return r;
}
vector<string> splitLines(const string &t) {
    vector<string> v;
    size_t s = 0;
    for (;;) {
        size_t e = t.find('\n', s);
        if (e == npos) {
            if (s < t.size())
                v.push_back(t.substr(s));
            break;
        }
        v.push_back(t.substr(s, e - s));
        s = e + 1;
    }
    return v;
}
string expandTabs(const string &l) {
    if (l.find('\t') == npos)
        return l;
    string o;
    int col = 0;
    for (char c : l) {
        if (c == '\t') {
            int n = 4 - (col % 4);
            o.append(n, ' ');
            col += n;
        } else {
            o += c;
            if (((unsigned char)c & 0xC0) != 0x80)
                col++;
        }
    }
    return o;
}

struct SgrState {
    bool b = false, d = false, i = false, u = false, s = false;
    string fg, bg;
    void apply(const string &p) {
        vector<int> v;
        string cur;
        for (char c : p + ";") {
            if (c == ';') {
                v.push_back(cur.empty() ? 0 : atoi(cur.c_str()));
                cur.clear();
            } else
                cur += c;
        }
        for (size_t k = 0; k < v.size(); k++) {
            int c = v[k];
            if (c == 0)
                *this = SgrState();
            else if (c == 1)
                b = true;
            else if (c == 2)
                d = true;
            else if (c == 22) {
                b = false;
                d = false;
            } else if (c == 3)
                i = true;
            else if (c == 23)
                i = false;
            else if (c == 4)
                u = true;
            else if (c == 24)
                u = false;
            else if (c == 9)
                s = true;
            else if (c == 29)
                s = false;
            else if ((c >= 30 && c <= 37) || (c >= 90 && c <= 97))
                fg = std::to_string(c);
            else if (c == 39)
                fg.clear();
            else if ((c >= 40 && c <= 47) || (c >= 100 && c <= 107))
                bg = std::to_string(c);
            else if (c == 49)
                bg.clear();
            else if (c == 38 || c == 48) {
                string x = std::to_string(c);
                if (k + 2 < v.size() && v[k + 1] == 5) {
                    x += ";5;" + std::to_string(v[k + 2]);
                    k += 2;
                } else if (k + 4 < v.size() && v[k + 1] == 2) {
                    x += ";2;" + std::to_string(v[k + 2]) + ";" + std::to_string(v[k + 3]) + ";" +
                         std::to_string(v[k + 4]);
                    k += 4;
                }
                (c == 38 ? fg : bg) = x;
            }
        }
    }
    bool any() const { return b || d || i || u || s || !fg.empty() || !bg.empty(); }
    string open() const {
        string r;
        auto add = [&](const string &c) { r += "\x1b[" + c + "m"; };
        if (b)
            add("1");
        if (d)
            add("2");
        if (i)
            add("3");
        if (u)
            add("4");
        if (s)
            add("9");
        if (!fg.empty())
            add(fg);
        if (!bg.empty())
            add(bg);
        return r;
    }
};

static void trackSgr(SgrState &st, const string &t) {
    for (size_t i = 0; i + 1 < t.size();) {
        if (t[i] == '\x1b' && t[i + 1] == '[') {
            size_t j = i + 2;
            while (j < t.size() && !((unsigned char)t[j] >= 0x40 && (unsigned char)t[j] <= 0x7E))
                j++;
            if (j < t.size() && t[j] == 'm')
                st.apply(t.substr(i + 2, j - i - 2));
            i = j + 1;
        } else {
            i++;
        }
    }
}

static vector<string> hardSplit(const string &t, int width) {
    vector<string> pieces;
    string cur;
    int cw = 0;
    for (size_t i = 0; i < t.size();) {
        if (t[i] == '\x1b' && i + 1 < t.size() && t[i + 1] == '[') {
            size_t j = i + 2;
            while (j < t.size() && !((unsigned char)t[j] >= 0x40 && (unsigned char)t[j] <= 0x7E))
                j++;
            j++;
            cur += t.substr(i, j - i);
            i = j;
            continue;
        }
        char32_t cp;
        size_t n = decodeUtf8(t, i, cp);
        int w = codepointWidth(cp);
        if (cw + w > width && cw > 0) {
            pieces.push_back(cur);
            cur.clear();
            cw = 0;
        }
        cur += t.substr(i, n);
        cw += w;
        i += n;
    }
    if (!cur.empty())
        pieces.push_back(cur);
    return pieces;
}

// Wrap text (which may contain ANSI SGR codes and '\n' hard breaks) to `width` columns.
// Styles that are open at a line end are closed and re-opened on the next line.
vector<string> wrap(const string &text, int width) {
    if (width < 1)
        width = 1;
    vector<string> out;
    SgrState st;
    vector<string> hard;
    {
        size_t s0 = 0;
        for (;;) {
            size_t e = text.find('\n', s0);
            if (e == npos) {
                hard.push_back(text.substr(s0));
                break;
            }
            hard.push_back(text.substr(s0, e - s0));
            s0 = e + 1;
        }
    }
    for (const string &hl : hard) {
        string cur = st.open();
        int cw = 0;
        bool has = false;
        auto flush = [&]() {
            if (st.any())
                cur += "\x1b[0m";
            out.push_back(cur);
            cur = st.open();
            cw = 0;
            has = false;
        };
        auto process = [&](const string &t) {
            int w = displayWidth(t);
            if (w > width) {
                if (has)
                    flush();
                auto ps = hardSplit(t, width);
                for (size_t k = 0; k < ps.size(); k++) {
                    cur += ps[k];
                    trackSgr(st, ps[k]);
                    if (k + 1 < ps.size())
                        flush();
                    else {
                        cw = displayWidth(ps[k]);
                        has = true;
                    }
                }
                return;
            }
            if (has && cw + 1 + w > width)
                flush();
            if (has) {
                cur += ' ';
                cw++;
            }
            cur += t;
            cw += w;
            has = true;
            trackSgr(st, t);
        };
        size_t i = 0;
        while (i < hl.size()) {
            while (i < hl.size() && hl[i] == ' ')
                i++;
            if (i >= hl.size())
                break;
            size_t j = i;
            while (j < hl.size() && hl[j] != ' ')
                j++;
            process(hl.substr(i, j - i));
            i = j;
        }
        if (has) {
            if (st.any())
                cur += "\x1b[0m";
            out.push_back(cur);
        } else {
            out.push_back("");
        }
    }
    return out;
}
string sanitize(std::string_view in) {
    string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '\n')
            out += '\n';
        else if (c == '\t')
            out += ' ';
        else if (c < 0x20 || c == 0x7F)
            continue;
        else if (c == 0xC2 && i + 1 < in.size() && (unsigned char)in[i + 1] >= 0x80 && (unsigned char)in[i + 1] < 0xA0)
            i++; // C1
        else
            out += (char)c;
    }
    return out;
}

} // namespace text
