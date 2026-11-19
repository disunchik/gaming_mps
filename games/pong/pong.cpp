/** @file pong.cpp @brief Управление и представление пин-понга через Screen. */
#include "pong.h"
#include "input.h"
#include "screen.h"
#include <algorithm>

void Pong::start(const Settings& settings) { model_.start(settings); }
void Pong::handle(const Input& input) {
    if (input.type == InputType::Left) model_.move(-1);
    else if (input.type == InputType::Right) model_.move(1);
    else if (input.type == InputType::HeldLeft) model_.hold(-1);
    else if (input.type == InputType::HeldRight) model_.hold(1);
    else if (input.type == InputType::HorizontalReleased) model_.hold(0);
    else if (input.type == InputType::PointerMove || input.type == InputType::PointerLeft) model_.point(input.x);
}
void Pong::update(std::uint32_t ms) { model_.update(ms); }
Result Pong::result() const {
    Result result;
    result.state = model_.state;
    result.score = model_.playerScore;
    result.level = model_.settings.level;
    result.timeMs = model_.timeMs;
    return result;
}
void Pong::draw(Screen& screen) const {
    const auto paddle = [&screen](int center, int y, Color color) {
        const int left = std::clamp((center - PongModel::half) * 2 / PongModel::unit, 0, Screen::width - 10);
        screen.text(left, y, "==========", color);
    };
    for (int x = 1; x < Screen::width; x += 3) screen.text(x, 14, "·", Theme::border);
    for (int i = static_cast<int>(model_.trailCount) - 1; i >= 0; --i)
        screen.text(std::clamp(model_.trail[i].x * 2 / PongModel::unit, 0, Screen::width - 1),
                    std::clamp(model_.trail[i].y / PongModel::unit, 0, Screen::height - 1), "·",
                    i == 0 ? Theme::muted : Theme::border);
    for (unsigned i = 0; i < model_.botCount(); ++i) paddle(model_.bots[i], 1, Theme::orange);
    paddle(model_.player, 28, Theme::cyan);
    screen.text(std::clamp(model_.ball.x * 2 / PongModel::unit, 0, Screen::width - 1),
                std::clamp(model_.ball.y / PongModel::unit, 0, Screen::height - 1), "◉", Theme::accent);
    if (model_.serveMs) screen.text(29, 16, "Подача…", Theme::accent);
    screen.panelText(0, "┌───────────────────┐", Theme::wood);
    for (int row = 1; row < 12; ++row) {
        screen.panelTextAt(0, row, "│", Theme::wood);
        screen.panelTextAt(20, row, "│", Theme::wood);
    }
    screen.panelText(12, "└───────────────────┘", Theme::wood);
    screen.panelTextAt(5, 2, "СЧЁТ САЛУНА", Theme::white);
    screen.panelTextAt(3, 4, "ТЫ", Theme::white);
    screen.panelTextAt(13, 4, model_.botCount() == 2 ? "БОТЫ" : "БОТ", Theme::white);
    // Broad, monochrome chalk strokes; score is always 0..5.
    static constexpr const char* digits[6][5] = {
        {"███", "█ █", "█ █", "█ █", "███"}, {" █ ", "██ ", " █ ", " █ ", "███"},
        {"███", "  █", "███", "█  ", "███"}, {"███", "  █", "███", "  █", "███"},
        {"█ █", "█ █", "███", "  █", "  █"}, {"███", "█  ", "███", "  █", "███"}
    };
    for (int row = 0; row < 5; ++row) {
        screen.panelTextAt(3, 6 + row, digits[std::min(model_.playerScore, 5u)][row], Theme::white);
        screen.panelTextAt(14, 6 + row, digits[std::min(model_.botScore, 5u)][row], Theme::white);
    }
    screen.panelTextAt(10, 7, "·", Theme::white);
    screen.panelTextAt(10, 9, "·", Theme::white);
    screen.panelNumber(15, "Уровень:", model_.settings.level);
    screen.panelText(18, "Сложность", Theme::muted);
    const char* names[] = {"Лёгкая", "Средняя", "Сложная"};
    screen.panelText(19, names[static_cast<unsigned>(model_.settings.difficulty)]);
    screen.panelNumber(22, "Ударов:", model_.hits);
    if (model_.flashMs)
        screen.panelText(25, model_.lastPointHuman ? "Твоё очко!" : "Очко соперника", model_.lastPointHuman ? Theme::cyan : Theme::orange);
    screen.panelText(28, "Матч до 5 очков", Theme::muted);
}
