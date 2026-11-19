/** @file screen.cpp @brief Буферизованный ANSI-экран с UTF-8. */
#include "screen.h"
#include <algorithm>
#include <cstdio>

/** @brief Добавляет один Unicode-символ к строке UTF-8. */
static void utf8(std::string& text, char32_t glyph) {
    if (glyph < 0x80) text += static_cast<char>(glyph);
    else if (glyph < 0x800) {
        text += static_cast<char>(0xc0 | (glyph >> 6));
        text += static_cast<char>(0x80 | (glyph & 63));
    } else if (glyph < 0x10000) {
        text += static_cast<char>(0xe0 | (glyph >> 12));
        text += static_cast<char>(0x80 | ((glyph >> 6) & 63));
        text += static_cast<char>(0x80 | (glyph & 63));
    } else {
        text += '?'; // The fixed-cell UI deliberately excludes emoji and wide glyphs.
    }
}
/** @brief Читает один символ доверенного UTF-8-текста интерфейса. */
static char32_t readUtf8(std::string_view text, std::size_t& index) {
    auto first = static_cast<unsigned char>(text[index++]);
    if (first < 128) return first;
    unsigned count = (first & 0xe0) == 0xc0 ? 1 : (first & 0xf0) == 0xe0 ? 2 : 3;
    char32_t value = first & (count == 1 ? 31 : count == 2 ? 15 : 7);
    while (count--) {
        if (index >= text.size() || (static_cast<unsigned char>(text[index]) & 0xc0) != 0x80) return U'?';
        value = (value << 6) | (static_cast<unsigned char>(text[index++]) & 63);
    }
    return value;
}
/** @brief Выводит байты в stdout в сборке для ПК. */
static void writeStdout(const char* bytes, std::size_t size, void*) {
    std::fwrite(bytes, 1, size, stdout);
    std::fflush(stdout);
}
Screen::Screen(Output output, void* context)
    : output_(output ? output : writeStdout), context_(context) {}
