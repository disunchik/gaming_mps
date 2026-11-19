/** @file input.h @brief Готовые события клавиатуры и мыши. */
#pragma once
#include <cstdint>
#include <cstddef>

/** @brief Действия после разбора терминального ввода. */
enum class InputType : std::uint8_t {
    None, Up, Down, Left, Right, Confirm, Open, Flag,
    PointerMove, PointerLeft, PointerRight,
    Pause, Restart, Back, Text, Erase,
    Copy, CopySucceeded, CopyFailed
};

/** @brief Одно событие; команды оболочки перехватываются до Game::handle. */
struct Input {
    InputType type = InputType::None; ///< Готовое действие.
    std::int16_t x = -1; ///< Колонка символа внутри поля; -1 без указателя.
    std::int16_t y = -1; ///< Строка внутри поля; -1 без указателя.
    char32_t character = U'\0'; ///< Символ только для события Text.
};

/** @brief Инкрементальный разбор клавиш, стрелок и SGR-мыши без блокировки. */
class InputReader {
public:
    /** @brief Принимает байт UART; возвращает true при готовом событии. */
    bool feed(std::uint8_t byte, std::uint32_t now, bool textMode, Input& event);
    /** @brief Выдаёт одиночный Esc после ожидания продолжения последовательности. */
    bool poll(std::uint32_t now, Input& event);
private:
    char sequence_[64]{}; ///< Ограниченный буфер терминальной последовательности.
    std::size_t used_ = 0; ///< Количество накопленных байтов.
    std::uint32_t escapeTime_ = 0; ///< Время последнего байта последовательности.
    bool carriageReturn_ = false; ///< Подавляет второй Enter от CRLF.
    bool discard_ = false; ///< Отбрасывает переполненную последовательность до конца.
    /** @brief Преобразует завершённую CSI/SS3-последовательность в событие. */
    bool finish(Input& event);
};

/** @brief Возвращает короткое название действия для подсказок и заглушок. */
const char* inputName(InputType type);
