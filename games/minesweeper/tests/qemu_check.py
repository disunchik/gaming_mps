"""Exercise real Minesweeper firmware through the shared QEMU UART launcher."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT / "tools"), str(ROOT / "tests")]
from run import Qemu
from terminal import mouse_bytes
from uart_session import Session

# Equivalent codes for 8x8, Normal, seed 0, first cell (0, 1).
LEGACY_CODE = "P1-040G20-800000-000000-002DY9"
CURRENT_CODE = "P2-0M00-0000-20G5Y"
MINES = {2, 11, 21, 23, 31, 38, 39, 41, 42, 54}
P2_PATTERN = re.compile(r"P2-[0-9A-Z]{4}-[0-9A-Z]{4}-[0-9A-Z]{5}")

# Match Screen::fieldX, fieldY, width and height.
FIELD_X, FIELD_Y, FIELD_WIDTH, FIELD_HEIGHT = 2, 5, 72, 30
SIZE, CELL_WIDTH, CELL_HEIGHT = 8, 7, 3
CLOSED = "▪"


def click(x, y, right=False):
    """Encode a mouse press and release at absolute console coordinates."""
    button = 2 if right else 0
    return mouse_bytes(button, x, y) + mouse_bytes(button, x, y, True)


def cell_position(x, y):
    """Return the top-left console coordinates of a tile on the 8x8 board."""
    left = FIELD_X + (FIELD_WIDTH - CELL_WIDTH * SIZE) // 2
    top = FIELD_Y + (FIELD_HEIGHT - CELL_HEIGHT * SIZE) // 2
    return left + CELL_WIDTH * x, top + CELL_HEIGHT * y


def cell_marker(x, y):
    """Return the console position of the glyph inside a tile."""
    column, row = cell_position(x, y)
    return column + (CELL_WIDTH - 2) // 2, row + (CELL_HEIGHT - 1) // 2


def cell_click(x, y, right=False, dx=0, dy=0):
    """Click inside a tile, including its outer edges."""
    column, row = cell_position(x, y)
    return click(column + dx, row + dy, right)


def board(model):
    """Read one glyph per cell from the actual console frame."""
    markers = (cell_marker(x, y) for y in range(SIZE) for x in range(SIZE))
    return "".join(model.cells[row][column][0] for column, row in markers)


def replay(session, prefer_legacy=False):
    """Enter a code through the menu and wait for the restored board."""
    session.send(click(30, 21), "Введи код друга")
    accepts_legacy = "P1 тоже принимается" in session.model.text()
    code = LEGACY_CODE if prefer_legacy and accepts_legacy else CURRENT_CODE
    session.send(code.encode("ascii") + b"\r", "Код проверен")
    session.send(b"\r", "Поле: 8x8")
    assert CURRENT_CODE in session.model.text() and board(session.model)[8] == " "


def main():
    """Check the first move, canonical codes, outcomes and next board size."""
    with Qemu(port=5557) as qemu:
        session = Session(qemu)
        model = session.model
        session.wait("Выбери игру", 15)
        session.send(click(30, 17), "Меняет долю динамита")
        session.send(b"ss\r", "Поле: 8x8")
        assert board(model) == CLOSED * 64
        assert "Код после первого открытия" in model.text()
        assert "ШАХТА САЛУНА" in model.text() and "СВОДКА ШАХТЫ" in model.text()

        session.send(cell_click(7, 7, right=True, dx=CELL_WIDTH - 1, dy=CELL_HEIGHT - 1), "Флагов: 1")
        assert board(model)[-1] == "F"
        session.send(cell_click(7, 7) + cell_click(7, 7, right=True), "Флагов: 0")
        assert board(model) == CLOSED * 64

        session.send(b"\x1b[D\r", "Копировать [C]")
        session.wait("Копировать [C]", condition=lambda m: board(m)[62] != CLOSED)
        initial_board = board(model)
        code = P2_PATTERN.search(model.text()).group()
        session.wait("Время, с: 1")
        session.send(b"p", "Пауза")
        session.send(b"r", "Поле: 8x8")
        assert board(model) == initial_board and code in model.text()
        assert "Время, с: 0" in model.text()
        session.send(b"q", "Выбери игру")

        replay(session, prefer_legacy=True)
        legacy_board = board(model)
        session.send(b"c", "Скопировано")
        session.send(b"q", "Выбери игру")
        replay(session)
        assert board(model) == legacy_board

        # On this board cell (3, 2) is an open 1 next to the mine at (3, 1).
        target_x, target_y = cell_marker(4, 1)
        closed_color = model.cells[target_y][target_x][2]
        hover_clue = mouse_bytes(35, *cell_position(3, 2))
        session.send(hover_clue, "Ещё флагов: 1")
        preview_color = model.cells[target_y][target_x][2]
        assert preview_color != closed_color
        session.send(cell_click(3, 1, right=True), "Флагов: 1")
        session.send(hover_clue, "Можно раскрыть")
        assert model.cells[target_y][target_x][2] != preview_color
        assert board(model)[12] == CLOSED
        session.send(cell_click(3, 2))
        session.wait("Поле: 8x8", condition=lambda m: board(m)[12] != CLOSED)

        session.send(b"r", "Поле: 8x8")
        session.wait("Флагов: 0", condition=lambda m: board(m) == legacy_board)
        session.send(cell_click(4, 1, right=True), "Флагов: 1")
        session.send(cell_click(3, 2), "Партия окончена")
        session.send(b"r", "Поле: 8x8")
        session.send(cell_click(2, 0), "Партия окончена")
        session.send(b"r", "Поле: 8x8")

        safe_clicks = b"".join(cell_click(i % SIZE, i // SIZE)
                               for i in range(SIZE * SIZE) if i not in MINES)
        session.send(safe_clicks, "Уровень пройден")
        session.send(b"\r", "Поле: 10x10")
        assert "Динамиты: 15" in model.text() and "Код после первого открытия" in model.text()
        session.send(b"q", "Выбери игру")
    print("PASS: ESP32/QEMU, large tiles, keyboard, mouse, flags, P2 replay, chord, win, loss and next board.")


if __name__ == "__main__":
    main()
