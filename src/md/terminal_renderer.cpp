#include "md/terminal_renderer.h"
#include "ascii/glyphs.h"
#include "text.h"

#include <md4c.h>

#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <utility>

namespace md {

namespace {

// keeps inline code from wrapping mid-span
const char *const kNbsp = "\xC2\xA0";
const char *const kReplacement = "\xEF\xBF\xBD";

std::string attrText(const MD_ATTRIBUTE &a) { return std::string(a.text ? a.text : "", a.size); }

std::string decodeEntity(std::string_view e) {
    if (e.size() > 3 && e[1] == '#') {
        bool hex = e[2] == 'x' || e[2] == 'X';
        std::string digits(e.substr(hex ? 3 : 2, e.size() - (hex ? 4 : 3)));
        unsigned long cp = strtoul(digits.c_str(), nullptr, hex ? 16 : 10);
        if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return kReplacement;
        return text::utf8((char32_t)cp);
    }
    static const std::pair<const char *, const char *> named[] = {
        {"&amp;", "&"},
        {"&lt;", "<"},
        {"&gt;", ">"},
        {"&quot;", "\""},
        {"&apos;", "'"},
        {"&nbsp;", kNbsp},
        {"&copy;", "\xC2\xA9"},
        {"&reg;", "\xC2\xAE"},
        {"&trade;", "\xE2\x84\xA2"},
        {"&hellip;", "\xE2\x80\xA6"},
        {"&mdash;", "\xE2\x80\x94"},
        {"&ndash;", "\xE2\x80\x93"},
        {"&lsquo;", "\xE2\x80\x98"},
        {"&rsquo;", "\xE2\x80\x99"},
        {"&ldquo;", "\xE2\x80\x9C"},
        {"&rdquo;", "\xE2\x80\x9D"},
        {"&bull;", "\xE2\x80\xA2"},
        {"&middot;", "\xC2\xB7"},
        {"&deg;", "\xC2\xB0"},
        {"&times;", "\xC3\x97"},
        {"&laquo;", "\xC2\xAB"},
        {"&raquo;", "\xC2\xBB"}
    };
    for (auto &n : named)
        if (e == n.first)
            return n.second;
    return std::string(e);
}

// Reduce raw HTML to readable text: drop tags and comments, <br> -> newline, <img> -> [image: alt].
std::string htmlToText(std::string_view s) {
    std::string out;
    for (size_t i = 0; i < s.size();) {
        if (s[i] != '<') {
            out += s[i++];
            continue;
        }
        if (s.compare(i, 4, "<!--") == 0) {
            size_t e = s.find("-->", i);
            i = e == std::string_view::npos ? s.size() : e + 3;
            continue;
        }
        size_t e = s.find('>', i);
        if (e == std::string_view::npos) {
            out += s[i++];
            continue;
        }
        std::string tag = text::toLower(std::string(s.substr(i, e - i + 1)));
        if (text::startsWith(tag, "<br")) {
            out += '\n';
        } else if (text::startsWith(tag, "<img")) {
            size_t a = tag.find("alt=\"");
            std::string alt;
            if (a != std::string::npos) {
                size_t q = tag.find('"', a + 5);
                if (q != std::string::npos)
                    alt = std::string(s.substr(i + a + 5, q - a - 5));
            }
            out += "[image" + (alt.empty() ? std::string() : ": " + alt) + "]";
        }
        i = e + 1;
    }
    return out;
}

enum class Leaf { None, Paragraph, Heading, Code, Html, Table, Cell };

struct StyleEntry {
    std::string sgr;    // SGR parameters used when colour is on
    std::string marker; // literal marker used when colour is off (e.g. "**")
};

// A container that contributes a prefix to every line inside it.
struct Scope {
    enum class Kind { Document, Quote, Item } kind = Kind::Document;
    std::string marker; // Item: styled bullet/number, printed on the item's first line only
    bool markerPending = false;
    int indent = 0;     // columns the prefix occupies
    bool tight = false; // Item of a tight list: no blank lines between its blocks
    int emitted = 0;    // blocks emitted so far inside this scope
};

struct ListState {
    bool ordered;
    unsigned next;
    bool tight;
    int items = 0;
};

struct TableData {
    std::vector<std::vector<std::string>> rows; // rows[0] is the header; cells hold ANSI text
    std::vector<char> align;                    // 'l' 'c' 'r' per column
};

} // namespace

// ============================================================================
// Impl: receives md4c events
// ============================================================================
class TerminalRenderer::Impl {
  public:
    explicit Impl(RenderOptions o) : opt_(o), g_(ascii::Glyphs::get(o.unicode)) {}

