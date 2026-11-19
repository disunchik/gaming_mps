"""Run the real Tetris firmware through the shared QEMU UART launcher."""
from pathlib import Path
import re
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT), str(ROOT / "tools")]
from run import Qemu
from terminal import mouse_bytes
from tests.terminal_model import TerminalModel

SCREENS = ROOT / "artifacts" / "screens"
CODE = re.compile(r"P2-(?:[0-9A-Z]{4}-){2}[0-9A-Z]{5}")
# Tetris, Normal, level 1, seed 0.
DEMO_P2 = "P2-0G00-0000-02RGG"

# Screen::fieldX, fieldY, width and height from the shared screen contract.
FIELD_X, FIELD_Y, FIELD_WIDTH, FIELD_HEIGHT = 2, 5, 72, 34
BOARD_COLUMNS, BOARD_ROWS, CELL_WIDTH = 10, 20, 2

# Position/rotate, descend that many steps, expect this total number of cleared lines.
# The first four figures earn the first bounty. The next three clear the marked row.
DEMO_MOVES = (
    (b"aaa", 19, 0), (b"waa", 18, 0), (b"dddd", 19, 0), (b"d", 19, 1),
    (b"dd", 19, 1), (b"a", 18, 1), (b"waaaa", 18, 2), (b"waa", 17, 2),
    (b"dddd", 19, 2), (b"waaaa", 17, 2), (b"dd", 18, 3), (b"d", 19, 4),
    (b"wwwddddd", 18, 4), (b"wdd", 18, 4), (b"ww", 18, 5), (b"wdddd", 18, 5),
    (b"wwaaa", 18, 6), (b"d", 19, 6), (b"wdd", 17, 6), (b"wa", 18, 7),
    (b"d", 18, 7), (b"dddd", 18, 7), (b"aaa", 18, 8), (b"aa", 19, 8),
    (b"waaaa", 18, 9), (b"wwa", 18, 10),
)


class Session:
    """Send terminal events and observe complete ESP32 frames."""

    def __init__(self, uart):
        self.uart = uart
        self.screen = TerminalModel()

    def receive(self):
        try:
            data = self.uart.recv(16384)
        except socket.timeout:
            return
        assert data, "QEMU UART unexpectedly closed"
        self.screen.feed(data)

    def wait(self, condition, description, timeout=10):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.receive()
            if self.screen.complete and condition():
                return
        self.screen.snapshot(SCREENS, "tetris-failure")
        raise AssertionError(f"Expected {description}\n{self.screen.text()}")

    def text(self, expected):
        self.wait(lambda: expected in self.screen.text(), expected)

    def key(self, key):
        self.uart.sendall(key)

    def click_option(self, label):
        for y, line in enumerate(self.screen.text().splitlines()):
            x = line.find(label)
            if x >= 0:
                self.key(mouse_bytes(0, x, y) + mouse_bytes(0, x, y, True))
                return
        raise AssertionError(f"Menu option {label!r} is missing")

    def code(self):
        return CODE.search(self.screen.text()).group()

    def falling(self):
        return {(x, y) for x, y, glyph in self.field() if glyph == "◆"}

    def measure_drop_interval(self, expected_ms):
        """Time consecutive automatic steps of the same piece using UART frames."""
        points = self.falling()
        assert len(points) == 4
        observed = []
        for _ in range(7):
            points = {(x, y + 1) for x, y in points}
            self.wait(lambda: self.falling() == points, "one automatic drop without keyboard input")
            observed.append(time.monotonic())
        interval_ms = round(1000 * (observed[-1] - observed[0]) / (len(observed) - 1))
        # Allow for the 50 ms render cadence and the time to deliver a UART frame.
        assert abs(interval_ms - expected_ms) <= 100, (expected_ms, interval_ms)
        return interval_ms

    def check_preview(self, visible):
        frame = self.screen.text()
        assert ("СЛЕДУЮЩАЯ" in frame) == visible
        assert frame.count("◆") - len(self.falling()) == (4 if visible else 0)

    def field(self):
        left = FIELD_X + (FIELD_WIDTH - CELL_WIDTH * BOARD_COLUMNS) // 2
        top = FIELD_Y + (FIELD_HEIGHT - BOARD_ROWS) // 2
        return ((x, y, self.screen.cells[y][x][0])
                for y in range(top, top + BOARD_ROWS)
                for x in range(left, left + CELL_WIDTH * BOARD_COLUMNS, CELL_WIDTH))

    def start(self, adjustment, difficulty):
        self.click_option("Тетрис")
        self.text("Меняет начальный темп")
        if adjustment:
            self.key(adjustment)
        self.wait(lambda: any("Сложность:" in row and difficulty in row
                              for row in self.screen.text().splitlines()),
                  f"difficulty {difficulty}")
        self.click_option("Начать игру")
        self.wait(lambda: len(self.falling()) == 4 and "Заработок: 0" in self.screen.text(),
                  "a fresh Tetris board")
        self.text("До уровня: 10")
        self.text("ЗАКАЗЫ ШАХТЫ")
        self.text("Оплата")


