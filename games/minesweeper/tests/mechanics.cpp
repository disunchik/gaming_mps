/** @file mechanics.cpp @brief Проверка механики сапёра и подключения к меню на ПК. */
#include "code.h"
#include "input.h"
#include "menu.h"
#include "minesweeper.h"
#include "screen.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

/** @brief Завершает проверку с указанием строки даже при сборке с NDEBUG. */
#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << __LINE__ << ": " << #condition << '\n'; \
        std::exit(EXIT_FAILURE); \
    } \
} while (false)

/** @brief Прямоугольник клетки в области 68×32, включая разделители. */
struct Tile {
    int x; ///< Столбец клетки.
    int y; ///< Строка клетки.
    int width; ///< Ширина клетки.
    int height; ///< Высота клетки.
};
/** @brief Символ закрытой клетки. */
static constexpr std::string_view closed = "▪";

/** @brief Вычисляет ожидаемые одинаковые клетки, центрированные в игровой области. */
static Tile tileAt(int size, int x, int y) {
    const int height = std::min((Screen::width - 4) / (2 * size), (Screen::height - 2) / size);
    const int width = 2 * height;
    return {(Screen::width - size * width) / 2 + x * width,
            (Screen::height - size * height) / 2 + y * height, width, height};
}

/** @brief Возвращает событие мыши в заданной точке клетки. */
static Input mouse(InputType type, int size, int x, int y, int dx = 0, int dy = 0) {
    const auto tile = tileAt(size, x, y);
    return {type, static_cast<std::int16_t>(tile.x + dx), static_cast<std::int16_t>(tile.y + dy)};
}

/** @brief Возвращает изображение модуля из общего рендерера. */
static std::string frame(const Minesweeper& game) {
    Screen screen;
    game.draw(screen);
    return screen.plain();
}

/** @brief Считывает глиф в центре плитки, учитывая одноколоночный UTF-8. */
static std::string_view cellGlyph(std::string_view frame, int size, int x, int y, bool sideMark = false) {
    const auto tile = tileAt(size, x, y);
    const auto marker = mouse(InputType::None, size, x, y,
                              sideMark ? tile.width - 1 : (tile.width - 1) / 2, (tile.height - 1) / 2);
    std::size_t byte = 0;
    for (int row = 0; row < Screen::fieldY + marker.y; ++row) {
        byte = frame.find('\n', byte) + 1;
    }
    const auto nextGlyph = [&frame](std::size_t start) {
        std::size_t end = start + 1;
        while (end < frame.size() && (static_cast<unsigned char>(frame[end]) & 0xC0u) == 0x80u) ++end;
        return end;
    };
    for (int col = 0; col < Screen::fieldX + marker.x; ++col) byte = nextGlyph(byte);
    return frame.substr(byte, nextGlyph(byte) - byte);
}

