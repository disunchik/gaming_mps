"""Integration test of real Windows console records -> transport -> ESP32 UART."""
from pathlib import Path
from native_output import console_rows
import json
import os
import re
import subprocess
import sys
import time
import traceback

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT))


def worker(report):
    """Own a console and verify keyboard, left/right click and motion end to end."""
    from terminal import Terminal, InputRecord, BufferInfo, C, W, kernel
    from run import Qemu
    from uart_session import Session, player_left
    from clipboard_support import verified_copy
    import socket
    assert C.sizeof(InputRecord) == 20
    with Terminal() as terminal, Qemu(port=5557) as qemu:
        session = Session(qemu, terminal.write, verified_copy)
        model = session.model
        def wait(text, condition=lambda _: True):
            session.wait(text, condition=condition)
            actual = console_rows(terminal)
            expected = model.text().splitlines()
            assert actual == expected, '\n'.join(f'{i}: {a!r} != {b!r}' for i, (a, b) in enumerate(zip(actual, expected)) if a != b)

        def inject(record):
            """Write one native input record to this test's own console queue."""
            written = W.DWORD()
            assert kernel.WriteConsoleInputW(terminal.input, C.byref(record), 1, C.byref(written)) and written.value == 1
            deadline = time.monotonic() + 2
            while time.monotonic() < deadline:
                data = terminal.read()
                if data:
                    qemu.serial.sendall(data)
                    return data
            raise AssertionError('No encoded console event')

        def mouse(x, y, buttons, flags=0):
            """Inject a mouse action at a visible terminal cell."""
            info = BufferInfo()
            assert kernel.GetConsoleScreenBufferInfo(terminal.output, C.byref(info))
            record = InputRecord(); record.type = 2
            record.event.mouse.position.X = x + info.window.Left
            record.event.mouse.position.Y = y + info.window.Top
            record.event.mouse.buttons = buttons
            record.event.mouse.flags = flags
            return inject(record)

        def key(virtual, character='\0', controls=0, down=True, tap=True):
            """Inject native press/release records without UI automation."""
            record = InputRecord(); record.type = 1
            record.event.key.down = down
            record.event.key.repeat = 1
            record.event.key.key = virtual
            record.event.key.character.UnicodeChar = character
            record.event.key.controls = controls
            data = inject(record)
            if down and tap and virtual in (37, 39, 65, 68):
                record.event.key.down = False
                inject(record)
            return data

        wait('Выбери игру')
        session.snapshot('native-main')
        assert key(40) == b'\x1b[B'
        mouse(30, 15, 1); mouse(30, 15, 0)
        wait('Меняет начальный темп')
        mouse(30, 23, 1); mouse(30, 23, 0)
        wait('КАССА САЛУНА')
        session.snapshot('native-tetris')
        tetris_code = re.search(r'P2-[0-9A-Z-]+', model.text())[0]
        assert key(80, 'з') == b'p'; wait('Пауза')
        assert key(67, 'с') == b'c'; wait('Скопировано')
        assert key(82, 'к') == b'r'; wait('КАССА САЛУНА')
        assert tetris_code in model.text()
        assert key(81, 'й') == b'q'; wait('Выбери игру')
        mouse(30, 19, 1); mouse(30, 19, 0)
        wait('Меняет только точность')
        mouse(30, 23, 1); mouse(30, 23, 0)
        wait('Матч до 5 очков')
        initial_left = player_left(model)
        assert key(68, 'в', tap=False) == b'\x1b[911~d'
        wait('Матч до 5 очков', condition=lambda m: player_left(m) >= initial_left + 5)
        first = player_left(model)
        session.pump(0.12)  # No repeated key-down records during this interval.
        assert player_left(model) > first, (first, player_left(model))
        assert key(68, down=False) == b'\x1b[912~'
        session.pump(0.12); stopped = player_left(model)
        session.pump(0.15); assert player_left(model) == stopped
        # A lost-focus event releases the direction, without changing real focus.
        key(65, 'ф', tap=False)
        wait('Матч до 5 очков', condition=lambda m: player_left(m) < stopped)
        focus = InputRecord(); focus.type = 16
        assert inject(focus) == b'\x1b[912~'
        session.pump(0.12); stopped = player_left(model)
        session.pump(0.15); assert player_left(model) == stopped
        before = player_left(model)
        assert mouse(6, 20, 0, 1).startswith(b'\x1b[<35;')
        wait('Матч до 5 очков', condition=lambda m: 0 <= player_left(m) < before)
        assert mouse(20, 11, 2).startswith(b'\x1b[<2;')
        mouse(20, 11, 0)
        key(68, 'd', tap=False)
        assert key(80, 'p') == b'p'
        wait('Пауза')
        assert key(68, down=False) == b'\x1b[912~'
        assert key(67, 'c') == b'c'
        wait('Скопировано')
        wait('Пауза')
        key(80, 'P', controls=0x10); wait('Матч до 5 очков')
        session.pump(0.1); stopped = player_left(model)
        session.pump(0.15); assert player_left(model) == stopped
        assert key(82, 'r') == b'r'
        wait('Матч до 5 очков', condition=lambda m: player_left(m) == 33)
        session.snapshot('native-windows')
        key(81, 'q'); wait('Выбери игру')
        mouse(30, 21, 1); mouse(30, 21, 0); wait('Введи код друга')
        russian = dict(zip('QWERTYUIOPASDFGHJKLZXCVBNM', 'ЙЦУКЕНГШЩЗФЫВАПРОЛДЯЧСМИТЬ'))
        for symbol in 'P2-4SW5-CD0J-0228E':
            key(ord(symbol) if symbol != '-' else 189, russian.get(symbol, symbol).lower())
        wait('p2-4sw5-cd0j-0228e')  # C remains code text rather than Copy here.
        def paste_up():
            assert key(38) == b'\x1b[A'
            wait('P2-4SW5-CD0J-0228E')
        assert verified_copy('P2-4SW5-CD0J-0228E', paste_up)
        def paste_ctrl_v():
            assert key(86, '\x16', controls=8) == b'\x16'
            wait('P2-4SW5-CD0J-0228E')
        assert verified_copy('P2-4SW5-CD0J-0228E', paste_ctrl_v)
        key(13, '\r'); wait('Код проверен')
        key(13, '\r'); wait('Матч до 5 очков')
        assert 'P2-4SW5-CD0J-0228E' in model.text()
        key(81, 'й'); wait('Выбери игру')
    Path(report).write_text(json.dumps({'passed': True,
        'checks': ['ReadConsoleInputW', 'WriteConsoleInputW', 'WriteConsoleW', 'ReadConsoleOutputCharacterW: all 100x44 cells match firmware', 'continuous hold without repeat, key-up, lost focus, release while paused', 'arrow', 'left click', 'right click', 'hover', 'pause', 'return', 'QEMU UART', 'Tetris field, glyphs and pause', 'RU and EN P/Q/R/C, physical WASD, RU code entry', 'real clipboard copy and read-back; original formats restored']}, indent=2), encoding='utf-8')


def main():
    """Run the worker in a separate Windows console, leaving parent handles alone."""
    if os.name != 'nt':
        print('SKIP: native console test requires Windows')
        return
    report = ROOT / 'artifacts/native-windows.json'
    if '--worker' in sys.argv:
        try:
            worker(report)
        except BaseException:
            report.write_text(json.dumps({'passed': False, 'error': traceback.format_exc()}, indent=2), encoding='utf-8')
            raise
        return
    if report.exists(): report.unlink()
    proc = subprocess.Popen([sys.executable, str(Path(__file__)), '--worker'],
                            creationflags=subprocess.CREATE_NEW_CONSOLE)
    try:
        result = proc.wait(timeout=60)
    except subprocess.TimeoutExpired:
        subprocess.run([r'C:\Windows\System32\taskkill.exe', '/PID', str(proc.pid), '/T', '/F'], check=False)
        raise
    content = json.loads(report.read_text(encoding='utf-8')) if report.exists() else {}
    assert result == 0 and content.get('passed'), content
    print('PASS: native Windows console 100x44, mouse/keyboard -> Tetris and Pong, real clipboard read-back; RU/EN, held movement, release and focus loss')


if __name__ == '__main__':
    main()
