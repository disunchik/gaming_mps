/** @file minesweeper.h @brief Состояние и интерфейс игры «Сапёр». */
#pragma once
#include "game.h"
#include <array>

/** @brief Одиночный сапёр, управляемый общей оболочкой. */
class Minesweeper final : public Game {
public:
    /** @brief Сбрасывает партию и воспроизводит первый ход, если он задан. */
    void start(const Settings& settings) override;
    /** @brief Перемещает курсор, открывает клетку или переключает флаг. */
    void handle(const Input& input) override;
    /** @brief Накапливает игровое время между первым открытием и исходом. */
    void update(std::uint32_t ms) override;
    /** @brief Рисует поле и боковую панель в общей палитре. */
    void draw(Screen& screen) const override;
    /** @brief Возвращает исход, время, уровень и фактический первый ход. */
    Result result() const override;

private:
    /** @brief Логические координаты клетки. Пара (-1,-1) означает отсутствие выбора. */
    struct Point {
        int x = -1; ///< Колонка.
        int y = -1; ///< Строка.
        /** @brief Проверяет совпадение двух логических позиций. */
        bool operator==(Point other) const { return x == other.x && y == other.y; }
    };

    /** @brief Положение и размер всей доски в символах терминала. */
    struct Layout {
        Point origin; ///< Левый верхний угол клеток внутри Screen.
        int width; ///< Ширина доски без рамки.
        int height; ///< Высота доски без рамки.
    };

    /** @brief Содержимое и видимое состояние одной клетки. */
    struct Cell {
        std::uint8_t adjacent = 0; ///< Число мин среди восьми соседей.
        bool mine = false; ///< Наличие мины.
        bool open = false; ///< Клетка уже открыта.
        bool flag = false; ///< Клетка отмечена игроком.
    };

    static constexpr int maxCells = 16 * 16; ///< Ёмкость поля и очереди раскрытия.
    std::array<Cell, maxCells> cells_{}; ///< Клетки текущего поля, по строкам.
    Result result_{}; ///< Результат, который считывает оболочка.
    std::uint32_t seed_ = 0; ///< Начальное число для раскладки.
    int size_ = 8; ///< Сторона квадратного поля.
    int mines_ = 10; ///< Заданное число мин.
    int flags_ = 0; ///< Число флагов от нуля до 256.
    int safeLeft_ = 0; ///< Сколько безопасных клеток ещё закрыто.
    Point cursor_{}; ///< Текущая позиция клавиатурного курсора.
    Point hover_{}; ///< Клетка под указателем, если он находится на доске.

    /** @brief Проверяет границы активной доски. */
    bool contains(Point position) const;
    /** @brief Преобразует логическую позицию в индекс массива. */
    int index(Point position) const;
    /** @brief Выбирает масштаб для текущего размера и центрирует поле. */
    Layout boardLayout() const;
    /** @brief Преобразует локальные символьные координаты мыши в клетку. */
    Point pointerCell(const Input& input) const;
    /** @brief Передаёт индексы всех существующих соседей одному обработчику. */
    template<typename Action>
    void forNeighbors(int cellIndex, Action action) const;
    /** @brief Расставляет мины и вычисляет числа, исключая клетку первого хода. */
    void placeMines(Point first);
    /** @brief Открывает безопасную клетку и связанную область нулей. */
    void openSafeCells(int first);
    /** @brief Выполняет допустимое открытие и проверяет исход партии. */
    void open(Point position);
    /** @brief Открывает соседей цифры при совпадении числа флагов. */
    void openNeighbors(int cellIndex);
    /** @brief Возвращает число флагов у открытой цифры. */
    int neighboringFlags(int cellIndex) const;
    /** @brief Переключает флаг только на закрытой клетке. */
    void toggleFlag(Point position);
    /** @brief Рисует одну клетку с её числом или отметкой. */
    void drawCell(Screen& screen, const Layout& layout, Point position,
                  bool selected, bool preview, bool ready) const;
};