    void addFence(std::string lang, FenceHandler h) { fences_[text::toLower(std::move(lang))] = std::move(h); }

    std::string render(std::string_view md) {
        reset();
        std::string_view body = md;
        // YAML front matter is not Markdown; show it as a labelled block.
        if (md.substr(0, 4) == "---\n") {
            size_t e = md.find("\n---", 3);
            if (e != std::string_view::npos && (e + 4 == md.size() || md[e + 4] == '\n' || md[e + 4] == '\r')) {
                emitCodeBox(text::splitLines(text::sanitize(md.substr(4, e - 4))), "front matter");
                scopes_.back().emitted++;
                size_t rest = e + 4;
                while (rest < md.size() && (md[rest] == '\n' || md[rest] == '\r'))
                    rest++;
                body = md.substr(rest);
            }
        }

        MD_PARSER parser{};
        parser.abi_version = 0;
        parser.flags = MD_DIALECT_GITHUB; // tables, strikethrough, task lists, permissive autolinks
        parser.enter_block = &Impl::onEnterBlock;
        parser.leave_block = &Impl::onLeaveBlock;
        parser.enter_span = &Impl::onEnterSpan;
        parser.leave_span = &Impl::onLeaveSpan;
        parser.text = &Impl::onText;
        parser.debug_log = nullptr;
        parser.syntax = nullptr;
        md_parse(body.data(), (MD_SIZE)body.size(), &parser, this);

        std::string result;
        for (auto &l : out_)
            result += l + "\n";
        return result;
    }

  private:
    // ---- md4c trampolines ------------------------------------------------------------
    static int onEnterBlock(MD_BLOCKTYPE t, void *d, void *u) { return static_cast<Impl *>(u)->enterBlock(t, d); }
    static int onLeaveBlock(MD_BLOCKTYPE t, void *d, void *u) { return static_cast<Impl *>(u)->leaveBlock(t, d); }
    static int onEnterSpan(MD_SPANTYPE t, void *d, void *u) { return static_cast<Impl *>(u)->enterSpan(t, d); }
    static int onLeaveSpan(MD_SPANTYPE t, void *d, void *u) { return static_cast<Impl *>(u)->leaveSpan(t, d); }
    static int onText(MD_TEXTTYPE t, const MD_CHAR *s, MD_SIZE n, void *u) {
        return static_cast<Impl *>(u)->onTextChunk(t, s, n);
    }

    // ---- state -----------------------------------------------------------------------
    RenderOptions opt_;
    const ascii::Glyphs &g_;
    std::unordered_map<std::string, FenceHandler> fences_;

    std::vector<std::string> out_;
    std::vector<Scope> scopes_;
    std::vector<ListState> lists_;

    Leaf leaf_ = Leaf::None;
    bool implicit_ = false; // paragraph opened implicitly (md4c omits <p> in tight list items)
    std::string inl_;       // inline text of the current leaf, with ANSI codes
    std::vector<StyleEntry> styles_;

    int headingLevel_ = 0;
    std::string code_, codeLang_, html_;
    TableData table_;
    bool cellHead_ = false;

