/** @file core.cpp @brief Проверки границ протокола и поведения общей оболочки. */
#include "menu.h"
#include "code.h"
#include "tetris.h"
#include "minesweeper.h"
#include "pong.h"
#include <cstdlib>
#include <iostream>
#include <limits>

/** @brief Прерывает проверку с номером строки при нарушении условия. */
#define CHECK(condition) do { if (!(condition)) { std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; std::exit(1); } } while (false)

/** @brief Игровой двойник для проверки именно оболочки, включая победу и поражение. */
class Fake final : public Game {
public:
    Settings settings{}; ///< Последние полученные условия.
    Result current{}; ///< Управляемое тестом состояние.
    Input last{}; ///< Последнее полученное событие.
    unsigned starts = 0; ///< Количество полных перезапусков.
    unsigned handled = 0; ///< Количество игровых событий.
    /** @brief Сбрасывает партию и запоминает старт. */
    void start(const Settings& value) override { settings = value; current = {}; current.level = value.level; ++starts; }
    /** @brief Запоминает готовый ввод. */
    void handle(const Input& value) override { last = value; ++handled; }
    /** @brief Учитывает только переданное игровое время. */
    void update(std::uint32_t ms) override { current.timeMs += ms; }
    /** @brief Создаёт постоянное изображение для проверки чистоты draw. */
    void draw(Screen& screen) const override { screen.text(0, 0, "FAKE"); }
    /** @brief Возвращает управляемое состояние. */
    Result result() const override { return current; }
};

/** @brief Даёт воспроизводимый новый seed для теста меню. */
static std::uint32_t seed() { static std::uint32_t next = 123; return next++; }
/** @brief Отправляет одно действие оболочке. */
static void press(Menu& menu, InputType type, int x = -1, int y = -1) {
    Input input; input.type = type; input.x = x; input.y = y; menu.handle(input);
}
/** @brief Возвращает видимый экран без ANSI. */
static std::string view(Menu& menu) { Screen screen; menu.draw(screen); return screen.plain(); }
/** @brief Добавляет байты рендера в строку теста. */
static void capture(const char* text, std::size_t count, void* context) { static_cast<std::string*>(context)->append(text, count); }

/** @brief Проверяет фрагментацию, безопасный отказ, CRLF и контекст ввода кода. */
static void inputTests() {
    InputReader parser;
    Input event;
    std::uint32_t now = 0;
    auto feed = [&](std::string_view bytes, bool text = false) {
        bool produced = false;
        for (unsigned char byte : bytes) if (parser.feed(byte, ++now, text, event)) produced = true;
        return produced;
    };
    CHECK(!feed("\x1b[")); CHECK(feed("D")); CHECK(event.type == InputType::Left);
    CHECK(feed("\x1bOA")); CHECK(event.type == InputType::Up);
    CHECK(feed("\x1b[<0;3;6M")); CHECK(event.type == InputType::PointerLeft && event.x == 2 && event.y == 5);
    CHECK(feed("\x1b[<2;54;25M")); CHECK(event.type == InputType::PointerRight && event.x == 53 && event.y == 24);
    CHECK(!feed("\x1b[<0;3;6m")); CHECK(!feed("\x1b[<0;0;6M"));
    CHECK(!feed("\x1b[<0;99999;6M")); CHECK(!feed("\x1b[<64;4;4M"));
    CHECK(feed("\x1b[<35;4;4M")); CHECK(event.type == InputType::PointerMove);
    CHECK(!feed("\x1b[<" + std::string(90, '1') + ";2;2M"));
    CHECK(feed("c")); CHECK(event.type == InputType::Copy);
    CHECK(feed("c", true)); CHECK(event.type == InputType::Text);
    CHECK(feed("\x1b[901~")); CHECK(event.type == InputType::CopySucceeded);
    CHECK(feed("\x1b[902~")); CHECK(event.type == InputType::CopyFailed);
    CHECK(feed("\x1b[910~")); CHECK(event.type == InputType::HeldLeft);
    CHECK(feed("\x1b[911~")); CHECK(event.type == InputType::HeldRight);
    CHECK(feed("\x1b[912~", true)); CHECK(event.type == InputType::HorizontalReleased);
    CHECK(feed("\x16", true)); CHECK(event.type == InputType::Paste);
    CHECK(feed("\x1b[200~")); CHECK(event.type == InputType::PasteBegin);
    CHECK(feed("qprc")); CHECK(event.type == InputType::PasteText);
    CHECK(!feed("\r"));
    CHECK(feed("\x1b[201~")); CHECK(event.type == InputType::PasteEnd);
    CHECK(feed("\x1b[903~")); CHECK(event.type == InputType::PasteFailed);
    CHECK(feed("\x1b[930~")); CHECK(event.type == InputType::Redraw);
    CHECK(feed("r")); CHECK(event.type == InputType::Restart);
    CHECK(feed("q", true)); CHECK(event.type == InputType::Text && event.character == U'q');
    CHECK(feed("\r")); CHECK(!feed("\n"));
    CHECK(!feed("\x1b")); CHECK(!parser.poll(now + 49, event));
    CHECK(parser.poll(now + 50, event) && event.type == InputType::Back);
    CHECK(!parser.feed(27, std::numeric_limits<std::uint32_t>::max() - 10, false, event));
    CHECK(parser.poll(45, event) && event.type == InputType::Back);
}

