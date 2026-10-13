// mdcat - `cat` for Markdown, with Mermaid diagrams drawn as ASCII art.
//
#define _CRT_SECURE_NO_WARNINGS
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
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
        "  -h, --help        show this help\n"
        "  -V, --version     show version\n"
        "\n"
        "Files ending in .mmd or .mermaid are rendered as a single Mermaid diagram.\n"
        "Mermaid support: flowchart/graph, sequenceDiagram, stateDiagram(-v2), pie."
    );
}

} // namespace

constexpr std::string_view app = "mdcat";
constexpr std::string_view version = "1.0.0";

int main(int argc, char **argv) {
    std::vector<std::string> files;
    int width = 0;
    bool forceColor = false, noColor = false, ascii = false, mermaidOn = true, urls = true;
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
    for (auto &path : files) {
        std::string data;
        if (!readAll(path, data)) {
            fprintf(stderr, "mdcat: %s: cannot read file\n", path.c_str());
            rc = 1;
            continue;
        }
        std::string lp = text::toLower(path);
        if (hasSuffix(lp, ".mmd") || hasSuffix(lp, ".mermaid"))
            data = "~~~~mermaid\n" + data + "\n~~~~\n";
        std::string rendered = renderer.render(data);
        if (!first && !rendered.empty())
            fputc('\n', stdout);
        fwrite(rendered.data(), 1, rendered.size(), stdout);
        if (!rendered.empty())
            first = false;
    }
    return rc;
}