    bool inCodeSpan_ = false, inLink_ = false;
    std::string linkHref_, linkPlain_;
    int imgDepth_ = 0;
    std::string alt_, imgSrc_;

    void reset() {
        out_.clear();
        scopes_.assign(1, Scope{});
        lists_.clear();
        leaf_ = Leaf::None;
        implicit_ = false;
        inl_.clear();
        styles_.clear();
        inCodeSpan_ = inLink_ = false;
        imgDepth_ = 0;
    }

    // ---- styling ---------------------------------------------------------------------
    std::string paint(const std::string &sgr, const std::string &s) const {
        return opt_.color ? "\x1b[" + sgr + "m" + s + "\x1b[0m" : s;
    }
    void pushStyle(const std::string &sgr, const std::string &marker) {
        styles_.push_back({sgr, marker});
        inl_ += opt_.color ? "\x1b[" + sgr + "m" : marker;
    }
    void popStyle() {
        StyleEntry e = styles_.back();
        styles_.pop_back();
        if (!opt_.color) {
            inl_ += e.marker;
            return;
        }
        inl_ += "\x1b[0m"; // reset, then re-open whatever is still active
        for (auto &s : styles_)
            inl_ += "\x1b[" + s.sgr + "m";
    }

    // ---- output with container prefixes ----------------------------------------------
    int available() const {
        int w = opt_.width;
        for (auto &s : scopes_)
            w -= s.indent;
        return std::max(w, 10);
    }
    std::string prefix(bool consumeMarkers) {
        std::string p;
        for (size_t i = 1; i < scopes_.size(); i++) {
            Scope &s = scopes_[i];
            if (s.kind == Scope::Kind::Quote) {
                p += paint("2;36", text::utf8(g_.vline)) + " ";
            } else if (s.markerPending && consumeMarkers) {
                p += s.marker;
                s.markerPending = false;
            } else {
                p += std::string(s.indent, ' ');
            }
        }
        return p;
    }
    void emit(const std::string &content) { out_.push_back(prefix(true) + content); }
    void emitBlank() {
        std::string p = prefix(false);
        while (!p.empty() && p.back() == ' ')
            p.pop_back();
        out_.push_back(p);
    }
    // Called before every top-level block of a scope: blank line between blocks (not in tight items).
    void beginBlock() {
        Scope &s = scopes_.back();
        if (s.emitted > 0 && !s.tight)
            emitBlank();
        s.emitted++;
    }

    // ---- block events ----------------------------------------------------------------
    int enterBlock(MD_BLOCKTYPE type, void *detail) {
        flushImplicit();
        switch (type) {
        case MD_BLOCK_DOC:
            break;
        case MD_BLOCK_QUOTE: {
            beginBlock();
            Scope s;
            s.kind = Scope::Kind::Quote;
            s.indent = 2;
            scopes_.push_back(s);
            break;
        }
        case MD_BLOCK_UL: {
            beginBlock();
            lists_.push_back({false, 1, static_cast<MD_BLOCK_UL_DETAIL *>(detail)->is_tight != 0});
            break;
        }
        case MD_BLOCK_OL: {
            auto *d = static_cast<MD_BLOCK_OL_DETAIL *>(detail);
            beginBlock();
            lists_.push_back({true, d->start, d->is_tight != 0});
            break;
        }
        case MD_BLOCK_LI:
            enterItem(static_cast<MD_BLOCK_LI_DETAIL *>(detail));
            break;
        case MD_BLOCK_HR:
            beginBlock();
            emit(paint("2", text::repeat(text::utf8(g_.hline), std::min(available(), 72))));
            break;
        case MD_BLOCK_H: {
            static const char *const colors[] = {"", "1;35", "1;36", "1;33", "1;32", "1;34", "1;37"};
            headingLevel_ = (int)static_cast<MD_BLOCK_H_DETAIL *>(detail)->level;
            headingLevel_ = std::min(std::max(headingLevel_, 1), 6);
            beginBlock();
            startLeaf(Leaf::Heading);
            pushStyle(colors[headingLevel_], "");
            if (headingLevel_ >= 3)
                inl_ += std::string(headingLevel_, '#') + " ";
            break;
        }
        case MD_BLOCK_CODE: {
            auto *d = static_cast<MD_BLOCK_CODE_DETAIL *>(detail);
            beginBlock();
            startLeaf(Leaf::Code);
            codeLang_ = text::sanitize(attrText(d->lang.size ? d->lang : d->info));
            codeLang_ = codeLang_.substr(0, codeLang_.find(' '));
            code_.clear();
            break;
        }
        case MD_BLOCK_HTML:
            startLeaf(Leaf::Html);
            html_.clear();
            break;
        case MD_BLOCK_P:
            beginBlock();
            startLeaf(Leaf::Paragraph);
            break;
        case MD_BLOCK_TABLE:
            beginBlock();
            startLeaf(Leaf::Table);
            table_ = TableData{};
            break;
        case MD_BLOCK_THEAD:
        case MD_BLOCK_TBODY:
            break;
        case MD_BLOCK_TR:
            table_.rows.emplace_back();
            break;
        case MD_BLOCK_TH:
        case MD_BLOCK_TD:
            enterCell(type == MD_BLOCK_TH, static_cast<MD_BLOCK_TD_DETAIL *>(detail));
            break;
        }
        return 0;
    }

