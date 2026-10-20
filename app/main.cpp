// mdcat - `cat` for Markdown, with Mermaid diagrams drawn as ASCII art.
//
#define _CRT_SECURE_NO_WARNINGS
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <string_view>

#include "md/terminal_renderer.h"
#include "mermaid/mermaid.h"
#include "text.h"
#include "ascii/pager.h"

namespace {

bool outIsTty() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(1) != 0;
#endif
}

int terminalWidth() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return info.srWindow.Right - info.srWindow.Left + 1;
#else
    struct winsize w;
    if (ioctl(1, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
        return w.ws_col;
#endif
    if (const char *c = getenv("COLUMNS")) {
        int v = atoi(c);
        if (v > 0)
            return v;
    }
    return 80;
}

bool readAll(const std::string &path, std::string &data) {
    std::ifstream file;
    std::istream *in = &std::cin;
    if (path != "-") {
        file.open(path, std::ios::binary);
        if (!file)
            return false;
        in = &file;
    }
    std::ostringstream ss;
    ss << in->rdbuf();
    data = ss.str();
    if (data.compare(0, 3, "\xEF\xBB\xBF") == 0)
        data.erase(0, 3); // UTF-8 BOM
    return true;
}

bool hasSuffix(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void usage() {
    puts(
        "mdcat - cat for Markdown, with Mermaid diagrams rendered as ASCII art\n"
        "\n"
        "Usage: mdcat [options] [file ...]      (no file, or '-', reads stdin)\n"
        "\n"
        "Options:\n"
        "  -w, --width N     wrap output at N columns (default: terminal width, max 100)\n"
        "  -a, --ascii       use plain ASCII instead of Unicode box drawing\n"
        "  -c, --color       force ANSI colors even when not writing to a terminal\n"
        "  -C, --no-color    disable ANSI colors (NO_COLOR is also honoured)\n"
        "  -m, --no-mermaid  show mermaid blocks as source instead of rendering them\n"
        "  -u, --no-urls     do not print link targets after link text\n"
        "  -p, --pager       use pager mode (default, can be disabled with --no-pager)\n"
        "  --no-pager        disable pager mode\n"
        "  -h, --help        show this help\n"
        "  -V, --version     show version\n"
        "\n"
        "Pager Controls:\n"
        "  ↑ / Ctrl+P        scroll up\n"
        "  ↓ / Ctrl+N        scroll down\n"
        "  Page Up           scroll page up\n"
        "  Page Down         scroll page down\n"
        "  Home / Ctrl+A     jump to top\n"
        "  End / Ctrl+E      jump to bottom\n"
        "  q                 quit pager\n"
        "  Ctrl+C            quit pager\n"
        "\n"
        "Files ending in .mmd or .mermaid are rendered as a single Mermaid diagram.\n"
        "Mermaid support: flowchart/graph, sequenceDiagram, stateDiagram(-v2), pie."
    );
}

} // namespace

constexpr std::string_view app = "mdcat";
constexpr std::string_view version = "1.0.0";

// Blocks until a key is pressed. Returns the raw key bytes; special keys are
// returned as ANSI escape sequences (e.g. "\x1b[A" for Up).
std::string get_key() {
#ifdef _WIN32
    int c = _getch();
    if (c == 0 || c == 0xE0) {          // extended key: prefix + scan code
        switch (_getch()) {
            case 72: return "\x1b[A";       // Up
            case 80: return "\x1b[B";       // Down
            case 73: return "\x1b[5~";      // Page Up
            case 81: return "\x1b[6~";      // Page Down
            case 71: return "\x1b[H";       // Home
            case 79: return "\x1b[F";       // End
        default: return std::string();  // ignore other extended keys
        }
    }
    return std::string(1, static_cast<char>(c)); // 'q', Ctrl+C (0x03), Ctrl+P, ...
#else
    struct termios oldt;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0)
        return std::string();

    struct termios newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO | ISIG); // ISIG off: Ctrl+C arrives as "\x03"
    newt.c_cc[VMIN] = 1;                     // block until at least 1 byte
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char buf[8] = {0};
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf)); // returns whatever one keypress produced

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return n > 0 ? std::string(buf, static_cast<size_t>(n)) : std::string();
#endif
}

std::vector<std::string> splitLines(const std::string &s) {
    std::vector<std::string> out;
    std::string::size_type pos = 0;
    while (pos <= s.size()) {
        auto nl = s.find('\n', pos);
        if (nl == std::string::npos) {
            if (pos < s.size())
                out.push_back(s.substr(pos));
            break;
        }
        std::string line = s.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        out.push_back(std::move(line));
        pos = nl + 1;
    }
    return out;
}

