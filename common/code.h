/** @file code.h @brief Версионированные коды стартовых условий. */
#pragma once
#include "game.h"
#include <string>
#include <string_view>

/** @brief Проверяет допустимость всех стартовых условий P2. */
bool validSettings(GameId game, const Settings& settings);
/** @brief Кодирует условия в P2; возвращает пустую строку для неполного/неверного старта. */
std::string encodeCode(GameId game, const Settings& settings);
/** @brief Проверяет и декодирует P2; при ошибке оставляет выходные условия неизменными. */
bool decodeCode(std::string_view code, GameId& game, Settings& settings, std::string& error);
