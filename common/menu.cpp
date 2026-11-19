/** @file menu.cpp @brief Общая навигация без игровых механик. */
#include "menu.h"
#include "code.h"
#include <algorithm>

const char* gameName(GameId game) {
    switch (game) {
        case GameId::Tetris: return "Тетрис";
        case GameId::Minesweeper: return "Сапёр";
        case GameId::Pong: return "Пин-понг";
    }
    return "Игра";
}
const char* difficultyName(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy: return "Лёгкая";
        case Difficulty::Normal: return "Средняя";
        case Difficulty::Hard: return "Сложная";
    }
    return "Средняя";
}
Menu::Menu(Game& tetris, Game& minesweeper, Game& pong, Seed seed)
    : games_{{&tetris, &minesweeper, &pong}}, seed_(seed) {}
Game& Menu::game() const { return *games_[static_cast<unsigned>(game_)]; }
bool Menu::textMode() const { return page_ == Page::Code; }
void Menu::home() { pasteRequest_ = pasting_ = false; pasteWait_ = 0; pasteText_.clear(); page_ = Page::Main; selected_ = mainSelected_; copyStatus_.clear(); copyWait_ = 0; copyRequest_.clear(); }
void Menu::start() {
    copyStatus_.clear(); copyWait_ = 0; copyRequest_.clear();
    game().start(settings_);
    result_ = game().result();
    page_ = Page::Playing;
    skipUpdate_ = true;
}
void Menu::startFreshMinefield() {
    const auto seed = seed_();
    settings_.seed = seed == settings_.seed ? seed + 1u : seed;
    settings_.firstX = settings_.firstY = -1;
    start();
}
bool Menu::hasNext() const {
    return result_.state == State::Won && ((game_ == GameId::Minesweeper && settings_.level < 5)
        || (game_ == GameId::Pong && settings_.level < 4));
}
bool Menu::onGamePage() const {
    return page_ == Page::Playing || page_ == Page::Paused || page_ == Page::Results ||
           page_ == Page::BoardReview;
}
const char* Menu::resultTitle() const {
    if (game_ == GameId::Pong) return result_.state == State::Won ? "Соперник переигран" : "Проиграл дуэль";
    if (result_.state == State::Lost)
        return game_ == GameId::Minesweeper ? "Не заметил растяжку" : "Партия окончена";
    return game_ == GameId::Minesweeper ? "Поле зачищено" : "Уровень пройден";
}
std::string Menu::currentCode() const {
    Settings recorded = settings_;
    if (game_ == GameId::Minesweeper && result_.firstX >= 0 && result_.firstY >= 0) {
        recorded.firstX = result_.firstX;
        recorded.firstY = result_.firstY;
    }
    return encodeCode(game_, recorded);
}
int Menu::rows() const {
    switch (page_) {
        case Page::Main: return 5;
        case Page::Settings: return game_ == GameId::Tetris ? 3 : 4;
        case Page::Paused: return 4;
        case Page::Results: return 2 + hasNext() + (game_ == GameId::Minesweeper ? 2 : 0);
        case Page::BoardReview: return 2;
        case Page::CodeConfirm: return 2;
        default: return 0;
    }
}
int Menu::hit(int x, int y) const {
    if (page_ == Page::BoardReview) {
        if (x <= Screen::panelX || x >= Screen::panelX + Screen::panelWidth - 1) return -1;
        const int offset = y - reviewFirstY_;
        return (offset == 0 || offset == 2) ? offset / 2 : -1;
    }
    x -= 10; y -= 5;
    if (x < 9 || x >= 71) return -1;
    switch (page_) {
        case Page::Main:
            if (y >= 10 && y <= 18 && y % 2 == 0) return (y - 10) / 2;
            break;
        case Page::Settings:
            if (y == settingsDifficultyY_ - 5) return 0;
            if (game_ != GameId::Tetris && y == settingsLevelY_ - 5) return 1;
            if (y == settingsStartY_ - 5) return rows() - 2;
            if (y == settingsBackY_ - 5) return rows() - 1;
            break;
        case Page::Paused:
            if (y >= 13 && y <= 19 && y % 2) return (y - 13) / 2;
            break;
        case Page::Results:
            if (y >= 16 && y <= 16 + 2 * (rows() - 1) && y % 2 == 0) return (y - 16) / 2;
            break;
        case Page::CodeConfirm:
            if (y == 16 || y == 18) return (y - 16) / 2;
            break;
        default: break;
    }
    return -1;
}
void Menu::activate() {
    if (page_ == Page::Main) {
        mainSelected_ = selected_;
        if (selected_ < 3) {
            game_ = static_cast<GameId>(selected_);
            page_ = Page::Settings;
            selected_ = 0;
        } else if (selected_ == 3) {
            page_ = Page::Code;
            codeText_.clear();
            error_.clear();
        } else page_ = Page::Help;
    } else if (page_ == Page::Settings) {
        if (selected_ == rows() - 1) home();
        else if (selected_ == rows() - 2) {
            settings_ = {};
            settings_.difficulty = difficulties_[static_cast<unsigned>(game_)];
            settings_.level = game_ == GameId::Minesweeper ? mineLevel_ : game_ == GameId::Pong ? pongLevel_ : 1;
            settings_.seed = seed_();
            start();
        } else {
            Input right; right.type = InputType::Right; handle(right);
        }
    } else if (page_ == Page::Paused) {
        if (selected_ == 0) { page_ = Page::Playing; skipUpdate_ = true; }
        else if (selected_ == 1) start();
        else if (selected_ == 2) copy();
        else home();
    } else if (page_ == Page::Results) {
        if (hasNext() && selected_ == 0) {
            ++settings_.level;
            settings_.seed += 0x9e3779b9u;
            settings_.firstX = settings_.firstY = -1;
            start();
        } else if (selected_ == rows() - 1) home();
        else if (game_ == GameId::Minesweeper && selected_ == rows() - 2) startFreshMinefield();
        else if (game_ == GameId::Minesweeper && selected_ == rows() - 3) {
            page_ = Page::BoardReview;
            selected_ = 0;
        }
        else start();
    } else if (page_ == Page::BoardReview) {
        if (selected_ == 0) startFreshMinefield();
        else { page_ = Page::Results; selected_ = rows() - 3; }
    } else if (page_ == Page::CodeConfirm) {
        if (!selected_) start();
        else { page_ = Page::Code; error_.clear(); }
    }
}
void Menu::handle(const Input& input) {
    if (input.type == InputType::HeldLeft || input.type == InputType::HeldRight ||
        input.type == InputType::HorizontalReleased) {
        // Release must reach the game even while paused, without advancing time.
        if (page_ == Page::Playing || page_ == Page::Paused) game().handle(input);
        return;
    }
    if (input.type == InputType::CopySucceeded || input.type == InputType::CopyFailed) {
        if (copyWait_) {
            copyStatus_ = input.type == InputType::CopySucceeded ? "Скопировано" : "Не удалось скопировать";
            copyWait_ = 0;
        }
        return;
    }
    const bool gamePage = onGamePage();
    if (input.type == InputType::Copy || (gamePage && input.type == InputType::PointerLeft &&
        input.y == Screen::canvasHeight - 2 && input.x >= 31 && input.x < 46)) {
        if (gamePage) copy();
        return;
    }

    if (page_ == Page::Code) {
        if (input.type == InputType::Up || input.type == InputType::Paste) {
            if (!pasteWait_) { pasteRequest_ = true; pasteWait_ = 2000; error_ = "Читаю буфер…"; }
        } else if (input.type == InputType::PasteBegin && pasteWait_) {
            pasting_ = true; pasteText_.clear();
        } else if (input.type == InputType::PasteText) {
            if (pasting_ && pasteText_.size() < 64) pasteText_ += static_cast<char>(input.character);
        } else if (input.type == InputType::PasteEnd && pasting_) {
            codeText_ = pasteText_; pasteText_.clear(); pasting_ = false; pasteWait_ = 0; error_.clear();
        } else if (input.type == InputType::PasteFailed && pasteWait_) {
            pasting_ = false; pasteWait_ = 0; error_ = "В буфере нет кода P2";
        } else if (input.type == InputType::Back) home();
        else if (input.type == InputType::Text) {
            if (codeText_.size() < 64 && input.character < 128) {
                codeText_ += static_cast<char>(input.character); error_.clear();
            } else error_ = "Код слишком длинный";
        } else if (input.type == InputType::Erase && !codeText_.empty()) {
            codeText_.pop_back(); error_.clear();
        } else if (input.type == InputType::Confirm && decodeCode(codeText_, game_, settings_, error_)) {
            page_ = Page::CodeConfirm; selected_ = 0;
        }
        return;
    }
    if (page_ == Page::Help) {
        if (input.type == InputType::Back || input.type == InputType::Confirm || input.type == InputType::PointerLeft) home();
        return;
    }
    if (input.type == InputType::Back) {
        if (page_ == Page::CodeConfirm) { page_ = Page::Code; error_.clear(); }
        else if (page_ == Page::BoardReview) { page_ = Page::Results; selected_ = rows() - 3; }
        else home();
        return;
    }
    if (gamePage) {
        if (input.type == InputType::Restart) {
            if (game_ == GameId::Minesweeper && result_.firstX >= 0 && result_.firstY >= 0) {
                settings_.firstX = result_.firstX; settings_.firstY = result_.firstY;
            }
            start(); return;
        }
        if (input.type == InputType::Pause && (page_ == Page::Playing || page_ == Page::Paused)) {
            if (page_ == Page::Playing) { page_ = Page::Paused; selected_ = 0; }
            else { page_ = Page::Playing; skipUpdate_ = true; }
            return;
        }
    }
    if (page_ == Page::Playing) {
        Input local = input;
        if (input.type == InputType::PointerMove || input.type == InputType::PointerLeft || input.type == InputType::PointerRight) {
            local.x -= 2; local.y -= 5;
            if (local.x < 0 || local.x >= Screen::width || local.y < 0 || local.y >= Screen::height) {
                if (input.type != InputType::PointerMove) return;
                local.x = local.y = -1; // Explicit leave-field event clears hover.
            }
        }
        game().handle(local);
        result_ = game().result();
        return;
    }
    const int count = rows();
    if (input.type == InputType::PointerLeft || input.type == InputType::PointerMove) {
        const int row = hit(input.x, input.y);
        if (row >= 0) {
            selected_ = row;
            if (input.type == InputType::PointerLeft) {
                if (page_ == Page::Settings && row < rows() - 2) {
                    Input direction;
                    direction.type = input.x < 53 ? InputType::Left : InputType::Right;
                    handle(direction);
                } else activate();
            }
        }
        return;
    }
    if (input.type == InputType::Up) selected_ = std::max(0, selected_ - 1);
    else if (input.type == InputType::Down) selected_ = std::max(0, std::min(count - 1, selected_ + 1));
    else if (input.type == InputType::Confirm) activate();
    else if (page_ == Page::Settings && (input.type == InputType::Left || input.type == InputType::Right)) {
        const int delta = input.type == InputType::Right ? 1 : -1;
        if (selected_ == 0) {
            auto& difficulty = difficulties_[static_cast<unsigned>(game_)];
            difficulty = static_cast<Difficulty>(std::max(0, std::min(2, static_cast<int>(difficulty) + delta)));
        } else if (selected_ == 1 && game_ != GameId::Tetris) {
            auto& level = game_ == GameId::Minesweeper ? mineLevel_ : pongLevel_;
            level = static_cast<std::uint32_t>(std::clamp(static_cast<int>(level) + delta, 1, game_ == GameId::Minesweeper ? 5 : 3));
        }
    }
}
void Menu::update(std::uint32_t ms) {
    if (pasteWait_) {
        if (ms >= pasteWait_) { pasteWait_ = 0; pasting_ = false; error_ = "Буфер недоступен. Введи код вручную."; }
        else pasteWait_ -= ms;
    }
    if (page_ == Page::Main || page_ == Page::Settings || page_ == Page::Code ||
        page_ == Page::CodeConfirm || page_ == Page::Help)
        sceneryMs_ = (sceneryMs_ + ms % sceneryPeriodMs_) % sceneryPeriodMs_;
    if (copyWait_) {
        if (ms >= copyWait_) { copyWait_ = 0; copyStatus_ = "Буфер недоступен"; }
        else copyWait_ -= ms;
    }
    if (page_ != Page::Playing) return;
    if (skipUpdate_) { skipUpdate_ = false; return; }
    game().update(ms);
    result_ = game().result();
    if (result_.state != State::Playing) {
        if (game_ == GameId::Minesweeper && result_.firstX >= 0 && result_.firstY >= 0) {
            settings_.firstX = result_.firstX; settings_.firstY = result_.firstY;
        }
        page_ = Page::Results; selected_ = 0;
    }
}
void Menu::option(Screen& s, int row, int y, std::string_view label) const {
    const bool selected = selected_ == row;
    const bool paper = page_ != Page::Paused && page_ != Page::Results;
    const Color background = selected ? Theme::wood : paper ? Theme::paper : Theme::background;
    const Color foreground = selected ? Theme::white : paper ? Theme::ink : Theme::text;
    s.canvasFill(19, y + 5, 62, 1, background);
    s.canvasText(20, y + 5, selected ? "*" : " ", foreground, background);
    s.canvasText(23, y + 5, label, foreground, background);
}
void Menu::reviewButtons(Screen& s) const {
    constexpr const char* labels[] = {"Начать заново", "К результату"};
    for (int row = 0; row < 2; ++row) {
        const int y = reviewFirstY_ + 2 * row;
        const Color background = selected_ == row ? Theme::wood : Theme::background;
        const Color foreground = selected_ == row ? Theme::white : Theme::text;
        s.canvasFill(Screen::panelX + 1, y, Screen::panelWidth - 2, 1, background);
        s.canvasText(Screen::panelX + 2, y, labels[row], foreground, background);
    }
}
void Menu::draw(Screen& s) const {
    s.begin();
    const bool noticePage = !onGamePage() && page_ != Page::Help;
    s.canvasText(noticePage ? 19 : 1, noticePage ? 6 : 1,
                 "Промпт, паяльник, три ствола", Theme::accent);
    if (onGamePage()) {
        s.canvasText(1, 2, std::string(gameName(game_)) + " / " + difficultyName(settings_.difficulty)
                     + " / Уровень " + std::to_string(result_.level), Theme::muted);
        s.canvasFrame(1, 4, 74, Screen::height + 2);
        s.canvasFrame(76, 4, 23, Screen::panelHeight + 2);
        game().draw(s);
        s.canvasText(1, Screen::canvasHeight - 3, page_ == Page::BoardReview
            ? "↑/↓: выбор   Enter/ЛКМ: подтвердить   Esc/Q: к результату"
            : "Стрелки/WASD: движение   P: пауза   R: повтор   Q: в меню", Theme::muted);
        const auto code = currentCode();
        s.canvasText(1, Screen::canvasHeight - 2, code.empty() ? "Код после первого открытия" : "Код: " + code, Theme::accent);
        if (!code.empty()) s.canvasText(31, Screen::canvasHeight - 2, "Копировать [C]", Theme::cyan);
        s.canvasText(50, Screen::canvasHeight - 2, copyStatus_, Theme::muted);
        if (page_ == Page::Paused) {
            s.canvasFill(1, 4, 98, Screen::height + 2, Theme::background);
            s.canvasFill(18, 13, 64, 13, Theme::background);
            s.canvasFrame(18, 13, 64, 13, Theme::accent);
            s.canvasText(23, 15, "Пауза", Theme::accent);
            option(s, 0, 13, "Продолжить");
            option(s, 1, 15, "Повторить те же условия");
            option(s, 2, 17, "Копировать код");
            option(s, 3, 19, "В меню");
        } else if (page_ == Page::Results) {
            const bool mines = game_ == GameId::Minesweeper;
            const int height = mines ? 12 + 2 * rows() : 16;
            s.canvasFill(1, 4, 98, Screen::height + 2, Theme::background);
            s.canvasFill(18, 12, 64, height, Theme::background);
            s.canvasFrame(18, 12, 64, height, Theme::accent);
            s.canvasText(23, 14, resultTitle(), Theme::accent);
            s.canvasText(23, 16, std::string(mines ? "Цена ошибки: " : "Счёт: ") + std::to_string(result_.score));
            s.canvasText(23, 17, "Игровое время: " + std::to_string(result_.timeMs / 1000) + " с");
            int row = 0;
            if (hasNext()) { option(s, row, 16 + 2 * row, game_ == GameId::Pong && settings_.level == 3 ? "Экстремальный уровень (2 ковбоя)" : "Следующий уровень"); ++row; }
            option(s, row, 16 + 2 * row, "Повторить те же условия"); ++row;
            if (mines) {
                option(s, row, 16 + 2 * row, "Посмотреть все поле"); ++row;
                option(s, row, 16 + 2 * row, "Начать заново"); ++row;
            }
            option(s, row, 16 + 2 * row, "В меню");
        } else if (page_ == Page::BoardReview) {
            s.panelText(22, resultTitle(), Theme::accent);
            reviewButtons(s);
        }
        return;
    }
    scenery(s);
    const char* cactus[] = {"  |  ", "| |  ", "|_| |", "  |_|", "  |  ", "__|__"};
    for (int i = 0; i < 6; ++i) {
        s.canvasText(11, 23 + i, cactus[i], Theme::muted);
        s.canvasText(84, 23 + i, cactus[i], Theme::muted);
    }
    if (page_ == Page::Help) {
        s.canvasText(1, 2, "Три игры. Один салун.", Theme::muted);
        s.canvasFill(18, 5, 64, 30, Theme::paper);
        s.canvasFrame(18, 5, 64, 30, Theme::wood);
        s.canvasFill(19, 6, 62, 3, Theme::wood);
        s.canvasText(38, 7, "КРАТКАЯ СВОДКА САЛУНА", Theme::white, Theme::wood);
        const auto ink = [&s](int x, int y, std::string_view value, Color color = Theme::ink) {
            s.canvasText(x, y, value, color, Theme::paper);
        };
        ink(21, 10, "ТЕТРИС / ЗАКАЗЫ ШАХТЫ", Theme::wood);
        ink(23, 11, "A/D или ←/→ — сдвиг. W/↑ — поворот. S/↓ — быстрее.");
        ink(23, 12, "Ряды дают заработок, заказы — дополнительные выплаты.");
        ink(23, 13, "Жила, двойные очистки и серии — следи за афишей.");
        ink(23, 14, "Каждые 10 линий — уровень. На сложной нет предпросмотра.");
        ink(21, 16, "САПЁР / ДИНАМИТНАЯ ДОЛИНА", Theme::wood);
        ink(23, 17, "ЛКМ / Enter — открыть. ПКМ / F — бандана.");
        ink(23, 18, "Повторно нажми на открытую цифру: можно даблкликом.");
        ink(23, 19, "Бандан вокруг столько же? Остальные соседи раскроются.");
        ink(23, 20, "Ошибочная бандана может привести к взрыву.");
        ink(23, 21, "Первый ход безопасен. Открой все клетки без динамита.");
        ink(21, 23, "ПИН-ПОНГ / РИКОШЕТ САЛУНА", Theme::wood);
        ink(23, 24, "A/D или ←/→, мышь — ракетка. Дуэль до 5 очков.");
        ink(23, 25, "Уровень — скорость пробки, сложность — точность врага.");
        ink(23, 26, "После 10 секунд без очка пробка начинает разгоняться.");
        ink(23, 27, "Победи на уровне 3 — откроется дуэль с 2 ковбоями.");
        s.canvasFill(20, 29, 60, 3, Theme::wood);
        s.canvasText(23, 29, "P — пауза     R — повторить те же условия", Theme::white, Theme::wood);
        s.canvasText(23, 31, "Q — в салун   C — скопировать код испытания", Theme::white, Theme::wood);
        ink(23, 33, "Enter / Esc / ЛКМ — назад", Theme::paperMuted);
        return;
    }
    // A paper notice inside a wooden saloon sign; all controls keep their coordinates.
    const int noticeHeight = page_ == Page::Main ? 21 : 20;
    s.canvasFill(18, 10, 64, noticeHeight, Theme::paper);
    s.canvasFrame(18, 10, 64, noticeHeight, Theme::wood);
    s.canvasFill(19, 11, 62, 3, Theme::wood);
    s.canvasText(38, 11, "*  САЛУН «ТРИ СТВОЛА»  *", Theme::white, Theme::wood);
    s.canvasText(19, 7, "Три игры. Один салун.", Theme::muted);
    if (page_ != Page::Code)
        s.canvasText(19, 33, "Стрелки/WASD: выбор   Enter/ЛКМ: выбрать   Esc/Q: назад", Theme::muted);
    const auto text = [&s](int y, std::string_view label, Color color = Theme::ink) {
        s.canvasText(23, y + 5, label, color, Theme::paper);
    };
    const auto title = [&s](std::string_view label) {
        s.canvasText(23, 13, label, Theme::white, Theme::wood);
    };
    if (page_ == Page::Main) {
        title("Выбери игру");
        const char* items[] = {"Тетрис", "Сапёр", "Пин-понг", "Играть по коду", "Краткая сводка"};
        const char* notes[] = {"01 / КОВБОЙ ПАША", "02 / КОВБОЙ ТЁМА", "03 / КОВБОЙ ДИМА", "КОД ИСПЫТАНИЯ", "ПРАВИЛА И ФИШКИ"};
        for (int i = 0; i < 5; ++i) {
            const int y = 10 + 2 * i;
            option(s, i, y, items[i]);
            s.canvasText(60, y + 5, notes[i], selected_ == i ? Theme::white : Theme::paperMuted,
                         selected_ == i ? Theme::wood : Theme::paper);
        }
        const char* mug[] = {"  .~~~~.   ", " |~~~~~~|_ ", " | | |  | )", " | | |  |/ ", " |______|  ", "  '----'   "};
        for (int row = 0; row < 6; ++row)
            s.canvasText(65, 24 + row, mug[row], row == 0 ? Theme::ink : Theme::wood, Theme::paper);
    } else if (page_ == Page::Settings) {
        title(gameName(game_));
        setting(s, 0, settingsDifficultyY_ - 5, "Сложность:", difficultyName(difficulties_[static_cast<unsigned>(game_)]));
        const char* hint = game_ == GameId::Tetris ? "Меняет начальный темп падения"
            : game_ == GameId::Pong ? "Меняет только точность врага" : "Меняет долю динамита: 10 / 15 / 20%";
        text(settingsDifficultyY_ - 4, hint, Theme::paperMuted);
        if (game_ == GameId::Minesweeper) {
            const std::string size = std::to_string(6 + 2 * mineLevel_);
            setting(s, 1, settingsLevelY_ - 5, "Размер:", size + "x" + size);
            text(settingsLevelY_ - 4, "После победы предложим следующий размер", Theme::paperMuted);
        }
        if (game_ == GameId::Pong) {
            setting(s, 1, settingsLevelY_ - 5, "Уровень:", std::to_string(pongLevel_));
            text(settingsLevelY_ - 4, "1–3: начальная скорость пробки", Theme::paperMuted);
        }
        option(s, rows() - 2, settingsStartY_ - 5, "Начать игру");
        option(s, rows() - 1, settingsBackY_ - 5, "Назад");
        const char* summary = game_ == GameId::Pong ? "Матч до 5 очков. После уровня 3 — бонус."
            : game_ == GameId::Tetris ? "Убирай ряды. Каждые 10 линий — новый уровень."
            : "Выбери размер поля и сложность.";
        text(23, summary, Theme::paperMuted);
    } else if (page_ == Page::Code) {
        title("Играть по коду");
        text(10, "Введи код друга — сыграй в тех же условиях");
        text(12, "> " + codeText_.substr(codeText_.size() > 54 ? codeText_.size() - 54 : 0));
        text(15, error_, Theme::paperError);
        text(19, "Формат: P2-XXXX-XXXX-XXXXX", Theme::paperMuted);
        s.canvasText(19, 33, "↑ / Ctrl+V: вставить код   Enter: проверить   Esc: назад", Theme::muted);
    } else if (page_ == Page::CodeConfirm) {
        title("Код проверен");
        text(10, std::string(gameName(game_)) + " / " + difficultyName(settings_.difficulty));
        text(12, "Начальный уровень: " + std::to_string(settings_.level));
        option(s, 0, 16, "Начать игру");
        option(s, 1, 18, "Назад к коду");
    }
}