/** @brief Сверяет внешний контрольный вектор, повреждения и все допустимые размеры. */
static void codeTests() {
    Settings settings; settings.level = 3; settings.seed = 0x12345678;
    CHECK(encodeCode(GameId::Pong, settings) == "P2-4SW5-CD0J-0228E");
    GameId game = GameId::Tetris;
    Settings decoded;
    std::string error;
    CHECK(decodeCode("p2 4sw5 cd0j 0228e", game, decoded, error));
    CHECK(game == GameId::Pong && decoded.seed == settings.seed && decoded.level == 3);
    const Settings before = decoded;
    CHECK(!decodeCode("P1-041020-R00007-GNHM2B-ZZYZJ4", game, decoded, error));
    CHECK(error == "Нужен полный код P2" && decoded.seed == before.seed && game == GameId::Pong);
    CHECK(!decodeCode("P1-041020-R00007-GNHM2B-ZZYZJ5", game, decoded, error));
    CHECK(decoded.seed == before.seed && decoded.level == before.level && !error.empty());
    CHECK(!decodeCode("P2-041020-R00007-GNHM2B-ZZYZJ4", game, decoded, error));
    for (unsigned id = 0; id < 3; ++id)
        for (unsigned difficulty = 0; difficulty < 3; ++difficulty)
            for (unsigned level = 1; level <= (id == 1 ? 5u : id == 2 ? 4u : 1u); ++level) {
                Settings value; value.difficulty = static_cast<Difficulty>(difficulty); value.level = level;
                value.seed = level == 1 ? 0 : 0xffffffff;
                if (id == 1) value.firstX = value.firstY = 5 + 2 * level;
                const auto code = encodeCode(static_cast<GameId>(id), value);
                CHECK(!code.empty()); CHECK(decodeCode(code, game, decoded, error));
                CHECK(game == static_cast<GameId>(id) && decoded.firstX == value.firstX && decoded.seed == value.seed);
            }
    Settings invalid; CHECK(encodeCode(GameId::Minesweeper, invalid).empty());
    invalid.firstX = 8; invalid.firstY = 0; CHECK(!validSettings(GameId::Minesweeper, invalid));
    invalid.firstX = 7; CHECK(validSettings(GameId::Minesweeper, invalid));
    for (unsigned gameId = 0; gameId < 3; ++gameId) {
        Settings edge; edge.seed = 0xffffffff;
        if (gameId == 1) { edge.level = 5; edge.firstX = 15; edge.firstY = 15; }
        else if (gameId == 2) edge.level = 4;
        const auto encoded = encodeCode(static_cast<GameId>(gameId), edge);
        CHECK(encoded.size() == 18);
        CHECK(decodeCode(encoded, game, decoded, error));
        CHECK(decoded.seed == edge.seed && decoded.level == edge.level && decoded.firstX == edge.firstX);
        const auto previous = decoded;
        std::string broken = encoded; broken[3] = broken[3] == 'A' ? 'B' : 'A';
        CHECK(!decodeCode(broken, game, decoded, error) && decoded.seed == previous.seed);
        broken = encoded; broken.back() = 'Z';
        CHECK(!decodeCode(broken, game, decoded, error));
    }
    invalid.level = 0; CHECK(!validSettings(GameId::Minesweeper, invalid));
}

