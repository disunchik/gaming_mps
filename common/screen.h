/** @file screen.h @brief Публичные методы отрисовки, доступные играм. */
#pragma once
#include <cstdint>
#include <string_view>
#include <string>
#include <array>
#include "theme.h"

/** @brief Общая отрисовка; координаты методов поля локальны, в символах. */
class Screen {
public:
    /** @brief Приёмник UTF-8/ANSI-байтов готового экрана. */
    using Output = void (*)(const char*, std::size_t, void*);
    /** @brief Создаёт экран с выбранным приёмником; nullptr использует stdout. */
    explicit Screen(Output output = nullptr, void* context = nullptr);
    /** @brief Начинает новый кадр с общим фоном. */
    void begin();
    /** @brief Отправляет только изменившиеся участки кадра. */
    void flush();
    /** @brief Запрашивает полный кадр после подключения нового терминала. */
    void invalidate();
    /** @brief Просит запускатель прочитать код из буфера ПК; вне draw. */
    void requestPaste();
    /** @brief Отправляет запрос копирования через OSC52, вне отрисовки кадра. */
    void copyText(std::string_view text);
    /** @brief Рисует текст в абсолютных координатах канваса; для оболочки. */
    void canvasText(int x, int y, std::string_view value,
                    Color foreground = Theme::text, Color background = Theme::background);
    /** @brief Рисует рамку канваса; для оболочки. */
    void canvasFrame(int x, int y, int w, int h, Color foreground = Theme::border);
    /** @brief Заполняет прямоугольник канваса; для оболочки. */
    void canvasFill(int x, int y, int w, int h, Color background);
    /** @brief Возвращает снимок символов канваса без ANSI для проверок. */
    std::string plain() const;
    static constexpr int canvasWidth = 100; ///< Ширина полного терминала.
    static constexpr int canvasHeight = 40; ///< Высота полного терминала.
    static constexpr int fieldX = 2; ///< Начало поля по горизонтали.
    static constexpr int fieldY = 5; ///< Начало поля по вертикали.
    static constexpr int panelX = 77; ///< Начало боковой панели.
    static constexpr int width = 72; ///< Ширина содержимого игрового поля.
    static constexpr int height = 30; ///< Высота содержимого игрового поля.
    static constexpr int panelWidth = 21; ///< Ширина содержимого боковой панели.
    static constexpr int panelHeight = 30; ///< Высота содержимого боковой панели.

    /** @brief Выводит строку UTF-8 с одноколоночными символами внутри поля. */
    void text(int x, int y, std::string_view value,
              Color foreground = Theme::text, Color background = Theme::background);
    /** @brief Выводит целое число внутри поля. */
    void number(int x, int y, std::int64_t value,
                Color foreground = Theme::text, Color background = Theme::background);
    /** @brief Рисует рамку; w и h включают её границы. */
    void frame(int x, int y, int w, int h, Color foreground = Theme::border);
    /** @brief Закрашивает прямоугольник заданным фоном. */
    void fill(int x, int y, int w, int h, Color background);
    /** @brief Рисует glyph и пробел в двух соседних столбцах; x задан в символах. */
    void cell(int x, int y, Color foreground, Color background,
              char32_t glyph = U' ');
    /** @brief Выводит строку внутри боковой панели, начиная с колонки 0. */
    void panelText(int row, std::string_view value, Color foreground = Theme::text);
    /** @brief Выводит текст с локальным x внутри панели; обрезает по её границам. */
    void panelTextAt(int x, int row, std::string_view value, Color foreground = Theme::text);
    /** @brief Выводит подпись и число внутри боковой панели. */
    void panelNumber(int row, std::string_view label, std::int64_t value,
                     Color foreground = Theme::text);
    /** @brief Возвращает отступ поля из columns клеток шириной два символа. */
    static constexpr int boardX(int columns) { return (width - 2 * columns) / 2; }
    /** @brief Возвращает отступ клеточного поля по вертикали. */
    static constexpr int boardY(int rows) { return (height - rows) / 2; }
private:
    /** @brief Символ шириной один столбец и его цвета. */
    struct Pixel {
        char32_t glyph = U' '; ///< Одноколоночный символ.
        Color foreground = Theme::text; ///< Цвет символа.
        Color background = Theme::background; ///< Цвет фона.
        /** @brief Сравнивает отображаемое содержимое двух ячеек. */
        bool operator==(const Pixel& other) const {
            return glyph == other.glyph && foreground == other.foreground && background == other.background;
        }
    };
    std::array<Pixel, canvasWidth * canvasHeight> next_{}; ///< Новый кадр.
    std::array<Pixel, canvasWidth * canvasHeight> shown_{}; ///< Последний отправленный кадр.
    Output output_; ///< Приёмник готовых байтов.
    void* context_; ///< Контекст приёмника.
    bool initialized_ = false; ///< Первый кадр ещё не отправлен.
    /** @brief Записывает символ с проверкой границ канваса. */
    void put(int x, int y, char32_t glyph, Color foreground, Color background);
    /** @brief Рисует UTF-8-строку с локальной областью отсечения. */
    void clippedText(int x, int y, std::string_view value, Color foreground, Color background,
                     int left, int top, int w, int h);
};
