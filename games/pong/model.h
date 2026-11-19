/** @file model.h @brief Детерминированная физика пин-понга без терминала и SDK. */
#pragma once
#include "game.h"
#include <array>

/** @brief Чистая модель; координаты в квадратных единицах (две колонки на единицу). */
class PongModel {
public:
    static constexpr int unit = 10000; ///< Точность фиксированных координат.
    static constexpr int arena = 36 * unit; ///< Ширина 72 символьных колонок.
    static constexpr int half = 25000; ///< Половина длины ракетки, пять единиц целиком.
    /** @brief Положение и скорость точечного мяча. */
    struct Ball {
        int x = arena / 2; ///< Горизонтальная позиция.
        int y = 15 * unit; ///< Вертикальная позиция.
        int vx = 0; ///< Горизонтальная скорость в единицах/с.
        int vy = 0; ///< Вертикальная скорость в единицах/с.
    };
    Settings settings{}; ///< Стартовые условия, не текущий прогресс.
    State state = State::Playing; ///< Исход матча.
    Ball ball{}; ///< Текущий мяч.
    int player = arena / 2; ///< Центр нижней ракетки.
    int target = arena / 2; ///< Цель игрока.
    std::array<int, 2> bots{{arena / 2, arena / 2}}; ///< Центры ракеток компьютера.
    std::array<int, 2> errors{}; ///< Постоянная погрешность текущего подлёта.
    unsigned playerScore = 0; ///< Очки человека.
    unsigned botScore = 0; ///< Общие очки компьютера.
    unsigned hits = 0; ///< Удары текущего розыгрыша.
    std::uint64_t timeMs = 0; ///< Игровое время с начала матча, включая подачу.
    std::uint64_t rallyMs = 0; ///< Время полёта в текущем розыгрыше.
    unsigned flashMs = 0; ///< Оставшаяся подсветка последнего очка.
    bool lastPointHuman = false; ///< Автор последнего очка.
    /** @brief Координаты одной точки декоративного следа. */
    struct Point { int x = 0; ///< Позиция по горизонтали.
                   int y = 0; ///< Позиция по вертикали.
    };
    std::array<Point, 3> trail{}; ///< Последние положения мяча через 50 мс.
    unsigned trailCount = 0; ///< Число заполненных точек следа.
    unsigned serveMs = 700; ///< Оставшееся ожидание подачи.
    /** @brief Сбрасывает весь матч и генератор, в том числе при seed=0. */
    void start(const Settings& value);
    /** @brief Продвигает модель фиксированными шагами по 10 мс. */
    void update(std::uint32_t ms);
    /** @brief Задаёт цель в символьных координатах поля, ограничивая её. */
    void point(int column);
    /** @brief Сдвигает цель на одну квадратную единицу в выбранную сторону. */
    void move(int direction);
    /** @brief Количество активных ботов. */
    unsigned botCount() const;
    /** @brief Начальная скорость мяча, в фиксированных единицах/с. */
    int baseSpeed() const;
    /** @brief Текущий множитель скорости, где 10000 означает x1. */
    int multiplier() const;
    /** @brief Отражает произвольную горизонтальную позицию от боковых стен. */
    static int reflect(std::int64_t x);
private:
    std::uint32_t random_ = 1; ///< Собственное состояние xorshift32.
    unsigned trailClock_ = 0; ///< Фаза записи следа в миллисекундах.
    std::uint64_t pending_ = 0; ///< Неразобранные миллисекунды до шага физики.
    /** @brief Следующее детерминированное случайное число. */
    std::uint32_t random();
    /** @brief Погрешность прицеливания одного бота. */
    int aimError();
    /** @brief Обновляет погрешность при начале нового подлёта. */
    void aim();
    /** @brief Назначает угол от вертикали по точке контакта или подаче. */
    void direction(int angle, int vertical);
    /** @brief Начинает подачу в сторону пропустившего очко. */
    void serve(int vertical);
    /** @brief Засчитывает очко и определяет конец матча. */
    void goal(bool human);
    /** @brief Один шаг физики, движения ракеток и времени. */
    void step();
};