def play_demo(session, input_code, emitted_code):
    """Play through both rewards and measure gravity after the first level transition."""
    session.click_option("Играть по коду")
    session.text("Введи код друга")
    session.key(input_code.encode("ascii") + b"\r")
    session.text("Код проверен")
    session.key(b"\r")
    session.text("Убери 1 ряд")
    assert session.code() == emitted_code
    assert any(glyph == "◇" for _, _, glyph in session.field())
    session.check_preview(visible=True)
    for placed, (position, steps, lines) in enumerate(DEMO_MOVES, 1):
        session.key(position + b"s" * steps)
        expected_cells = 4 * placed - 10 * lines
        session.wait(lambda: sum(glyph in "▪*" for _, _, glyph in session.field()) == expected_cells,
                     f"figure {placed} to lock with {lines} cleared lines")
        if placed == 4:
            session.text("Заработок: 175")
            session.text("ЗОЛОТАЯ ЖИЛА")
            session.text("Осталось фигур: 6")
            session.text("Работа оплачена")
            assert sum(glyph == "*" for _, _, glyph in session.field()) == 1
            session.screen.snapshot(SCREENS, "tetris-gold")
        elif placed == 7:
            session.text("Жила взята!")
            session.text("Заработок: 525")
            session.text("Оплат: 2")
    session.text("Уровень: 2")
    session.text("Линии: 10")
    session.screen.snapshot(SCREENS, "tetris-level-2")
    level_two_ms = session.measure_drop_interval(863)
    session.key(b"r")
    session.text("Убери 1 ряд")
    session.text("Уровень: 1")
    session.text("Оплат: 0")
    assert session.code() == emitted_code
    session.key(b"q")
    session.text("Выбери игру")
    return level_two_ms


def main():
    """Exercise all difficulties, movement, pause, replay and game over."""
    with Qemu(port=5557) as qemu:
        session = Session(qemu.serial)
        session.text("Выбери игру")
        session.start(b"a", "Лёгкая")
        session.check_preview(visible=True)
        original_code = session.code()
        assert original_code.startswith("P2-")
        initial = session.falling()
        session.key(b"\x1b[D")
        left = min(x for x, _ in initial)
        session.wait(lambda: min((x for x, _ in session.falling()), default=-1) == left - CELL_WIDTH,
                     "a left move from an ANSI arrow")
        session.key(b"r")
        session.wait(lambda: session.falling() == initial, "the same opening piece after replay")
        assert session.code() == original_code
        session.key(b"p")
        session.text("Продолжить")
        deadline = time.monotonic() + 1.4
        while time.monotonic() < deadline:
            session.receive()
        session.key(b"p")
        session.wait(lambda: session.falling() == initial, "the same board after a long pause")
        session.key(b"s")
        session.wait(lambda: session.falling() == {(x, y + 1) for x, y in initial}, "a soft drop")
        session.screen.snapshot(SCREENS, "tetris-easy")
        easy_ms = session.measure_drop_interval(1500)
        session.key(b"q")
        session.text("Выбери игру")

        session.start(b"d", "Средняя")
        session.check_preview(visible=True)
        assert session.code() != original_code
        session.screen.snapshot(SCREENS, "tetris-normal")
        normal_ms = session.measure_drop_interval(950)
        session.key(b"q")
        session.text("Выбери игру")

        session.start(b"d", "Сложная")
        session.check_preview(visible=False)
        session.screen.snapshot(SCREENS, "tetris-hard")
        hard_ms = session.measure_drop_interval(500)
        hard_code = session.code()
        for _ in range(20):
            session.key(b"s" * 16)
            session.receive()
            if "Партия окончена" in session.screen.text():
                break
        session.text("Партия окончена")
        session.key(b"r")
        session.wait(lambda: len(session.falling()) == 4, "restarting after game over")
        session.check_preview(visible=False)
        assert session.code() == hard_code
        session.key(b"q")
        session.text("Выбери игру")
        level_two_ms = play_demo(session, DEMO_P2, DEMO_P2)
        assert easy_ms > normal_ms > hard_ms, (easy_ms, normal_ms, hard_ms)
        assert level_two_ms < normal_ms, (normal_ms, level_two_ms)
        print(f"PASS: measured automatic drop at {easy_ms}, {normal_ms}, {hard_ms} ms; "
              f"medium level 2 at {level_two_ms} ms")
        print("PASS: P2 challenge and deterministic replay")
    print("PASS: ESP32 UART, all difficulties, preview on easy/normal, pause, replay, loss, orders and level 2")


if __name__ == "__main__":
    main()