/** @brief Проверяет паузу, границы мыши, повтор и последовательность уровней. */
static void menuTests() {
    Fake tetris, mines, pong;
    Menu menu(tetris, mines, pong, seed);
    press(menu, InputType::Up); // No wrapping from the first option.
    press(menu, InputType::Confirm);
    press(menu, InputType::Right);
    press(menu, InputType::Down); press(menu, InputType::Confirm);
    CHECK(tetris.starts == 1 && tetris.settings.difficulty == Difficulty::Hard);
    menu.update(0); menu.update(1000); CHECK(tetris.current.timeMs == 1000);
    const auto firstSeed = tetris.settings.seed;
    press(menu, InputType::Pause); menu.update(60000);
    const auto handled = tetris.handled;
    press(menu, InputType::Right); CHECK(tetris.handled == handled);
    press(menu, InputType::HorizontalReleased);
    CHECK(tetris.last.type == InputType::HorizontalReleased && tetris.handled == handled + 1);
    CHECK(tetris.current.timeMs == 1000);
    press(menu, InputType::Pause); menu.update(60000); menu.update(17);
    CHECK(tetris.current.timeMs == 1017);
    press(menu, InputType::Restart);
    CHECK(tetris.starts == 2 && tetris.settings.seed == firstSeed && tetris.current.timeMs == 0);
    press(menu, InputType::PointerLeft, 2, 5);
    CHECK(tetris.last.x == 0 && tetris.last.y == 0);
    press(menu, InputType::PointerRight, 73, 34);
    CHECK(tetris.last.x == 71 && tetris.last.y == 29);
    const auto clicks = tetris.handled;
    press(menu, InputType::PointerLeft, 74, 34); CHECK(tetris.handled == clicks);
    press(menu, InputType::PointerMove, 74, 34); CHECK(tetris.last.x == -1 && tetris.last.y == -1);
    press(menu, InputType::Back); press(menu, InputType::Confirm);
    press(menu, InputType::Down); press(menu, InputType::Confirm);
    CHECK(tetris.settings.seed != firstSeed);
    tetris.current.state = State::Lost; menu.update(0); menu.update(1);
    CHECK(view(menu).find("Партия окончена") != std::string::npos);
    press(menu, InputType::Back);
    press(menu, InputType::Down); press(menu, InputType::Confirm); // Mines settings.
    press(menu, InputType::PointerLeft, 64, 19); // Size: 8 -> 10.
    press(menu, InputType::PointerLeft, 42, 19); // Back to 8.
    press(menu, InputType::PointerLeft, 64, 19); // Again 10.
    press(menu, InputType::PointerLeft, 30, 23);
    CHECK(mines.settings.level == 2);
    menu.update(0);
    mines.current.firstX = 3; mines.current.firstY = 4; mines.current.state = State::Won;
    menu.update(1); CHECK(view(menu).find("Следующий уровень") != std::string::npos);
    const auto oldSeed = mines.settings.seed;
    press(menu, InputType::Restart);
    CHECK(mines.settings.firstX == 3 && mines.settings.firstY == 4 && mines.settings.seed == oldSeed);
    menu.update(0); mines.current.state = State::Won; menu.update(1);
    press(menu, InputType::Confirm);
    CHECK(mines.settings.level == 3 && mines.settings.seed == oldSeed + 0x9e3779b9u);
    CHECK(mines.settings.firstX == -1 && mines.settings.firstY == -1);
    press(menu, InputType::Back);
    press(menu, InputType::PointerLeft, 30, 21); // Play by code.
    CHECK(menu.textMode());
    for (char c : std::string("P2-4SW5-CD0J-0228E")) {
        Input input; input.type = InputType::Text; input.character = c; menu.handle(input);
    }
    press(menu, InputType::Confirm); CHECK(!menu.textMode());
    CHECK(view(menu).find("Код проверен") != std::string::npos);
    press(menu, InputType::Confirm);
    CHECK(pong.settings.level == 3 && pong.settings.seed == 0x12345678);
    menu.update(0); pong.current.state = State::Won; menu.update(1);
    CHECK(view(menu).find("Соперник переигран") != std::string::npos);
    CHECK(view(menu).find("Экстремальный уровень") != std::string::npos);
    press(menu, InputType::Confirm); CHECK(pong.settings.level == 4);
    menu.update(0); pong.current.state = State::Won; menu.update(1);
    CHECK(view(menu).find("Экстремальный уровень") == std::string::npos);
    const auto handledBeforeCopy = pong.handled;
    press(menu, InputType::Copy);
    CHECK(menu.takeCopyRequest().substr(0, 3) == "P2-");
    CHECK(menu.takeCopyRequest().empty() && pong.handled == handledBeforeCopy);
    press(menu, InputType::CopySucceeded); CHECK(view(menu).find("Скопировано") != std::string::npos);
}

