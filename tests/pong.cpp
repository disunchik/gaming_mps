/** @file pong.cpp @brief Поведенческие проверки механики пин-понга. */
#include "model.h"
#include "pong.h"
#include "screen.h"
#include <cstdlib>
#include <iostream>

/** @brief Завершает тест с номером нарушенного условия. */
#define CHECK(x) do { if (!(x)) { std::cerr << "PONG FAIL " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (false)
/** @brief Проверяет детерминизм, границы, столкновения и исход матча. */
int main() {
    PongModel m;
    Settings settings; settings.seed = 0;
    m.start(settings);
    CHECK(m.state == State::Playing && m.playerScore == 0 && m.serveMs == 700);
    m.point(0); m.update(1000); CHECK(m.player >= m.half && m.player < m.arena / 2);
    m.point(71); m.update(2000); CHECK(m.player <= m.arena - m.half);
    m.start(settings); m.serveMs = 0;
    m.ball = {100, 15 * m.unit, -120000, 120000};
    m.update(10); CHECK(m.ball.x >= 0 && m.ball.vx > 0);
    m.start(settings); m.serveMs = 0;
    m.ball = {m.player, 28 * m.unit - 500, 10000, 120000};
    m.update(10); CHECK(m.ball.vy < 0 && m.hits == 1);
    CHECK(m.ball.vx != 0); // A center hit must not produce a vertical stalemate.
    m.start(settings); m.serveMs = 0;
    m.ball = {10000, 28 * m.unit - 500, 10000, 120000};
    m.update(10); CHECK(m.ball.vy > 0 && m.hits == 0);
    m.ball.y = 30 * m.unit - 100;
    m.update(10); CHECK(m.botScore == 1 && m.serveMs == 700 && m.rallyMs == 0);
    m.start(settings); m.serveMs = 0; m.playerScore = 4;
    m.ball = {m.unit, 100, 0, -120000};
    m.update(10); CHECK(m.state == State::Won && m.playerScore == 5);
    const auto stopped = m.timeMs;
    m.update(10000); m.move(1); CHECK(m.timeMs == stopped);
    m.start(settings); m.serveMs = 0; m.botScore = 4;
    m.ball = {m.unit, 30 * m.unit - 100, 0, 120000};
    m.update(10); CHECK(m.state == State::Lost && m.botScore == 5);
    PongModel a, b;
    a.start(settings); b.start(settings);
    for (int i = 0; i < 50; ++i) { a.move(i % 3 - 1); b.move(i % 3 - 1); a.update(100); for (int j = 0; j < 10; ++j) b.update(10); }
    CHECK(a.ball.x == b.ball.x && a.ball.y == b.ball.y && a.botScore == b.botScore && a.timeMs == b.timeMs);
    a.start(settings); b.start(settings); a.update(5000); b.update(5000);
    CHECK(a.ball.x == b.ball.x && a.ball.vx == b.ball.vx && a.bots == b.bots);
    Pong game; game.start(settings); Screen screen;
    game.update(1000); const auto before = game.result();
    game.draw(screen); const auto frame = screen.plain(); game.draw(screen);
    CHECK(frame == screen.plain() && before.timeMs == game.result().timeMs);
    CHECK(frame.find("seed:") == std::string::npos && frame.find("События:") == std::string::npos);
    m.start(settings); m.serveMs = 0; m.rallyMs = 9990;
    m.ball = {18 * m.unit, 15 * m.unit, m.unit, m.unit};
    m.update(10); CHECK(m.multiplier() == 11000 && m.ball.vy == 11000);
    m.rallyMs = 14990; m.update(10); CHECK(m.multiplier() == 12100);
    m.rallyMs = 45000; CHECK(m.multiplier() == 20000);
    m.rallyMs = 100000; CHECK(m.multiplier() == 20000);
    m.ball.y = 30 * m.unit - 1; m.ball.vy = 120000; m.update(10);
    CHECK(m.multiplier() == m.unit && m.rallyMs == 0 && m.trailCount == 0);
    for (unsigned level = 1; level <= 4; ++level) {
        for (unsigned difficulty = 0; difficulty < 3; ++difficulty) {
            Settings config; config.level = level; config.difficulty = static_cast<Difficulty>(difficulty);
            m.start(config); CHECK(m.baseSpeed() == (level == 1 ? 12 : level == 2 ? 16 : 20) * m.unit);
            CHECK(m.botCount() == (level == 4 ? 2u : 1u));
            m.update(1000); CHECK(m.ball.x >= 0 && m.ball.x <= m.arena);
            m.point(0);
            for (int tick = 0; tick < 1200 && m.state == State::Playing; ++tick) m.update(100);
            CHECK(m.state != State::Playing); // Natural play with a stationary edge paddle must finish.
            if (level == 4) CHECK(m.bots[0] + m.half <= m.arena / 2 && m.bots[1] - m.half >= m.arena / 2);
        }
    }
    std::uint64_t sums[3]{}; unsigned rare = 0;
    for (unsigned difficulty = 0; difficulty < 3; ++difficulty) {
        for (unsigned seed = 0; seed < 1000; ++seed) {
            Settings config; config.seed = seed; config.difficulty = static_cast<Difficulty>(difficulty);
            m.start(config); sums[difficulty] += std::abs(m.errors[0]);
            if (difficulty == 2 && std::abs(m.errors[0]) >= 3 * m.unit) ++rare;
        }
    }
    CHECK(sums[0] > sums[1] && sums[1] > sums[2] && rare > 50 && rare < 200);
    m.start(settings); m.serveMs = 0;
    m.ball = {m.player + m.half - 1000, 28 * m.unit - 100, 0, 40 * m.unit};
    m.update(10); CHECK(m.ball.vy < 0 && m.hits == 1);
    std::cout << "PASS: acceleration thresholds/cap/reset, 12 configurations, two-bot bounds and difficulty accuracy\n";
    std::cout << "PASS: Pong collisions, scoring, replay, input bounds, fixed-step determinism and pure draw\n";
}
