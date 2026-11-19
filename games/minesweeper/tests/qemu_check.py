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
FIELD_X, FIELD_Y, FIELD_WIDTH, FIELD_HEIGHT = 2, 5, 72, 34
SIZE = 8
CELL_HEIGHT = min((FIELD_WIDTH - 4) // (2 * SIZE), (FIELD_HEIGHT - 2) // SIZE)
CELL_WIDTH = 2 * CELL_HEIGHT
BOARD_WIDTH, BOARD_HEIGHT = CELL_WIDTH * SIZE, CELL_HEIGHT * SIZE
BOARD_X = FIELD_X + (FIELD_WIDTH - BOARD_WIDTH) // 2
BOARD_Y = FIELD_Y + (FIELD_HEIGHT - BOARD_HEIGHT) // 2
CLOSED = "▪"


def click(x, y, right=False):
    """Encode a mouse press and release at absolute console coordinates."""
    button = 2 if right else 0
    return mouse_bytes(button, x, y) + mouse_bytes(button, x, y, True)


def cell_size(x, y):
    """Return the uniform cell dimensions for this board."""
    return CELL_WIDTH, CELL_HEIGHT


def cell_position(x, y):
    """Return the top-left console coordinates of a scaled tile."""
    return BOARD_X + x * CELL_WIDTH, BOARD_Y + y * CELL_HEIGHT


def cell_marker(x, y):
    """Return the console position of the glyph inside a tile."""
    column, row = cell_position(x, y)
    width, height = cell_size(x, y)
    return column + (width - 1) // 2, row + (height - 1) // 2


def cell_click(x, y, right=False, dx=0, dy=0):
    """Click inside a tile, including its outer edges."""
    column, row = cell_position(x, y)
    return click(column + dx, row + dy, right)


def board(model):
    """Read one glyph per cell from the actual console frame."""
    markers = (cell_marker(x, y) for y in range(SIZE) for x in range(SIZE))
    return "".join(model.cells[row][column][0] for column, row in markers)


def choose(session, label, expected):
    """Click the exact visible menu option or review button."""
    for y, row in enumerate(session.model.text().splitlines()):
        x = row.find(label)
        if x >= 0:
            session.send(click(x, y), expected)
            return
    raise AssertionError(f"Missing option: {label}")


def check_review(session, detonated=False):
    """Inspect the revealed layout with the original code and a restart button."""
    model = session.model
    assert "К результату" in model.text() and CURRENT_CODE in model.text()
    cells = board(model)
    assert {i for i, glyph in enumerate(cells) if glyph in "■X"} == MINES
    assert cells.count("X") == int(detonated)
    assert CLOSED not in cells
    assert any("Начать заново" in row[77:98] for row in model.text().splitlines())


def restart_fresh(session):
    """Start the same size on a new seed and open the same first cell."""
    model = session.model
    choose(session, "Начать заново", "Код после первого открытия")
    assert "Поле: 8x8" in model.text() and "Динамиты: 10" in model.text()
    assert "Цена ошибки: 0" in model.text() and "Время, с: 0" in model.text()
    assert board(model) == CLOSED * (SIZE * SIZE)
    session.send(cell_click(0, 1), "Копировать [C]")
    assert P2_PATTERN.search(model.text()).group() != CURRENT_CODE


def replay(session, prefer_legacy=False):
    """Enter a code through the menu and wait for the restored board."""
    session.send(click(30, 21), "Введи код друга")
    accepts_legacy = "P1 тоже принимается" in session.model.text()
    code = LEGACY_CODE if prefer_legacy and accepts_legacy else CURRENT_CODE
    session.send(code.encode("ascii") + b"\r", "Код проверен")
    session.send(b"\r", "Поле: 8x8")
    assert CURRENT_CODE in session.model.text() and board(session.model)[8] == " "


def main():
    """Check canonical codes, both results, full board review and fresh starts."""
    with Qemu(port=5557) as qemu:
        session = Session(qemu)
        model = session.model
        session.wait("Выбери игру", 15)
        session.send(click(30, 17), "Меняет долю динамита")
        session.send(b"ss\r", "Поле: 8x8")
        assert board(model) == CLOSED * 64
        assert "Код после первого открытия" in model.text()
        assert "ДИНАМИТНАЯ ДОЛИНА" in model.text()
        assert model.text().splitlines()[5][77:98].startswith("Цена ошибки: 0")
        assert "Динамиты: 10" in model.text() and "Enter/ЛКМ" not in model.text()

        width, height = cell_size(7, 7)
        session.send(cell_click(7, 7, right=True, dx=width - 1, dy=height - 1), "Банданы: 1")
        assert board(model)[-1] == "▼" and "Цена ошибки: 0" in model.text()
        flag_x, flag_y = cell_marker(7, 7)
        assert model.cells[flag_y][flag_x][1] == 167  # Theme::red
        session.send(cell_click(7, 7) + cell_click(7, 7, right=True), "Банданы: 0")
        assert board(model) == CLOSED * 64

        session.send(b"\x1b[D\r", "Копировать [C]")
        session.wait("Копировать [C]", condition=lambda m: board(m)[62] != CLOSED)
        initial_board = board(model)
        opening_score = initial_board.count(" ") + sum(initial_board.count(str(i)) for i in range(1, 9))
        assert f"Цена ошибки: {opening_score}" in model.text()
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
        closed_color = model.cells[target_y][target_x][1]
        hover_clue = mouse_bytes(35, *cell_position(3, 2))
        session.send(hover_clue)
        session.wait("Поле: 8x8", condition=lambda m: m.cells[target_y][target_x][1] != closed_color)
        preview_color = model.cells[target_y][target_x][1]
        session.send(cell_click(3, 1, right=True), "Банданы: 1")
        session.send(hover_clue)
        session.wait("Поле: 8x8", condition=lambda m: m.cells[target_y][target_x][1] == 179)
        assert model.cells[target_y][target_x][1] != preview_color
        assert board(model)[12] == CLOSED
        session.send(cell_click(3, 2))
        session.wait("Поле: 8x8", condition=lambda m: board(m)[12] != CLOSED)

        session.send(b"r", "Поле: 8x8")
        session.wait("Банданы: 0", condition=lambda m: board(m) == legacy_board)
        session.send(cell_click(4, 1, right=True), "Банданы: 1")
        session.send(cell_click(3, 2), "Не заметил растяжку")
        choose(session, "Посмотреть все поле", "К результату")
        check_review(session, detonated=True)
        mark_x, mark_y = cell_position(4, 1)
        _, glyph_row = cell_marker(4, 1)
        assert model.cells[glyph_row][mark_x + cell_size(4, 1)[0] - 1][0] == "!"
        choose(session, "К результату", "Посмотреть все поле")
        choose(session, "Посмотреть все поле", "К результату")
        restart_fresh(session)
        session.send(b"q", "Выбери игру")
        replay(session)

        session.send(cell_click(3, 1, right=True), "Банданы: 1")
        safe_clicks = b"".join(cell_click(i % SIZE, i // SIZE)
                               for i in range(SIZE * SIZE) if i not in MINES)
        session.send(safe_clicks, "Поле зачищено")
        assert "Цена ошибки: 54" in model.text()
        assert "Следующий уровень" in model.text()
        session.send(b"ss\r", "К результату")
        check_review(session)
        mark_x, mark_y = cell_position(3, 1)
        _, glyph_row = cell_marker(3, 1)
        assert model.cells[glyph_row][mark_x + cell_size(3, 1)[0] - 1][0] == "▼"
        restart_fresh(session)
        session.send(b"q", "Выбери игру")
    print("PASS: ESP32/QEMU, price of error, P2 replay, win and loss, full board review, fresh keys.")


if __name__ == "__main__":
    main()
