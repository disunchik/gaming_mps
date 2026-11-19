/** @file model.cpp @brief Фиксированный шаг, отражения, перехват и счёт пин-понга. */
#include "model.h"
#include <algorithm>
#include <cstdlib>

/** @brief Приближает позицию к цели с заданным пределом скорости. */
static int approach(int position, int target, int speed) {
    const int distance = speed * PongModel::unit / 100;
    return position + std::clamp(target - position, -distance, distance);
}
void PongModel::start(const Settings& value) {
    settings = value;
    state = State::Playing;
    playerScore = botScore = hits = 0;
    timeMs = rallyMs = pending_ = 0;
    player = target = arena / 2;
    held_ = 0;
    bots = botCount() == 2 ? std::array<int, 2>{{arena / 4, 3 * arena / 4}} : std::array<int, 2>{{arena / 2, arena / 2}};
    flashMs = 0; lastPointHuman = false;
    random_ = settings.seed ? settings.seed : 0x6d2b79f5u;
    serve(1);
}
std::uint32_t PongModel::random() {
    random_ ^= random_ << 13;
    random_ ^= random_ >> 17;
    random_ ^= random_ << 5;
    return random_;
}
unsigned PongModel::botCount() const { return settings.level == 4 ? 2 : 1; }
int PongModel::baseSpeed() const {
    static constexpr int speeds[] = {12, 16, 20, 20};
    return speeds[std::clamp<std::uint32_t>(settings.level, 1, 4) - 1] * unit;
}
int PongModel::multiplier() const {
    if (rallyMs < 10000) return unit;
    int value = unit;
    const auto count = std::min<std::uint64_t>(8, 1 + (rallyMs - 10000) / 5000);
    for (unsigned i = 0; i < count; ++i) value = std::min(2 * unit, (value * 11 + 5) / 10);
    return value;
}
int PongModel::aimError() {
    if (settings.difficulty == Difficulty::Hard && random() % 8 == 0) {
        const int amount = 3 * unit + random() % (2 * unit + 1);
        return random() & 1 ? amount : -amount;
    }
    const int range = settings.difficulty == Difficulty::Easy ? 6 * unit
                    : settings.difficulty == Difficulty::Normal ? 3 * unit : unit;
    return static_cast<int>(random() % (2 * range + 1)) - range;
}
void PongModel::aim() { for (auto& error : errors) error = aimError(); }
int PongModel::reflect(std::int64_t x) {
    x %= 2 * arena;
    if (x < 0) x += 2 * arena;
    return static_cast<int>(x <= arena ? x : 2 * arena - x);
}
void PongModel::direction(int angle, int vertical) {
    // Lookup tables avoid platform-dependent trigonometry and float rounding.
    static constexpr int sine[] = {0, 1736, 3420, 5000, 6428, 7660, 8660};
    static constexpr int cosine[] = {10000, 9848, 9397, 8660, 7660, 6428, 5000};
    const int index = std::clamp((std::abs(angle) + 5) / 10, 1, 6);
    const int speed = static_cast<int>(static_cast<std::int64_t>(baseSpeed()) * multiplier() / unit);
    ball.vx = speed * static_cast<std::int64_t>(sine[index]) / unit * (angle < 0 ? -1 : 1);
    ball.vy = speed * static_cast<std::int64_t>(cosine[index]) / unit * vertical;
}
void PongModel::serve(int vertical) {
    hits = 0;
    rallyMs = 0;
    serveMs = 700;
    trailCount = trailClock_ = 0;
    trail = {};
    ball = {};
    const int angle = (random() & 1 ? 1 : -1) * (20 + 10 * (random() % 3));
    direction(angle, vertical);
    aim();
}
void PongModel::goal(bool human) {
    flashMs = 300; lastPointHuman = human;
    if (human) ++playerScore;
    else ++botScore;
    if (playerScore == 5 || botScore == 5) {
        state = playerScore == 5 ? State::Won : State::Lost;
        return;
    }
    serve(human ? -1 : 1);
}
void PongModel::point(int column) {
    if (state == State::Playing && column >= 0 && column < 72) {
        held_ = 0;
        target = std::clamp(column * unit / 2 + unit / 4, half, arena - half);
    }
}
void PongModel::move(int direction) {
    if (state == State::Playing && !held_)
        target = std::clamp(target + direction * 2 * unit, half, arena - half);
}
void PongModel::hold(int direction) {
    if (!direction && held_) target = player;
    held_ = std::clamp(direction, -1, 1);
}
void PongModel::update(std::uint32_t ms) {
    if (state != State::Playing) return;
    pending_ += ms;
    while (pending_ >= 10 && state == State::Playing) {
        pending_ -= 10;
        step();
    }
}
void PongModel::step() {
    timeMs += 10;
    if (flashMs) flashMs -= 10;
    if (held_) target = held_ < 0 ? half : arena - half;
    player = approach(player, target, 27);
    for (unsigned i = 0; i < botCount(); ++i) {
        int destination = botCount() == 2 ? (2 * i + 1) * arena / 4 : arena / 2;
        if (ball.vy < 0 && !serveMs) {
            destination = reflect(ball.x + static_cast<std::int64_t>(ball.vx) * (unit - ball.y) / ball.vy) + errors[i];
        }
        const int low = botCount() == 2 ? i * arena / 2 + half : half;
        const int high = botCount() == 2 ? (i + 1) * arena / 2 - half : arena - half;
        bots[i] = approach(bots[i], std::clamp(destination, low, high), 18);
    }
    if (serveMs) { serveMs -= 10; return; }
    const int before = multiplier();
    rallyMs += 10;
    const int after = multiplier();
    if (after != before) {
        ball.vx = static_cast<int>(static_cast<std::int64_t>(ball.vx) * after / before);
        ball.vy = static_cast<int>(static_cast<std::int64_t>(ball.vy) * after / before);
    }
    const int oldX = ball.x, oldY = ball.y, oldVx = ball.vx;
    int x = ball.x + ball.vx / 100;
    if (x < 0 || x > arena) { x = reflect(x); ball.vx = -ball.vx; }
    ball.x = x;
    ball.y += ball.vy / 100;
    const int line = ball.vy < 0 ? unit : 28 * unit;
    if ((ball.vy < 0 && oldY > line && ball.y <= line) ||
        (ball.vy > 0 && oldY < line && ball.y >= line)) {
        const int contact = reflect(oldX + static_cast<std::int64_t>(oldVx) * (line - oldY) / ball.vy);
        int center = player;
        bool hit = false;
        if (ball.vy > 0) hit = std::abs(contact - player) <= half;
        else for (unsigned i = 0; i < botCount(); ++i) {
            if (std::abs(contact - bots[i]) <= half) { center = bots[i]; hit = true; break; }
        }
        if (hit) {
            const bool upward = ball.vy > 0;
            int angle = (contact - center) * 60 / half;
            if (!angle) angle = ball.vx < 0 ? -10 : 10;
            ball.x = contact;
            ball.y = line;
            direction(angle, upward ? -1 : 1);
            ++hits;
            if (upward) aim();
        }
    }
    trailClock_ += 10;
    if (trailClock_ == 50) {
        trailClock_ = 0;
        for (int i = 2; i > 0; --i) trail[i] = trail[i - 1];
        trail[0] = {oldX, oldY};
        trailCount = std::min(3u, trailCount + 1);
    }
    if (ball.y < 0) goal(true);
    else if (ball.y >= 30 * unit) goal(false);
}
