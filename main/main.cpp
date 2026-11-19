/** @file main.cpp @brief UART и основной цикл приложения ESP32. */
#include "menu.h"
#include "tetris.h"
#include "minesweeper.h"
#include "pong.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/** @brief Отправляет общий экран в UART0. */
static void writeUart(const char* data, std::size_t size, void*) {
    uart_write_bytes(UART_NUM_0, data, size);
}
/** @brief Получает seed новой партии из аппаратного источника SDK. */
static std::uint32_t newSeed() { return esp_random(); }

/** @brief Запускает оболочку; игровые модули не создают собственных циклов. */
extern "C" void app_main() {
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 4096, 0, 0, nullptr, 0));
    esp_log_level_set("*", ESP_LOG_NONE);
    static Screen screen(writeUart);
    static Tetris tetris;
    static Minesweeper minesweeper;
    static Pong pong;
    static Menu menu(tetris, minesweeper, pong, newSeed);
    InputReader reader;
    std::uint64_t previous = esp_timer_get_time() / 1000;
    std::uint64_t rendered = 0;
    for (;;) {
        const std::uint64_t now = esp_timer_get_time() / 1000;
        menu.update(static_cast<std::uint32_t>(now - previous));
        previous = now;
        std::uint8_t byte;
        Input event;
        for (int count = 0; count < 512 && uart_read_bytes(UART_NUM_0, &byte, 1, 0) == 1; ++count) {
            if (reader.feed(byte, static_cast<std::uint32_t>(now), menu.textMode(), event))
                menu.handle(event);
        }
        if (reader.poll(static_cast<std::uint32_t>(now), event)) menu.handle(event);
        const auto copy = menu.takeCopyRequest();
        if (!copy.empty()) screen.copyText(copy);
        if (now - rendered >= 50) {
            menu.draw(screen);
            screen.flush();
            rendered = now;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