void Screen::begin() { next_.fill(Pixel{}); }
void Screen::put(int x, int y, char32_t glyph, Color foreground, Color background) {
    if (x >= 0 && x < canvasWidth && y >= 0 && y < canvasHeight && !(x == canvasWidth - 1 && y == canvasHeight - 1))
        next_[y * canvasWidth + x] = Pixel{glyph >= 32 ? glyph : U' ', foreground, background};
}
void Screen::clippedText(int x, int y, std::string_view value, Color foreground, Color background,
                         int left, int top, int w, int h) {
    if (y < top || y >= top + h) return;
    for (std::size_t i = 0; i < value.size() && x < left + w; ++x) {
        const char32_t glyph = readUtf8(value, i);
        if (x >= left) put(x, y, glyph, foreground, background);
    }
}
void Screen::canvasText(int x, int y, std::string_view value, Color foreground, Color background) {
    clippedText(x, y, value, foreground, background, 0, 0, canvasWidth, canvasHeight);
}
void Screen::text(int x, int y, std::string_view value, Color foreground, Color background) {
    clippedText(x + 2, y + 5, value, foreground, background, 2, 5, width, height);
}
void Screen::number(int x, int y, std::int64_t value, Color foreground, Color background) {
    text(x, y, std::to_string(value), foreground, background);
}
void Screen::canvasFill(int x, int y, int w, int h, Color background) {
    for (int row = std::max(0, y); row < std::min(canvasHeight, y + h); ++row)
        for (int col = std::max(0, x); col < std::min(canvasWidth, x + w); ++col)
            put(col, row, U' ', Theme::text, background);
}
void Screen::fill(int x, int y, int w, int h, Color background) {
    for (int row = std::max(0, y); row < std::min(height, y + h); ++row)
        for (int col = std::max(0, x); col < std::min(width, x + w); ++col)
            put(col + 2, row + 5, U' ', Theme::text, background);
}
void Screen::cell(int x, int y, Color foreground, Color background, char32_t glyph) {
    if (y < 0 || y >= height) return;
    if (x >= 0 && x < width) put(x + 2, y + 5, glyph, foreground, background);
    if (x + 1 >= 0 && x + 1 < width) put(x + 3, y + 5, U' ', foreground, background);
}
void Screen::glyph(int x, int y, Color foreground, Color background, char32_t value) {
    if (x >= 0 && x < width && y >= 0 && y < height)
        put(x + fieldX, y + fieldY, value, foreground, background);
}
void Screen::canvasFrame(int x, int y, int w, int h, Color foreground) {
    if (w < 2 || h < 2) return;
    for (int i = 1; i < w - 1; ++i) {
        put(x + i, y, U'═', foreground, Theme::background);
        put(x + i, y + h - 1, U'═', foreground, Theme::background);
    }
    for (int i = 1; i < h - 1; ++i) {
        put(x, y + i, U'║', foreground, Theme::background);
        put(x + w - 1, y + i, U'║', foreground, Theme::background);
    }
    put(x, y, U'╔', foreground, Theme::background);
    put(x + w - 1, y, U'╗', foreground, Theme::background);
    put(x, y + h - 1, U'╚', foreground, Theme::background);
    put(x + w - 1, y + h - 1, U'╝', foreground, Theme::background);
}
void Screen::frame(int x, int y, int w, int h, Color foreground) {
    if (w < 2 || h < 2) return;
    for (int row = 0; row < h; ++row)
        for (int col = 0; col < w; ++col) {
            if (row && row != h - 1 && col && col != w - 1) continue;
            const int px = x + col, py = y + row;
            if (px < 0 || px >= width || py < 0 || py >= height) continue;
            const bool top = row == 0, bottom = row == h - 1;
            const char32_t glyph = col == 0 ? (top ? U'┌' : bottom ? U'└' : U'│')
                : col == w - 1 ? (top ? U'┐' : bottom ? U'┘' : U'│') : U'─';
            put(px + 2, py + 5, glyph, foreground, Theme::background);
        }
}
void Screen::panelText(int row, std::string_view value, Color foreground) {
    panelTextAt(0, row, value, foreground);
}
void Screen::panelTextAt(int x, int row, std::string_view value, Color foreground) {
    clippedText(panelX + x, row + fieldY, value, foreground, Theme::background, panelX, fieldY, panelWidth, panelHeight);
}
void Screen::panelNumber(int row, std::string_view label, std::int64_t value, Color foreground) {
    panelText(row, std::string(label) + " " + std::to_string(value), foreground);
}
void Screen::flush() {
    std::string output;
    if (!initialized_) {
        char setup[96];
        std::snprintf(setup, sizeof(setup), "\x1b[?1049h\x1b[?25l\x1b[?1003h\x1b[?1006h\x1b[48;5;%um\x1b[2J",
                      static_cast<unsigned>(Theme::background));
        output = setup; // Clear the full viewport, including space beyond the game canvas.
    }
    for (int y = 0; y < canvasHeight; ++y) {
        int x = 0;
        while (x < canvasWidth) {
            if ((x == canvasWidth - 1 && y == canvasHeight - 1) || (initialized_ && next_[y * canvasWidth + x] == shown_[y * canvasWidth + x])) { ++x; continue; }
            const Pixel first = next_[y * canvasWidth + x];
            char control[64];
            std::snprintf(control, sizeof(control), "\x1b[%d;%dH\x1b[38;5;%u;48;5;%um", y + 1, x + 1,
                          static_cast<unsigned>(first.foreground), static_cast<unsigned>(first.background));
            output += control;
            do {
                utf8(output, next_[y * canvasWidth + x].glyph);
                shown_[y * canvasWidth + x] = next_[y * canvasWidth + x];
                ++x;
            } while (x < canvasWidth && !(x == canvasWidth - 1 && y == canvasHeight - 1)
                     && next_[y * canvasWidth + x].foreground == first.foreground
                     && next_[y * canvasWidth + x].background == first.background
                     && (!initialized_ || !(next_[y * canvasWidth + x] == shown_[y * canvasWidth + x])));
        }
    }
    initialized_ = true;
    if (!output.empty()) {
        char reset[32];
        std::snprintf(reset, sizeof(reset), "\x1b[0m\x1b[48;5;%um", static_cast<unsigned>(Theme::background));
        output += reset;
        output_(output.data(), output.size(), context_);
    }
}
std::string Screen::plain() const {
    std::string result;
    for (int y = 0; y < canvasHeight; ++y) {
        for (int x = 0; x < canvasWidth; ++x) utf8(result, next_[y * canvasWidth + x].glyph);
        result += '\n';
    }
    return result;
}

void Screen::copyText(std::string_view text) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (text.empty() || text.size() > 64) return;
    std::string sequence = "\x1b]52;c;";
    for (std::size_t i = 0; i < text.size(); i += 3) {
        unsigned block = static_cast<unsigned char>(text[i]) << 16;
        if (i + 1 < text.size()) block |= static_cast<unsigned char>(text[i + 1]) << 8;
        if (i + 2 < text.size()) block |= static_cast<unsigned char>(text[i + 2]);
        sequence += alphabet[(block >> 18) & 63]; sequence += alphabet[(block >> 12) & 63];
        sequence += i + 1 < text.size() ? alphabet[(block >> 6) & 63] : '=';
        sequence += i + 2 < text.size() ? alphabet[block & 63] : '=';
    }
    sequence += '\a';
    output_(sequence.data(), sequence.size(), context_);
}

void Screen::invalidate() { initialized_ = false; }
void Screen::requestPaste() {
    static constexpr char query[] = "\x1b]52;c;?\a";
    output_(query, sizeof(query) - 1, context_);
}