void Menu::setting(Screen& s, int row, int y, std::string_view label, std::string_view value) const {
    option(s, row, y, label);
    const Color bg = selected_ == row ? Theme::wood : Theme::paper;
    const Color fg = selected_ == row ? Theme::white : Theme::ink;
    s.canvasText(42, y + 5, "<", fg, bg);
    s.canvasText(48, y + 5, value, fg, bg);
    s.canvasText(64, y + 5, ">", fg, bg);
}

void Menu::scenery(Screen& s) const {
    const unsigned step = sceneryMs_ / sceneryStepMs_;
    const int x = static_cast<int>((step + 6) % 106) - 3;
    static constexpr const char* frames[4][2] = {
        {"\\|/", "/|\\"}, {"-*-", "\\|/"}, {"/|\\", "\\|/"}, {"\\|/", "-*-"}
    };
    s.canvasText(x, 27, frames[step % 4][0], Theme::muted);
    s.canvasText(x, 28, frames[step % 4][1], Theme::muted);
}

void Menu::copy() {
    if (copyWait_) return;
    const auto code = currentCode();
    if (code.empty()) { copyStatus_ = "Код пока недоступен"; return; }
    copyRequest_ = code; copyWait_ = 2000; copyStatus_ = "Копирование…";
}
std::string Menu::takeCopyRequest() {
    std::string request;
    request.swap(copyRequest_);
    return request;
}

bool Menu::takePasteRequest() {
    const bool pending = pasteRequest_;
    pasteRequest_ = false;
    return pending;
}
