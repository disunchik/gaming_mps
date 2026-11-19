/** @file input.cpp @brief Разбор потокового терминального ввода. */
#include "input.h"
#include <cstdio>
#include <cctype>
#include <cstring>

bool InputReader::finish(Input& event) {
    event = {};
    if (used_ < 3) return false;
    const char final = sequence_[used_ - 1];
    if (sequence_[1] == '[' && sequence_[2] == '<') {
        unsigned button = 0, x = 0, y = 0;
        char terminator = 0, extra = 0;
        if (std::sscanf(sequence_ + 3, "%u;%u;%u%c%c", &button, &x, &y, &terminator, &extra) != 4
            || terminator != 'M' || x == 0 || y == 0 || x > 32768 || y > 32768
            || button > 127 || (button & 64)) return false;
        event.x = static_cast<std::int16_t>(x - 1);
        event.y = static_cast<std::int16_t>(y - 1);
        if (button & 32) event.type = InputType::PointerMove;
        else if ((button & 3) == 0) event.type = InputType::PointerLeft;
        else if ((button & 3) == 2) event.type = InputType::PointerRight;
        return event.type != InputType::None;
    }
    if (std::strcmp(sequence_, "\x1b[901~") == 0) { event.type = InputType::CopySucceeded; return true; }
    if (std::strcmp(sequence_, "\x1b[902~") == 0) { event.type = InputType::CopyFailed; return true; }
    if (std::strcmp(sequence_, "\x1b[910~") == 0) { event.type = InputType::HeldLeft; return true; }
    if (std::strcmp(sequence_, "\x1b[911~") == 0) { event.type = InputType::HeldRight; return true; }
    if (std::strcmp(sequence_, "\x1b[912~") == 0) { event.type = InputType::HorizontalReleased; return true; }
    if (std::strcmp(sequence_, "\x1b[200~") == 0) { paste_ = true; event.type = InputType::PasteBegin; return true; }
    if (std::strcmp(sequence_, "\x1b[201~") == 0) { paste_ = false; event.type = InputType::PasteEnd; return true; }
    if (std::strcmp(sequence_, "\x1b[903~") == 0) { event.type = InputType::PasteFailed; return true; }
    if (std::strcmp(sequence_, "\x1b[930~") == 0) { event.type = InputType::Redraw; return true; }
    switch (final) {
        case 'A': event.type = InputType::Up; break;
        case 'B': event.type = InputType::Down; break;
        case 'C': event.type = InputType::Right; break;
        case 'D': event.type = InputType::Left; break;
        default: return false;
    }
    return true;
}

bool InputReader::feed(std::uint8_t byte, std::uint32_t now, bool textMode, Input& event) {
    event = {};
    if (used_ && now - escapeTime_ >= 100) { used_ = 0; discard_ = false; }
    if (used_) {
        escapeTime_ = now;
        if (used_ == 1 && byte != '[' && byte != 'O') {
            used_ = 0; // An unrelated printable character starts a fresh event.
        } else {
            const bool final = used_ >= 2 && byte >= 0x40 && byte <= 0x7e;
            if (used_ + 1 < sizeof(sequence_)) sequence_[used_++] = static_cast<char>(byte);
            else discard_ = true;
            if (!final) return false;
            sequence_[used_] = '\0';
            const bool result = !discard_ && finish(event);
            used_ = 0;
            discard_ = false;
            return result;
        }
    }
    if (byte == 27) {
        sequence_[0] = 27;
        used_ = 1;
        escapeTime_ = now;
        return false;
    }
    if (paste_) {
        if (byte >= 32 && byte <= 126) { event.type = InputType::PasteText; event.character = byte; }
        return event.type != InputType::None;
    }
    if (byte == '\n' && carriageReturn_) { carriageReturn_ = false; return false; }
    carriageReturn_ = byte == '\r';
    if (byte == 22) event.type = InputType::Paste;
    else if (byte == '\r' || byte == '\n') event.type = InputType::Confirm;
    else if (byte == 8 || byte == 127) event.type = InputType::Erase;
    else if (textMode && byte >= 32 && byte <= 126) {
        event.type = InputType::Text;
        event.character = byte;
    } else {
        switch (std::tolower(static_cast<unsigned char>(byte))) {
            case 'w': event.type = InputType::Up; break;
            case 's': event.type = InputType::Down; break;
            case 'a': event.type = InputType::Left; break;
            case 'd': event.type = InputType::Right; break;
            case 'c': event.type = InputType::Copy; break;
            case 'p': event.type = InputType::Pause; break;
            case 'r': event.type = InputType::Restart; break;
            case 'q': event.type = InputType::Back; break;
            case 'f': event.type = InputType::Flag; break;
            case ' ': event.type = InputType::Open; break;
            default: break;
        }
    }
    return event.type != InputType::None;
}

bool InputReader::poll(std::uint32_t now, Input& event) {
    if (!used_ || now - escapeTime_ < 50) return false;
    const bool single = used_ == 1;
    used_ = 0;
    discard_ = false;
    event = {};
    if (single) event.type = InputType::Back;
    return single;
}

const char* inputName(InputType type) {
    switch (type) {
        case InputType::Up: return "Вверх";
        case InputType::Down: return "Вниз";
        case InputType::Left: return "Влево";
        case InputType::Right: return "Вправо";
        case InputType::Confirm: return "Enter";
        case InputType::Open: return "Пробел";
        case InputType::Flag: return "Флаг";
        case InputType::PointerMove: return "Наведение";
        case InputType::PointerLeft: return "ЛКМ";
        case InputType::PointerRight: return "ПКМ";
        default: return "Ожидание";
    }
}