    int leaveBlock(MD_BLOCKTYPE type, void *) {
        switch (type) {
        case MD_BLOCK_DOC:
            flushImplicit();
            break;
        case MD_BLOCK_QUOTE:
            scopes_.pop_back();
            break;
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            lists_.pop_back();
            break;
        case MD_BLOCK_LI:
            flushImplicit();
            scopes_.pop_back();
            break;
        case MD_BLOCK_P:
            finishParagraph();
            break;
        case MD_BLOCK_H:
            finishHeading();
            break;
        case MD_BLOCK_CODE:
            finishCode();
            break;
        case MD_BLOCK_HTML:
            finishHtml();
            break;
        case MD_BLOCK_TABLE:
            emitTable();
            leaf_ = Leaf::None;
            break;
        case MD_BLOCK_TH:
        case MD_BLOCK_TD:
            if (cellHead_)
                popStyle();
            table_.rows.back().push_back(inl_);
            inl_.clear();
            leaf_ = Leaf::Table;
            break;
        default:
            break;
        }
        return 0;
    }

    void startLeaf(Leaf l) {
        leaf_ = l;
        inl_.clear();
        styles_.clear();
    }

    // md4c omits <p> inside tight list items; text then arrives directly in the item.
    void ensureLeaf() {
        if (leaf_ != Leaf::None)
            return;
        beginBlock();
        startLeaf(Leaf::Paragraph);
        implicit_ = true;
    }
    void flushImplicit() {
        if (implicit_)
            finishParagraph();
    }

    void enterItem(const MD_BLOCK_LI_DETAIL *d) {
        ListState &list = lists_.back();
        if (list.items > 0 && !list.tight)
            emitBlank();
        list.items++;
        std::string marker;
        if (d->is_task) {
            bool done = d->task_mark != ' ';
            marker = paint(done ? "32" : "2", done ? "[x]" : "[ ]");
        } else if (list.ordered) {
            marker = paint("33", std::to_string(list.next) + ".");
        } else {
            marker = paint("33", text::utf8(g_.bullets[(lists_.size() - 1) % 3]));
        }
        if (list.ordered)
            list.next++;
        Scope s;
        s.kind = Scope::Kind::Item;
        s.tight = list.tight;
        s.indent = text::displayWidth(marker) + 1;
        s.marker = marker + " ";
        s.markerPending = true;
        scopes_.push_back(s);
    }

