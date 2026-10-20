#include "ascii/pager.h"

#include "text.h"

#include <algorithm>
#include <iostream>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace ascii {

namespace {

constexpr int kStatusLines = 1;

#ifdef _WIN32
DWORD g_old_out_mode = 0;
UINT g_old_cp = 0;
bool g_mode_saved = false;
#endif

int page_height() {
    TerminalSize t = get_terminal_size();
    return std::max(1, static_cast<int>(t.height) - kStatusLines);
}

// Keep start in range and recompute end / at_top / at_bottom.
void clamp(PagerState& s) {
    const size_t ph = static_cast<size_t>(page_height());
    const size_t n = s.lines.size();
    const size_t max_start = n > ph ? n - ph : 0;
    size_t start = std::min<size_t>(static_cast<size_t>(s.start), max_start);
    s.start = start;
    s.end = n == 0 ? 0 : std::min(start + ph, n) - 1;
    s.at_top = (start == 0);
    s.at_bottom = (start >= max_start);
}

} // anonymous namespace

void init() {
#ifdef _WIN32
    g_old_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(h, &g_old_out_mode)) {
        g_mode_saved = true;
        SetConsoleMode(h, g_old_out_mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
    // Alternate screen buffer + clear
    std::cout << "\x1b[?1049h\x1b[2J\x1b[H" << std::flush;
}

void cleanup() {
    disable_mouse();
    // Reset attributes, show cursor, leave alternate screen
    std::cout << "\x1b[0m\x1b[?25h\x1b[?1049l" << std::flush;
#ifdef _WIN32
    if (g_mode_saved)
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), g_old_out_mode);
    if (g_old_cp)
        SetConsoleOutputCP(g_old_cp);
#endif
}

bool can_use_mouse() {
#ifdef _WIN32
    return false;
#else
    return true;
#endif
}

void enable_mouse() {
    if (can_use_mouse())
        std::cout << "\x1b[?1000h" << std::flush; // h = enable
}

void disable_mouse() {
    if (can_use_mouse())
        std::cout << "\x1b[?1000l" << std::flush; // l = disable
}

TerminalSize get_terminal_size() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        // srWindow = visible window; dwSize would be the whole scrollback buffer
        int w = info.srWindow.Right - info.srWindow.Left + 1;
        int h = info.srWindow.Bottom - info.srWindow.Top + 1;
        return {w, h};
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
        return {ws.ws_col, ws.ws_row};
#endif
    return {80, 24};
}

void clear_screen() {
    std::cout << "\x1b[0m\x1b[2J\x1b[H" << std::flush;
}

void render(PagerState& state, const PagerOptions& opts) {
    TerminalSize term = get_terminal_size();
    const int ph = std::max(1, static_cast<int>(term.height) - kStatusLines);
    const int width = static_cast<int>(term.width);

    clamp(state);

    std::string out;
    out += "\x1b[H"; // home; overwrite in place instead of clearing (no flicker)
    out += opts.show_cursor ? "\x1b[?25h" : "\x1b[?25l";

    for (int row = 0; row < ph; ++row) {
        size_t i = static_cast<size_t>(state.start) + static_cast<size_t>(row);
        if (i < state.lines.size()) {
            std::string line = state.lines[i];
            if (opts.wrap_at_edge && text::displayWidth(line) > width) {
                auto wrapped = text::wrap(line, width);
                if (!wrapped.empty())
                    line = wrapped[0]; // note: this truncates rather than wraps
            }
            out += line;
        }
        out += "\x1b[0m\x1b[K\r\n"; // reset colours, clear to end of line
    }
    std::cout << out;

    render_status_bar(state);
}

void render_status_bar(const PagerState& state) {
    TerminalSize term = get_terminal_size();
    const size_t total = state.lines.size();
    const size_t first = total ? static_cast<size_t>(state.start) + 1 : 0;
    const size_t last = total ? static_cast<size_t>(state.end) + 1 : 0;
    const size_t pct = (total == 0 || state.at_bottom) ? 100 : last * 100 / total;

    std::string bar = " Lines " + std::to_string(first) + "-" + std::to_string(last) + " of " +
                      std::to_string(total) + " (" + std::to_string(pct) + "%)" +
                      "   q: quit | Up/Down/PgUp/PgDn/Home/End: scroll";

    const size_t w = static_cast<size_t>(term.width);
    if (bar.size() > w)
        bar.resize(w);
    bar.append(w - bar.size(), ' ');

    std::cout << "\x1b[" << term.height << ";1H" // last row
              << "\x1b[7m" << bar << "\x1b[0m" << std::flush;
}

void scroll_up(PagerState& state) {
    if (state.start > 0)
        state.start = state.start - 1;
    clamp(state);
}

void scroll_down(PagerState& state) {
    state.start = state.start + 1;
    clamp(state);
}

void page_up(PagerState& state) {
    const size_t ph = static_cast<size_t>(page_height());
    size_t start = static_cast<size_t>(state.start);
    state.start = start > ph ? start - ph : 0;
    clamp(state);
}

void page_down(PagerState& state) {
    state.start = static_cast<size_t>(state.start) + static_cast<size_t>(page_height());
    clamp(state);
}

void jump_to_top(PagerState& state) {
    state.start = 0;
    clamp(state);
}

void jump_to_bottom(PagerState& state) {
    state.start = state.lines.size(); // clamp() pulls it back to the last page
    clamp(state);
}

void on_resize(PagerState& state, const TerminalSize&) {
    clamp(state);
}

Key handle_key(PagerState& state, const std::string& key, bool& should_quit) {
    if (key.empty())
        return Key::NONE;

    if (key == "q" || key == "Q" || key == "\x03") { // q or Ctrl+C
        should_quit = true;
        return Key::QUIT;
    }
    if (key == "\x1b[A" || key == "\x10" || key == "k") { // Up / Ctrl+P
        scroll_up(state);
        return Key::UP;
    }
    if (key == "\x1b[B" || key == "\x0e" || key == "j") { // Down / Ctrl+N
        scroll_down(state);
        return Key::DOWN;
    }
    if (key == "\x1b[5~" || key == "b") { // Page Up
        page_up(state);
        return Key::PAGE_UP;
    }
    if (key == "\x1b[6~" || key == " ") { // Page Down
        page_down(state);
        return Key::PAGE_DOWN;
    }
    if (key == "\x1b[H" || key == "\x1b[1~" || key == "\x1bOH" || key == "\x01" || key == "g") { // Home / Ctrl+A
        jump_to_top(state);
        return Key::HOME;
    }
    if (key == "\x1b[F" || key == "\x1b[4~" || key == "\x1bOF" || key == "\x05" || key == "G") { // End / Ctrl+E
        jump_to_bottom(state);
        return Key::END;
    }
    return Key::NONE;
}

std::pair<size_t, size_t> get_visible_range(const PagerState& state) {
    const size_t ph = static_cast<size_t>(page_height());
    if (state.lines.empty())
        return {0, 0};
    size_t start = static_cast<size_t>(state.start);
    size_t end = std::min(start + ph, state.lines.size());
    return {start, end - 1};
}

} // namespace ascii