/** @brief Проверяет обе стрелки мыши во всех настройках, включая границы. */
static void mouseSettingsTests() {
    for (int id = 0; id < 3; ++id) {
        Fake tetris, mines, pong;
        Menu menu(tetris, mines, pong, seed);
        const auto main = view(menu);
        CHECK(main.find("01 / КОВБОЙ ПАША") != std::string::npos);
        CHECK(main.find("02 / КОВБОЙ ТЁМА") != std::string::npos);
        CHECK(main.find("03 / КОВБОЙ ДИМА") != std::string::npos);
        CHECK(main.find("разработке") == std::string::npos);
        press(menu, InputType::PointerLeft, 30, 15 + 2 * id);
        for (int i = 0; i < 4; ++i) press(menu, InputType::PointerLeft, 42, 16);
        press(menu, InputType::PointerLeft, 30, 23);
        Fake* game = id == 0 ? &tetris : id == 1 ? &mines : &pong;
        CHECK(game->settings.difficulty == Difficulty::Easy);
        press(menu, InputType::Back); press(menu, InputType::Confirm);
        for (int i = 0; i < 4; ++i) press(menu, InputType::PointerLeft, 64, 16);
        if (id) {
            for (int i = 0; i < 6; ++i) press(menu, InputType::PointerLeft, 64, 19);
            press(menu, InputType::PointerLeft, 42, 19);
        }
        press(menu, InputType::PointerLeft, 30, 23);
        CHECK(game->settings.difficulty == Difficulty::Hard);
        CHECK(game->settings.level == (id == 1 ? 4u : id == 2 ? 2u : 1u));
    }
}