    void enterCell(bool head, const MD_BLOCK_TD_DETAIL *d) {
        leaf_ = Leaf::Cell;
        inl_.clear();
        styles_.clear();
        cellHead_ = head;
        if (head)
            pushStyle("1", "");
        if (table_.rows.size() == 1) {
            char a = d->align == MD_ALIGN_CENTER ? 'c' : d->align == MD_ALIGN_RIGHT ? 'r' : 'l';
            table_.align.push_back(a);
        }
    }

    // ---- leaf completion -------------------------------------------------------------
    void finishParagraph() {
        if (!text::trim(inl_).empty())
            for (auto &l : text::wrap(inl_, available()))
                emit(l);
        leaf_ = Leaf::None;
        implicit_ = false;
        inl_.clear();
        styles_.clear();
    }

    void finishHeading() {
        static const char *const colors[] = {"", "1;35", "1;36", "1;33", "1;32", "1;34", "1;37"};
        popStyle();
        auto lines = text::wrap(inl_, available());
        int w = 0;
        for (auto &l : lines) {
            emit(l);
            w = std::max(w, text::displayWidth(l));
        }
        if (headingLevel_ <= 2) {
            char32_t ch = headingLevel_ == 1 ? g_.heavyHline : g_.hline;
            emit(paint(colors[headingLevel_], text::repeat(text::utf8(ch), w)));
        }
        leaf_ = Leaf::None;
        inl_.clear();
        styles_.clear();
    }

    void finishCode() {
        std::string code = text::sanitize(code_);
        while (!code.empty() && code.back() == '\n')
            code.pop_back();
        std::string label = codeLang_;
        auto it = fences_.find(text::toLower(codeLang_));
        if (it != fences_.end()) {
            if (auto art = it->second(code, available())) {
                for (auto &l : *art)
                    emit(paint("36", l));
                leaf_ = Leaf::None;
                return;
            }
            label += " (could not render; showing source)";
        }
        emitCodeBox(text::splitLines(code), label);
        leaf_ = Leaf::None;
    }

    void finishHtml() {
        std::string t = text::trim(text::sanitize(htmlToText(html_)));
        if (!t.empty()) {
            beginBlock();
            for (auto &l : text::wrap(t, available()))
                emit(l);
        }
        leaf_ = Leaf::None;
    }

    void emitCodeBox(const std::vector<std::string> &lines, const std::string &label) {
        int widest = 0;
        std::vector<std::string> body;
        for (auto &l : lines) {
            body.push_back(text::expandTabs(l));
            widest = std::max(widest, text::displayWidth(body.back()));
        }
        int rule = std::max((int)label.size() + 6, std::min(available(), widest + 4));
        std::string h = text::utf8(g_.hline);
        // Top: ┌─ label ─┐ (or ──────── when no label)
        std::string top;
        if (label.empty()) {
            top = text::utf8(g_.junction(ascii::kDown | ascii::kRight)) + text::repeat(h, rule - 2) +
                  text::utf8(g_.junction(ascii::kDown | ascii::kLeft));
        } else {
            std::string head = text::utf8(g_.junction(ascii::kDown | ascii::kRight)) + h + " " + label + " ";
            int topDashCount = std::max(0, rule - 1 - (int)text::displayWidth(head));
            top = head + text::repeat(h, topDashCount) +
                  text::utf8(g_.junction(ascii::kDown | ascii::kLeft));
        }
        emit(paint("2", top));
        std::string vline = text::utf8(g_.vline);
        for (auto &l : body) {
            int w = text::displayWidth(l);
            if (w < widest)
                l += std::string(widest - w, ' ');
            emit(paint("2", vline + " ") + l + " " + vline);
        }
        // Bottom: └─┘
        int bottomDashCount = rule - 2; // minus both corners
        std::string bot = text::utf8(g_.junction(ascii::kUp | ascii::kRight)) + text::repeat(h, bottomDashCount) +
                          text::utf8(g_.junction(ascii::kUp | ascii::kLeft));
        emit(paint("2", bot));
    }