/** @brief Проверяет реальную раскладку и все подсказки одного набора настроек. */
static void auditField(Settings settings, int expectedMines) {
    const int size = 6 + 2 * static_cast<int>(settings.level);
    Minesweeper game;
    std::array<bool, 256> mine{};
    int actualMines = 0;

    // For each probe use the same first move and seed; use only the public API.
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            game.start(settings);
            CHECK(game.result().state == State::Playing);
            CHECK(game.result().firstX == settings.firstX && game.result().firstY == settings.firstY);
            CHECK(game.result().timeMs == 0);
            game.handle(mouse(InputType::PointerLeft, size, x, y, (x + y) % 2));
            const bool hitMine = game.result().state == State::Lost;
            mine[y * size + x] = hitMine;
            actualMines += hitMine ? 1 : 0;
        }
    }
    CHECK(actualMines == expectedMines);
    CHECK(!mine[settings.firstY * size + settings.firstX]);

    game.start(settings);
    CHECK(frame(game).find("Динамиты: " + std::to_string(expectedMines)) != std::string::npos);
    game.update(1250);
    CHECK(game.result().timeMs == 1250);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (!mine[y * size + x]) game.handle(mouse(InputType::PointerLeft, size, x, y));
        }
    }
    CHECK(game.result().state == State::Won &&
          game.result().score == static_cast<unsigned>(size * size - expectedMines));

    const auto solved = frame(game);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (mine[y * size + x]) {
                CHECK(cellGlyph(solved, size, x, y) == "■");
                continue;
            }
            int neighbors = 0;
            for (int i = -1; i <= 1; ++i) {
                for (int j = -1; j <= 1; ++j) {
                    const int nx = x + i, ny = y + j;
                    if (nx >= 0 && nx < size && ny >= 0 && ny < size) neighbors += mine[ny * size + nx];
                }
            }
            const char expected = neighbors == 0 ? ' ' : static_cast<char>('0' + neighbors);
            CHECK(cellGlyph(solved, size, x, y) == std::string_view(&expected, 1));
        }
    }
    game.update(2000);
    game.handle(mouse(InputType::PointerRight, size, 0, 0));
    CHECK(game.result().timeMs == 1250 && frame(game) == solved);

    game.start(settings);
    CHECK(game.result().state == State::Playing);
    for (int i = 0; i < size * size; ++i) {
        if (!mine[i]) continue;
        game.handle(mouse(InputType::PointerLeft, size, i % size, i / size, 1));
        CHECK(game.result().state == State::Lost);
        const auto lost = frame(game);
        CHECK(cellGlyph(lost, size, i % size, i / size) == "X");
        CHECK(lost.find(closed) == std::string::npos);
        return;
    }
    CHECK(false);
}

/** @brief Проверяет углы, мышь, клавиши, флаги и безопасный первый ход. */
static void auditInput() {
    Minesweeper game;
    constexpr int size = 16;
    Settings settings;
    settings.level = 5;
    game.start(settings);
    game.handle({InputType::PointerMove});
    game.handle(mouse(InputType::PointerRight, size, -1, 0, 1));
    game.handle(mouse(InputType::PointerLeft, size, size, 0));
    for (int i = 0; i < size * size; ++i) {
        game.handle(mouse(InputType::PointerRight, size, i % size, i / size, 1));
    }
    game.handle(mouse(InputType::PointerLeft, size, size - 1, size - 1));
    game.update(3200);
    CHECK(game.result().timeMs == 0 && game.result().firstX == -1);
    CHECK(frame(game).find("Банданы: 256") != std::string::npos);
    CHECK(game.result().score == 0);
    for (int i = 0; i < size * size; ++i) {
        game.handle(mouse(InputType::PointerRight, size, i % size, i / size));
    }
    CHECK(frame(game).find("Банданы: 0") != std::string::npos);
    game.handle({InputType::Open});
    CHECK(game.result().state == State::Playing);
    CHECK(game.result().firstX == size - 1 && game.result().firstY == size - 1);

    game.start({});
    for (int i = 0; i < 16; ++i) {
        game.handle({InputType::Up});
        game.handle({InputType::Left});
    }
    game.handle({InputType::Flag});
    game.handle({InputType::Confirm});
    CHECK(game.result().firstX == -1);
    game.handle({InputType::Flag});
    game.handle({InputType::Confirm});
    CHECK(game.result().firstX == 0 && game.result().firstY == 0);
    CHECK(game.result().state == State::Playing);
}

/** @brief Возвращает фиксированное значение для воспроизводимой проверки меню. */
static std::uint32_t seed() {
    return 0;
}

