/** @file game.h @brief Общие настройки, результат и контракт игр. */
#pragma once
#include <cstdint>

struct Input;
class Screen;

/** @brief Идентификаторы игр в коде испытания P2. */
enum class GameId : std::uint8_t { Tetris = 0, Minesweeper = 1, Pong = 2 };
/** @brief Три доступные сложности. */
enum class Difficulty : std::uint8_t { Easy = 0, Normal = 1, Hard = 2 };
/** @brief Состояние партии или текущего конечного уровня. */
enum class State : std::uint8_t { Playing, Won, Lost };

/** @brief Полные стартовые условия одной партии или уровня. */
struct Settings {
    Difficulty difficulty = Difficulty::Normal; ///< Выбранная сложность.
    std::uint32_t level = 1; ///< Начальный уровень, начиная с 1.
    std::uint32_t seed = 0; ///< Начальное состояние генератора; 0 допустим.
    std::int16_t firstX = -1; ///< Первая колонка сапёра; -1, если ещё неизвестна.
    std::int16_t firstY = -1; ///< Первая строка сапёра; -1, если ещё неизвестна.
};

/** @brief Текущее состояние игры для общей оболочки. */
struct Result {
    State state = State::Playing; ///< Продолжение, победа на уровне или поражение.
    std::uint32_t score = 0; ///< Счёт; у сапёра остаётся нулевым.
    std::uint32_t level = 1; ///< Текущий уровень, начиная с 1.
    std::uint64_t timeMs = 0; ///< Прошедшее игровое время без пауз.
    std::int16_t firstX = -1; ///< Фактическая первая колонка сапёра.
    std::int16_t firstY = -1; ///< Фактическая первая строка сапёра.
};

/** @brief Игра, управляемая общим циклом приложения. */
class Game {
public:
    /** @brief Освобождает ресурсы конкретной игры. */
    virtual ~Game() = default;
    /** @brief Полностью начинает партию с переданными стартовыми условиями. */
    virtual void start(const Settings& settings) = 0;
    /** @brief Обрабатывает одно готовое игровое событие. */
    virtual void handle(const Input& input) = 0;
    /** @brief Продвигает логику на ms миллисекунд игрового времени. */
    virtual void update(std::uint32_t ms) = 0;
    /** @brief Рисует состояние, не меняя игру и генератор случайных чисел. */
    virtual void draw(Screen& screen) const = 0;
    /** @brief Возвращает результат без изменения состояния игры. */
    virtual Result result() const = 0;
};
