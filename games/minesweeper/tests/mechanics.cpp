/** @file mechanics.cpp @brief Проверка механики сапёра и подключения к меню на ПК. */
#include "code.h"
#include "input.h"
#include "menu.h"
#include "minesweeper.h"
#include "screen.h"
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

/** @brief Ожидаемая геометрия на экране 72×30. */
struct Tile {
    int width; ///< Ширина клетки в символах.
    int height; ///< Высота клетки в строках.
};
/** @brief Допустимые размеры отображаемой клетки. */
static constexpr Tile tiles[] = {{7, 3}, {6, 2}, {5, 2}, {4, 2}, {4, 1}};
/** @brief Символ закрытой клетки. */
static constexpr std::string_view closed = "▪";

/** @brief Возвращает событие мыши в заданной точке клетки. */
static Input mouse(InputType type, int size, int x, int y, int dx = 0, int dy = 0) {
    const auto tile = tiles[(size - 8) / 2];
    const int left = (Screen::width - size * tile.width) / 2;
    const int top = (Screen::height - size * tile.height) / 2;
    return {type, static_cast<std::int16_t>(left + tile.width * x + dx),
            static_cast<std::int16_t>(top + tile.height * y + dy)};
}

/** @brief Возвращает изображение модуля из общего рендерера. */
static std::string frame(const Minesweeper& game) {
    Screen screen;
    game.draw(screen);
    return screen.plain();
}

/** @brief Считывает глиф в центре плитки, учитывая одноколоночный UTF-8. */
static std::string_view cellGlyph(std::string_view frame, int size, int x, int y) {
    const auto tile = tiles[(size - 8) / 2];
    const auto marker = mouse(InputType::None, size, x, y, (tile.width - 2) / 2, (tile.height - 1) / 2);
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
    CHECK(game.result().state == State::Won && game.result().score == 0);

    const auto solved = frame(game);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (mine[y * size + x]) {
                CHECK(cellGlyph(solved, size, x, y) == closed);
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
        CHECK(cellGlyph(frame(game), size, i % size, i / size) == "X");
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
    CHECK(frame(game).find("Флагов: 256") != std::string::npos);
    CHECK(frame(game).find("Осталось: -218") != std::string::npos);
    for (int i = 0; i < size * size; ++i) {
        game.handle(mouse(InputType::PointerRight, size, i % size, i / size));
    }
    CHECK(frame(game).find("Флагов: 0") != std::string::npos);
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
    CHECK(cellGlyph(revealed, 8, 0, 0) == "F");
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

    menu.update(0);
    menu.update(1500);
    menu.handle({InputType::Pause});
    menu.update(5000);
    CHECK(game.result().timeMs == 1500);
    menu.handle({InputType::Restart});
    CHECK(game.result().timeMs == 0 && frame(game) == original);
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
}

/** @brief Проверяет соседей цифры с точным, недостаточным и ошибочным набором флагов. */
static void auditChord() {
    Minesweeper game;
    const Settings settings{Difficulty::Normal, 1, 0, 0, 1};
    const auto clue = mouse(InputType::PointerLeft, 8, 3, 2);
    game.start(settings);
    CHECK(cellGlyph(frame(game), 8, 3, 2) == "1");
    CHECK(cellGlyph(frame(game), 8, 4, 1) == closed);
    game.handle(clue);
    CHECK(cellGlyph(frame(game), 8, 4, 1) == closed);

    game.handle(mouse(InputType::PointerRight, 8, 3, 1));
    game.handle(clue);
    CHECK(game.result().state == State::Playing);
    CHECK(cellGlyph(frame(game), 8, 4, 1) != closed);

    game.start(settings);
    game.handle(mouse(InputType::PointerRight, 8, 4, 1));
    game.handle(clue);
    CHECK(game.result().state == State::Lost);
    CHECK(cellGlyph(frame(game), 8, 3, 1) == "X");
    CHECK(cellGlyph(frame(game), 8, 4, 1) == "!");
}

/** @brief Проверяет расширенные зоны клика и отбрасывание щелчков по раме. */
static void auditHitboxes() {
    Minesweeper game;
    for (std::uint32_t level = 1; level <= 5; ++level) {
        Settings settings;
        settings.level = level;
        const int size = 6 + 2 * static_cast<int>(level);
        const auto tile = tiles[level - 1];
        for (const int x : {0, size - 1}) {
            for (const int y : {0, size - 1}) {
                game.start(settings);
                game.handle(mouse(InputType::PointerLeft, size, size, y));
                game.handle(mouse(InputType::PointerLeft, size, x, size));
                game.handle(mouse(InputType::PointerLeft, size, -1, y, tile.width - 1));
                game.handle(mouse(InputType::PointerLeft, size, x, -1, 0, tile.height - 1));
                CHECK(game.result().firstX == -1);
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
                 "flags, timing, pause, restart, replay code, chord and return to menu.\n";
}