    // ---- tables ----------------------------------------------------------------------
    void emitTable() {
        const size_t nc = table_.align.size();
        if (nc == 0 || table_.rows.empty())
            return;
        for (auto &r : table_.rows)
            r.resize(nc);

        std::vector<int> natural(nc, 1), minimum(nc, 3);
        for (auto &r : table_.rows)
            for (size_t c = 0; c < nc; c++) {
                for (auto &seg : text::splitLines(r[c] + "\n"))
                    natural[c] = std::max(natural[c], text::displayWidth(seg));
                size_t i = 0;
                const std::string &s = r[c];
                while (i < s.size()) {
                    while (i < s.size() && (s[i] == ' ' || s[i] == '\n'))
                        i++;
                    size_t j = i;
                    while (j < s.size() && s[j] != ' ' && s[j] != '\n')
                        j++;
                    if (j > i)
                        minimum[c] = std::max(minimum[c], std::min(text::displayWidth(s.substr(i, j - i)), 10));
                    i = j;
                }
            }
        std::vector<int> w = natural;
        const int budget = available() - (int)(nc * 3 + 1);
        auto total = [&] {
            int t = 0;
            for (int x : w)
                t += x;
            return t;
        };
        auto shrink = [&](const std::vector<int> &floor) {
            while (total() > budget) {
                int best = -1;
                for (size_t c = 0; c < nc; c++)
                    if (w[c] > floor[c] && (best < 0 || w[c] > w[best]))
                        best = (int)c;
                if (best < 0)
                    break;
                w[best]--;
            }
        };
        shrink(minimum);
        shrink(std::vector<int>(nc, 3));

        using namespace ascii;
        auto J = [&](uint8_t m) { return text::utf8(g_.junction(m)); };
        auto border = [&](uint8_t left, uint8_t mid, uint8_t right) {
            std::string s = J(left);
            for (size_t c = 0; c < nc; c++)
                s += text::repeat(text::utf8(g_.hline), w[c] + 2) + J(c + 1 < nc ? mid : right);
            return paint("2", s);
        };
        const std::string bar = paint("2", text::utf8(g_.vline));

        emit(border(kDown | kRight, kDown | kLeft | kRight, kDown | kLeft));
        for (size_t r = 0; r < table_.rows.size(); r++) {
            std::vector<std::vector<std::string>> cells;
            size_t height = 1;
            for (size_t c = 0; c < nc; c++) {
                cells.push_back(text::wrap(table_.rows[r][c], w[c]));
                height = std::max(height, cells[c].size());
            }
            for (size_t k = 0; k < height; k++) {
                std::string line = bar;
                for (size_t c = 0; c < nc; c++) {
                    std::string t = k < cells[c].size() ? cells[c][k] : "";
                    int pad = std::max(0, w[c] - text::displayWidth(t));
                    int left = table_.align[c] == 'r' ? pad : table_.align[c] == 'c' ? pad / 2 : 0;
                    line += " " + std::string(left, ' ') + t + std::string(pad - left, ' ') + " " + bar;
                }
                emit(line);
            }
            if (r == 0)
                emit(border(kUp | kDown | kRight, kUp | kDown | kLeft | kRight, kUp | kDown | kLeft));
        }
        emit(border(kUp | kRight, kUp | kLeft | kRight, kUp | kLeft));
    }