/** @brief Проверяет связанное раскрытие и работу паузы, повтора и кода в общем меню. */
static void auditMenu() {
    Minesweeper game;
    // On this board an open zero at (0,1) reaches more cells than just its immediate neighbors.
    game.start({});
    game.handle(mouse(InputType::PointerRight, 8, 0, 0));
    game.handle(mouse(InputType::PointerLeft, 8, 0, 1));
    const auto revealed = frame(game);
    CHECK(cellGlyph(revealed, 8, 0, 0) == "▼");
    CHECK(cellGlyph(revealed, 8, 0, 1) == " ");
    CHECK(cellGlyph(revealed, 8, 1, 0) != closed);
    CHECK(cellGlyph(revealed, 8, 0, 3) != closed);

    // Only the Minesweeper slot is activated in this test.
    Menu menu(game, game, game, seed);
    Screen screen;
    menu.handle({InputType::PointerLeft, 30, 17});
    menu.handle({InputType::Down});
    menu.handle({InputType::Down});
    menu.handle({InputType::Confirm});
    menu.draw(screen);
    CHECK(screen.plain().find("Код после первого открытия") != std::string::npos);
    Input first = mouse(InputType::PointerLeft, 8, 0, 1, 1);
    first.x += Screen::fieldX;
    first.y += Screen::fieldY;
    menu.handle(first);
    CHECK(game.result().firstX == 0 && game.result().firstY == 1);
    menu.draw(screen);
    Settings recorded;
    recorded.firstX = 0;
    recorded.firstY = 1;
    const auto code = encodeCode(GameId::Minesweeper, recorded);
    CHECK(!code.empty() && screen.plain().find(code) != std::string::npos);
    const auto original = frame(game);
    const auto openingScore = game.result().score;
    CHECK(openingScore > 0);
    CHECK(original.find("Цена ошибки: " + std::to_string(openingScore)) != std::string::npos);

    menu.update(0);
    menu.update(1500);
    menu.handle({InputType::Pause});
    menu.update(5000);
    CHECK(game.result().timeMs == 1500);
    menu.handle({InputType::Restart});
    CHECK(game.result().timeMs == 0 && game.result().score == openingScore && frame(game) == original);
    menu.handle({InputType::Back});
    menu.draw(screen);
    CHECK(screen.plain().find("Выбери игру") != std::string::npos);

    menu.handle({InputType::PointerLeft, 30, 21});
    for (char ch : code) {
        menu.handle({InputType::Text, -1, -1, static_cast<char32_t>(ch)});
    }
    menu.handle({InputType::Confirm});
    menu.handle({InputType::Confirm});
    CHECK(frame(game) == original && game.result().timeMs == 0);

    // The seed source always returns 0, including when asked for a fresh field.
    menu.update(0);
    Input mine = mouse(InputType::PointerLeft, 8, 2, 0);
    mine.x += Screen::fieldX;
    mine.y += Screen::fieldY;
    menu.handle(mine);
    menu.update(1);
    menu.draw(screen);
    CHECK(screen.plain().find("Посмотреть все поле") != std::string::npos);
    CHECK(screen.plain().find("ДИНАМИТНАЯ ДОЛИНА") == std::string::npos);

    menu.handle({InputType::Down});
    menu.handle({InputType::Confirm});
    menu.draw(screen);
    CHECK(screen.plain().find("ДИНАМИТНАЯ ДОЛИНА") != std::string::npos);
    CHECK(screen.plain().find(code) != std::string::npos);
    CHECK(cellGlyph(screen.plain(), 8, 2, 0) == "X");
    const auto reviewed = game.result();
    menu.handle(first);
    menu.update(5000);
    CHECK(game.result().score == reviewed.score && game.result().timeMs == reviewed.timeMs);

    menu.handle({InputType::Confirm});
    CHECK(game.result().state == State::Playing && game.result().firstX == -1 && game.result().score == 0);
    menu.draw(screen);
    CHECK(screen.plain().find("Код после первого открытия") != std::string::npos);
    menu.handle(first);
    menu.draw(screen);
    recorded.seed = 1;
    const auto freshCode = encodeCode(GameId::Minesweeper, recorded);
    CHECK(freshCode != code && screen.plain().find(freshCode) != std::string::npos);
}