int main(int argc, char **argv) {
    std::vector<std::string> files;
    int width = 0;
    bool forceColor = false, noColor = false, ascii = false, mermaidOn = true, urls = true, usePager = true;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--") {
            for (i++; i < argc; i++)
                files.push_back(argv[i]);
            break;
        }
        if (a == "-h" || a == "--help") {
            usage();
            return 0;
        }
        if (a == "-V" || a == "--version") {
            std::cout << app << " " << version;
            return 0;
        }
        if (a == "-w" || a == "--width") {
            if (i + 1 >= argc) {
                fprintf(stderr, "mdcat: option '%s' needs a value\n", a.c_str());
                return 2;
            }
            width = atoi(argv[++i]);
        } else if (text::startsWith(a, "--width="))
            width = atoi(a.c_str() + 8);
        else if (a == "-c" || a == "--color")
            forceColor = true;
        else if (a == "-C" || a == "--no-color")
            noColor = true;
        else if (a == "-a" || a == "--ascii")
            ascii = true;
        else if (a == "-m" || a == "--no-mermaid")
            mermaidOn = false;
        else if (a == "-u" || a == "--no-urls")
            urls = false;
        else if (a == "-p" || a == "--pager")
            usePager = true;
        else if (a == "--no-pager")
            usePager = false;
        else if (a == "-")
            files.push_back(a);
        else if (a.size() > 1 && a[0] == '-') {
            fprintf(stderr, "mdcat: unknown option '%s' (try --help)\n", a.c_str());
            return 2;
        } else
            files.push_back(a);
    }
    if (files.empty()) {
        files.push_back("-");
    }

    const bool tty = outIsTty();
    md::RenderOptions opt;
    opt.color = !noColor && (forceColor || (tty && !getenv("NO_COLOR")));
    opt.unicode = !ascii;
    opt.showUrls = urls;
    opt.width = width > 0 ? width : terminalWidth();
#ifdef _WIN32
    if (tty) {
        SetConsoleOutputCP(CP_UTF8);
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        if (GetConsoleMode(h, &mode))
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    md::TerminalRenderer renderer(opt);
    if (mermaidOn)
        renderer.onFence("mermaid", [&opt](const std::string &code, int availableWidth) {
            mermaid::Options mo;
            mo.maxWidth = availableWidth;
            mo.unicode = opt.unicode;
            return mermaid::render(code, mo);
        });

    int rc = 0;
    bool first = true;
    std::vector<std::string> all_lines;
    
    for (auto &path : files) {
        std::string data;
        if (!readAll(path, data)) {
            fprintf(stderr, "mdcat: %s: cannot read file\n", path.c_str());
            rc = 1;
            continue;
        }
        std::string lp = text::toLower(path);
        if (hasSuffix(lp, ".mmd") || hasSuffix(lp, ".mermaid"))
            data = "~~~~mermaid\n" + data + "~~~~\n";
        std::string rendered = renderer.render(data);
        if (!rendered.empty()) {
            if (!first)
                all_lines.push_back("");          // separator goes *before* the next file
            auto lines = splitLines(rendered);
            all_lines.insert(all_lines.end(),
                             std::make_move_iterator(lines.begin()),
                             std::make_move_iterator(lines.end()));
            first = false;
        }
    }
    
    if (all_lines.empty()) {
        fprintf(stderr, "mdcat: no content to display\n");
        return 1;
    }
    
    if (usePager) {
        ascii::init();
        ascii::PagerState state;
        state.lines = std::move(all_lines);
        state.start = 0;
        state.end = std::max<long long>(0LL, state.lines.size() - 1);
        state.at_top = true;
        state.at_bottom = false;
        
        ascii::PagerOptions pager_opts;
        pager_opts.wrap_at_edge = true;
        pager_opts.show_cursor = true;
        
        bool quit = false;
        
        while (!quit) {
            ascii::render(state, pager_opts);
            
            std::string key = get_key();
            bool should_quit = false;
            ascii::Key pressed = ascii::handle_key(state, key, should_quit);
            if (should_quit || pressed == ascii::Key::QUIT) {
                quit = true;
            }
        }
        
        ascii::cleanup();
    } else {
        for (const auto &line : all_lines) {
            std::cout << line << "\n";
        }
    }
    
    return rc;
}
