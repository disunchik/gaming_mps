/** @file pong.h @brief Интеграция настоящего пин-понга с общей оболочкой. */
#pragma once
#include "game.h"
#include "model.h"

/** @brief Одиночный пин-понг; меню, пауза и повтор принадлежат оболочке. */
class Pong final : public Game {
public:
    /** @brief Полностью сбрасывает матч и принимает стартовые условия. */
    void start(const Settings& settings) override;
    /** @brief Перемещает цель нижней ракетки клавишами или мышью. */
    void handle(const Input& input) override;
    /** @brief Продвигает фиксированную физику на переданное игровое время. */
    void update(std::uint32_t ms) override;
    /** @brief Рисует поле и полезные показатели без изменения состояния. */
    void draw(Screen& screen) const override;
    /** @brief Возвращает очки человека и фактический исход матча. */
    Result result() const override;
private:
    PongModel model_; ///< Изолированная от терминала игровая модель.
};
