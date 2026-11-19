"""Integration test of real Windows console records -> transport -> ESP32 UART."""
from pathlib import Path
from native_output import console_rows
import json
import os
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

        def key(virtual, character='\0'):
            """Inject one native key-down record without UI automation."""
            record = InputRecord(); record.type = 1
            record.event.key.down = True
            record.event.key.repeat = 1
            record.event.key.key = virtual
            record.event.key.character.UnicodeChar = character
            return inject(record)

        wait('Выбери игру')
        session.snapshot('native-main')
        assert key(40) == b'\x1b[B'
        mouse(30, 15, 1); mouse(30, 15, 0)
        wait('Меняет начальный темп')
        mouse(30, 21, 1); mouse(30, 21, 0)
        wait('КАССА САЛУНА')
        session.snapshot('native-tetris')
        key(80, 'p'); wait('Пауза')
        key(80, 'p'); wait('КАССА САЛУНА')
        key(81, 'q'); wait('Выбери игру')
        mouse(30, 19, 1); mouse(30, 19, 0)
        wait('Меняет только точность')
        mouse(30, 21, 1); mouse(30, 21, 0)
        wait('Матч до 5 очков')
        before = player_left(model)
        assert mouse(6, 20, 0, 1).startswith(b'\x1b[<35;')
        wait('Матч до 5 очков', condition=lambda m: 0 <= player_left(m) < before)
        assert mouse(20, 11, 2).startswith(b'\x1b[<2;')
        mouse(20, 11, 0)
        assert key(80, 'p') == b'p'
        wait('Пауза')
        assert key(67, 'c') == b'c'
        wait('Скопировано')
        wait('Пауза')
        key(80, 'p'); wait('Матч до 5 очков')
        session.snapshot('native-windows')
        key(81, 'q'); wait('Выбери игру')
    Path(report).write_text(json.dumps({'passed': True,
        'checks': ['ReadConsoleInputW', 'WriteConsoleInputW', 'WriteConsoleW', 'ReadConsoleOutputCharacterW: all 100x40 cells match firmware', 'arrow', 'left click', 'right click', 'hover', 'pause', 'return', 'QEMU UART', 'Tetris field, glyphs and pause', 'real clipboard copy and read-back; original formats restored']}, indent=2), encoding='utf-8')


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
    print('PASS: native Windows console 100x40, mouse/keyboard -> Tetris and Pong, real clipboard read-back')


if __name__ == '__main__':
    main()
