// text.h - UTF-8, display-width and ANSI-aware wrapping helpers shared by all modules.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace text {

std::string utf8(char32_t cp);                                   // encode one code point
size_t decodeUtf8(const std::string &s, size_t i, char32_t &cp); // returns bytes consumed
std::string fromU32(const std::u32string &s);
int codepointWidth(char32_t cp);        // 0, 1 or 2 terminal columns
int displayWidth(const std::string &s); // columns, ignoring ANSI SGR

std::string trim(const std::string &s);
std::string toLower(std::string s);
bool startsWith(const std::string &s, const std::string &prefix);
std::string repeat(const std::string &s, int n);
std::vector<std::string> splitLines(const std::string &s);
std::string expandTabs(const std::string &line);

// Remove bytes that could drive the terminal (ESC, C0/C1 controls) from untrusted text.
// '\n' is kept; '\t' becomes a space.
std::string sanitize(std::string_view s);

// Word-wrap text that may contain ANSI SGR codes and '\n' hard breaks. Styles open at a
// line end are closed and re-opened on the next line, so every returned line stands alone.
std::vector<std::string> wrap(const std::string &text, int width);

} // namespace text
