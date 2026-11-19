/** @file stub.cpp @brief Диагностический экран, общий для трёх игровых модулей. */
#include "stub.h"
#include "menu.h"
#include "screen.h"

Stub::Stub(GameId game) : game_(game) {}
void Stub::start(const Settings& settings) { settings_ = settings; input_ = {}; events_ = 0; time_ = 0; }
void Stub::handle(const Input& input) { input_ = input; ++events_; }
void Stub::update(std::uint32_t ms) { time_ += ms; }
Result Stub::result() const {
    Result result;
    result.level = settings_.level;
    result.timeMs = time_;
    result.firstX = settings_.firstX;
    result.firstY = settings_.firstY;
    return result;
}
void Stub::draw(Screen& screen) const {
    if (game_ == GameId::Minesweeper) {
        const int size = 6 + 2 * static_cast<int>(settings_.level);
        const int left = Screen::boardX(size), top = Screen::boardY(size);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                const bool pointer = input_.x >= left && input_.y >= top && input_.x < left + 2 * size
                    && input_.y < top + size && (input_.x - left) / 2 == x && input_.y - top == y;
                screen.cell(left + 2 * x, top + y, Theme::muted,
                            pointer ? Theme::border : Theme::background, U'·');
            }
    } else {
        screen.text(5, 5, gameName(game_), Theme::accent);
    }
    screen.panelText(0, gameName(game_), Theme::accent);
    screen.panelNumber(3, "Уровень:", settings_.level);
    screen.panelText(6, "Сложность", Theme::muted);
    screen.panelText(7, difficultyName(settings_.difficulty));
}
