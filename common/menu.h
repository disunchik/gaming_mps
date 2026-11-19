/** @file menu.h @brief Состояния общей оболочки и переходы между экранами. */
#pragma once
#include "game.h"
#include "input.h"
#include "screen.h"
#include <array>
#include <string>

/** @brief Меню, настройки, игра, пауза, результаты и ввод кода. */
class Menu {
public:
    /** @brief Источник seed только для новой партии из меню. */
    using Seed = std::uint32_t (*)();
    /** @brief Подключает три игры и источник новых стартовых условий. */
    Menu(Game& tetris, Game& minesweeper, Game& pong, Seed seed);
    /** @brief Принимает событие в абсолютных координатах канваса. */
    void handle(const Input& input);
    /** @brief Продвигает только активную неприостановленную игру. */
    void update(std::uint32_t ms);
    /** @brief Формирует полный кадр; метод не меняет состояние приложения. */
    void draw(Screen& screen) const;
    /** @brief Указывает парсеру, что печатные клавиши сейчас означают текст. */
    bool textMode() const;
    /** @brief Забирает одноразовый запрос копирования для UART, вне draw. */
    std::string takeCopyRequest();
private:
    /** @brief Экраны оболочки. */
    enum class Page { Main, Settings, Playing, Paused, Results, Code, CodeConfirm, Help };
    std::array<Game*, 3> games_; ///< Подключённые игры в порядке GameId.
    Seed seed_; ///< Источник новых seed.
    Page page_ = Page::Main; ///< Текущий экран.
    int selected_ = 0; ///< Выбранная строка текущего экрана.
    int mainSelected_ = 0; ///< Последняя строка главного меню.
    GameId game_ = GameId::Tetris; ///< Выбранная игра.
    Settings settings_{}; ///< Стартовые условия текущего уровня.
    Result result_{}; ///< Последний результат игры.
    std::array<Difficulty, 3> difficulties_{{Difficulty::Normal, Difficulty::Normal, Difficulty::Normal}}; ///< Запомненные сложности.
    std::uint32_t pongLevel_ = 1; ///< Выбранный обычный уровень Понга (1..3).
    std::uint32_t mineLevel_ = 1; ///< Выбранный стартовый размер сапёра.
    std::string codeText_; ///< Вводимый код.
    std::string error_; ///< Ошибка ввода кода.
    std::string copyRequest_; ///< Ожидающий отправки код для буфера.
    std::string copyStatus_; ///< Подтверждение или ошибка копирования.
    unsigned copyWait_ = 0; ///< Тайм-аут подтверждения Windows в миллисекундах.
    /** @brief Запрашивает копирование доступного кода, не затрагивая игру. */
    void copy();
    bool skipUpdate_ = true; ///< Исключает время перехода/паузы из следующего update.
    /** @brief Возвращает подключённую текущую игру. */
    Game& game() const;
    /** @brief Возвращает оболочку к последнему выбранному пункту главного меню. */
    void home();
    /** @brief Запускает или повторяет полностью сохранённые стартовые условия. */
    void start();
    /** @brief Действие выбранного пункта текущего экрана. */
    void activate();
    /** @brief Определяет строку кнопки под указателем. */
    int hit(int x, int y) const;
    /** @brief Число выбираемых строк текущего экрана. */
    int rows() const;
    /** @brief Проверяет наличие следующего конечного уровня. */
    bool hasNext() const;
    /** @brief Формирует код именно стартовых условий, дополняя первый ход сапёра. */
    std::string currentCode() const;
    /** @brief Рисует выбранную или обычную строку меню. */
    void option(Screen& screen, int row, int y, std::string_view text) const;
};

/** @brief Читаемое имя игры. */
const char* gameName(GameId game);
/** @brief Читаемое имя сложности. */
const char* difficultyName(Difficulty difficulty);