    // ---- span events -----------------------------------------------------------------
    int enterSpan(MD_SPANTYPE type, void *detail) {
        ensureLeaf();
        if (imgDepth_ > 0 && type != MD_SPAN_IMG)
            return 0; // alt text is plain
        switch (type) {
        case MD_SPAN_EM:
            pushStyle("3", "*");
            break;
        case MD_SPAN_STRONG:
            pushStyle("1", "**");
            break;
        case MD_SPAN_DEL:
            pushStyle("9", "~~");
            break;
        case MD_SPAN_U:
            pushStyle("4", "_");
            break;
        case MD_SPAN_CODE:
            inCodeSpan_ = true;
            pushStyle("38;5;215", "`");
            break;
        case MD_SPAN_A:
            linkHref_ = text::sanitize(attrText(static_cast<MD_SPAN_A_DETAIL *>(detail)->href));
            linkPlain_.clear();
            inLink_ = true;
            pushStyle("4;34", "");
            break;
        case MD_SPAN_IMG:
            if (imgDepth_++ == 0) {
                imgSrc_ = text::sanitize(attrText(static_cast<MD_SPAN_IMG_DETAIL *>(detail)->src));
                alt_.clear();
            }
            break;
        default:
            break;
        }
        return 0;
    }

    int leaveSpan(MD_SPANTYPE type, void *) {
        if (imgDepth_ > 0 && type != MD_SPAN_IMG)
            return 0;
        switch (type) {
        case MD_SPAN_EM:
        case MD_SPAN_STRONG:
        case MD_SPAN_DEL:
        case MD_SPAN_U:
            popStyle();
            break;
        case MD_SPAN_CODE:
            inCodeSpan_ = false;
            popStyle();
            break;
        case MD_SPAN_A: {
            popStyle();
            inLink_ = false;
            std::string shown = linkHref_;
            if (text::startsWith(shown, "mailto:"))
                shown = shown.substr(7);
            if (opt_.showUrls && !linkHref_.empty() && shown != linkPlain_ && linkHref_[0] != '#') {
                inl_ += " (";
                pushStyle("2;4", "");
                inl_ += linkHref_;
                popStyle();
                inl_ += ")";
            }
            break;
        }
        case MD_SPAN_IMG:
            if (--imgDepth_ == 0) {
                pushStyle("2", "");
                inl_ += "[image: " + (alt_.empty() ? imgSrc_ : alt_) + "]";
                popStyle();
            }
            break;
        default:
            break;
        }
        return 0;
    }

    // ---- text ------------------------------------------------------------------------
    int onTextChunk(MD_TEXTTYPE type, const MD_CHAR *s, MD_SIZE n) {
        if (leaf_ == Leaf::Code) {
            code_.append(s, n);
            return 0;
        }
        if (leaf_ == Leaf::Html) {
            html_.append(s, n);
            return 0;
        }
        ensureLeaf();
        std::string piece;
        switch (type) {
        case MD_TEXT_NULLCHAR:
            piece = kReplacement;
            break;
        case MD_TEXT_BR:
            piece = "\n";
            break;
        case MD_TEXT_SOFTBR:
            piece = " ";
            break;
        case MD_TEXT_ENTITY:
            piece = decodeEntity(std::string_view(s, n));
            break;
        case MD_TEXT_HTML:
            piece = htmlToText(std::string_view(s, n));
            break;
        default:
            piece.assign(s, n);
            break;
        }
        piece = text::sanitize(piece); // untrusted input must never reach the terminal as control codes
        if (imgDepth_ > 0) {
            alt_ += piece;
            return 0;
        }
        if (inLink_)
            linkPlain_ += piece;
        if (inCodeSpan_) {
            std::string nb;
            for (char c : piece)
                nb += c == ' ' ? std::string(kNbsp) : std::string(1, c);
            piece = nb;
        }
        inl_ += piece;
        return 0;
    }
};

TerminalRenderer::TerminalRenderer(RenderOptions options) : impl_(std::make_unique<Impl>(options)) {}
TerminalRenderer::~TerminalRenderer() = default;
void TerminalRenderer::onFence(std::string language, FenceHandler handler) {
    impl_->addFence(std::move(language), std::move(handler));
}
std::string TerminalRenderer::render(std::string_view markdown) { return impl_->render(markdown); }

} // namespace md
