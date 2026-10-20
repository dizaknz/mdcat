#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ascii {

// Pager options for configuring behavior
struct PagerOptions {
    bool use_mouse = false;    // Use mouse for scrolling
    bool show_cursor = true;   // Show cursor
    bool wrap_at_edge = true;  // Wrap long lines
    bool clear_on_exit = true; // Clear screen on exit
};

// Pager state
struct PagerState {
    std::vector<std::string> lines;
    size_t start = 0;
    size_t end = 0;
    bool at_top = false;
    bool at_bottom = false;
    bool eof = false;
    bool quit = false;
};

// Initialize pager terminal capabilities
void init();

// Cleanup pager terminal capabilities
void cleanup();

// Check if pager should use mouse mode
bool can_use_mouse();

// Enable/disable mouse mode
void enable_mouse();
void disable_mouse();

// Get terminal dimensions
struct TerminalSize {
    int width = 0;
    int height = 0;
};
TerminalSize get_terminal_size();

// Render pager UI
void render(PagerState &state, const PagerOptions &opts);

// Handle key press
enum class Key { NONE, UP, DOWN, PAGE_UP, PAGE_DOWN, HOME, END, QUIT, RESIZE };

Key handle_key(PagerState& state, const std::string& key, bool& should_quit);

// Scroll content
void scroll_up(PagerState &state);
void scroll_down(PagerState &state);
void page_up(PagerState &state);
void page_down(PagerState &state);
void jump_to_top(PagerState &state);
void jump_to_bottom(PagerState &state);

// Update state based on terminal resize
void on_resize(PagerState &state, const TerminalSize &new_size);

// Clear pager from screen
void clear_screen();

// Render status bar
void render_status_bar(const PagerState &state);

// Get current line range
std::pair<size_t, size_t> get_visible_range(const PagerState &state);

} // namespace ascii