/** @brief Проверяет соседей цифры с точным, недостаточным и ошибочным набором флагов. */
static void auditChord() {
    Minesweeper game;
    const Settings settings{Difficulty::Normal, 1, 0, 0, 1};
    const auto clue = mouse(InputType::PointerLeft, 8, 3, 2);
    game.start(settings);
    CHECK(cellGlyph(frame(game), 8, 3, 2) == "1");
    CHECK(cellGlyph(frame(game), 8, 4, 1) == closed);
    const auto initialScore = game.result().score;
    CHECK(initialScore > 0);
    game.handle(clue);
    CHECK(game.result().score == initialScore);
    CHECK(cellGlyph(frame(game), 8, 4, 1) == closed);

    game.handle(mouse(InputType::PointerRight, 8, 3, 1));
    CHECK(game.result().score == initialScore);
    game.handle(clue);
    CHECK(game.result().score > initialScore);
    CHECK(game.result().state == State::Playing);
    CHECK(cellGlyph(frame(game), 8, 4, 1) != closed);

    game.start(settings);
    game.handle(mouse(InputType::PointerRight, 8, 4, 1));
    game.handle(clue);
    CHECK(game.result().state == State::Lost);
    CHECK(cellGlyph(frame(game), 8, 3, 1) == "X");
    const auto lost = frame(game);
    CHECK(cellGlyph(lost, 8, 4, 1) == "2");
    CHECK(cellGlyph(lost, 8, 4, 1, true) == "!");
}

/** @brief Проверяет все позиции и края растянутых клеток без пропусков и перекрытий. */
static void auditHitboxes() {
    Minesweeper game;
    for (std::uint32_t level = 1; level <= 5; ++level) {
        Settings settings;
        settings.level = level;
        const int size = 6 + 2 * static_cast<int>(level);
        game.start(settings);
        const auto first = tileAt(size, 0, 0);
        CHECK(first.width == 2 * first.height);
        CHECK((first.width * size + 4) == 2 * (first.height * size + 2));
        const int faceHeight = std::max(1, first.height - 1);
        CHECK(2 * faceHeight <= first.width && faceHeight <= first.height);
        for (int y = 0; y < Screen::height; ++y) {
            for (int x = 0; x < Screen::width; ++x) {
                if (x >= first.x && x < first.x + size * first.width &&
                    y >= first.y && y < first.y + size * first.height) continue;
                game.handle({InputType::PointerLeft, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)});
            }
        }
        CHECK(game.result().firstX == -1);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const auto tile = tileAt(size, x, y);
                CHECK(tile.width == first.width && tile.height == first.height);
                for (int dy = 0; dy < tile.height; ++dy) {
                    for (int dx = 0; dx < tile.width; ++dx) {
                        game.start(settings);
                        game.handle(mouse(InputType::PointerRight, size, x, y, dx, dy));
                        CHECK(cellGlyph(frame(game), size, x, y) == "▼");
                        CHECK(frame(game).find("Банданы: 1") != std::string::npos);
                    }
                }
                game.start(settings);
                game.handle(mouse(InputType::PointerLeft, size, x, y, tile.width - 1, tile.height - 1));
                CHECK(game.result().firstX == x && game.result().firstY == y);
            }
        }
    }
}

/** @brief Проверяет 15 сочетаний размера и сложности и работу общего управления. */
int main() {
    constexpr int counts[5][3] = {
        {6, 10, 13}, {10, 15, 20}, {14, 22, 29}, {20, 29, 39}, {26, 38, 51}
    };
    for (std::uint32_t level = 1; level <= 5; ++level) {
        for (unsigned difficulty = 0; difficulty < 3; ++difficulty) {
            const auto last = static_cast<std::int16_t>(5 + 2 * level);
            const auto firstX = static_cast<std::int16_t>(difficulty == 1 ? last / 2 : last);
            const auto firstY = static_cast<std::int16_t>(difficulty == 2 ? last : 0);
            auditField({static_cast<Difficulty>(difficulty), level, 0, firstX, firstY},
                       counts[level - 1][difficulty]);
        }
    }
    auditInput();
    auditHitboxes();
    auditMenu();
    auditChord();
    std::cout << "PASS: 15 profiles, mine counts, numbers, wins and losses, input, flood fill, "
                 "flags, score, timing, pause, board review, fresh seed, replay code, chord and return to menu.\n";
}