/** @brief Проверяет отсечение вывода, отсутствие лишних кадров и чистоту draw. */
static void screenTests() {
    std::string bytes;
    Screen screen(capture, &bytes);
    screen.begin();
    screen.text(-2, 0, "XXVISIBLE");
    screen.text(72, 0, "OUTSIDE");
    screen.panelText(30, "OUTSIDE");
    screen.panelTextAt(-2, 1, "XXPANEL");
    screen.panelTextAt(21, 1, "OUTSIDE");
    screen.canvasText(99, 39, "OUTSIDE");
    CHECK(screen.plain().find("PANEL") != std::string::npos);
    CHECK(screen.plain().find("VISIBLE") != std::string::npos);
    CHECK(screen.plain().find("OUTSIDE") == std::string::npos);
    screen.flush(); CHECK(!bytes.empty()); bytes.clear(); screen.flush(); CHECK(bytes.empty());
    screen.invalidate(); screen.flush(); CHECK(!bytes.empty()); bytes.clear();
    screen.requestPaste(); CHECK(bytes == "\x1b]52;c;?\a"); bytes.clear();
    Tetris tetris; Minesweeper mines; Pong pong;
    Menu menu(tetris, mines, pong, seed);
    menu.draw(screen); const auto first = screen.plain(); screen.flush(); bytes.clear();
    menu.draw(screen); screen.flush(); CHECK(bytes.empty() && screen.plain() == first);
    // Scenery moves outside the notice, is occluded by it, and wraps without gameplay.
    CHECK(first.find("\\|/") != std::string::npos);
    menu.update(160); CHECK(view(menu) != first);
    menu.update(19 * 160);
    CHECK(view(menu).find("\\|/") == std::string::npos);
    CHECK(view(menu).find("-*-") == std::string::npos);
    menu.update(66 * 160); CHECK(view(menu).find("\\|/") != std::string::npos);
    menu.update(20 * 160); CHECK(view(menu) == first);
    CHECK(tetris.result().timeMs == 0 && pong.result().timeMs == 0);
    CHECK(Screen::boardX(16) == 20 && Screen::boardY(16) == 7);
}


/** @brief Проверяет запрос буфера, замену текста, отказ и границу бонусного уровня. */
static void pasteAndBonusTests() {
    Fake tetris, mines, pong;
    Menu menu(tetris, mines, pong, seed);
    press(menu, InputType::PointerLeft, 30, 21);
    press(menu, InputType::Up); CHECK(menu.takePasteRequest()); CHECK(!menu.takePasteRequest());
    press(menu, InputType::PasteBegin);
    for (char c : std::string("P2-4SW5-CD0J-0228E")) {
        Input input; input.type = InputType::PasteText; input.character = c; menu.handle(input);
    }
    press(menu, InputType::PasteEnd);
    CHECK(view(menu).find("P2-4SW5-CD0J-0228E") != std::string::npos);
    CHECK(pong.starts == 0);
    press(menu, InputType::Paste); CHECK(menu.takePasteRequest());
    press(menu, InputType::PasteFailed);
    CHECK(view(menu).find("P2-4SW5-CD0J-0228E") != std::string::npos);
    press(menu, InputType::Confirm); CHECK(view(menu).find("Код проверен") != std::string::npos);
    for (unsigned difficulty = 0; difficulty < 3; ++difficulty) {
        for (unsigned level = 1; level <= 3; ++level) {
            Fake a, b, c;
            Menu trial(a, b, c, seed);
            press(trial, InputType::PointerLeft, 30, 21);
            Settings settings; settings.level = level; settings.difficulty = static_cast<Difficulty>(difficulty);
            for (char ch : encodeCode(GameId::Pong, settings)) {
                Input input; input.type = InputType::Text; input.character = ch; trial.handle(input);
            }
            press(trial, InputType::Confirm); press(trial, InputType::Confirm);
            trial.update(0); c.current.state = State::Won; trial.update(1);
            CHECK((view(trial).find("Экстремальный уровень") != std::string::npos) == (level == 3));
            press(trial, InputType::Restart); trial.update(0); c.current.state = State::Lost; trial.update(1);
            CHECK(view(trial).find("Проиграл дуэль") != std::string::npos);
        }
    }
}

/** @brief Выполняет все проверки общей основы. */
int main() {
    inputTests(); pasteAndBonusTests(); codeTests(); menuTests(); mouseSettingsTests(); screenTests();
    std::cout << "PASS: input, code vectors, pause/restart, pointer bounds, levels, renderer\n";
}
