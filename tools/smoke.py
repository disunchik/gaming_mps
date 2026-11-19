"""Exercise actual firmware menus, Tetris, Pong and challenge codes in ESP32/QEMU."""
import re
import shutil
import socket
import subprocess
import sys
from run import Qemu, ROOT
from terminal import mouse_bytes
sys.path.insert(0, str(ROOT / 'tests'))
from uart_session import Session, player_left
from terminal_model import TerminalModel


EXTREME = b'P2-6SW5-CD0J-0009T'


def main():
    artifacts = ROOT / 'artifacts'; artifacts.mkdir(exist_ok=True)
    with socket.socket() as occupied:
        occupied.bind(('127.0.0.1', 0)); occupied.listen()
        try:
            with Qemu(port=occupied.getsockname()[1]): raise AssertionError('occupied port accepted')
        except RuntimeError as error:
            assert 'занят' in str(error)
    with Qemu(port=5556) as qemu:
        wide = TerminalModel(width=120, height=45, background=0)
        s = Session(qemu, sink=wide.feed)
        def click(x, y, text):
            s.send(mouse_bytes(0, x, y) + mouse_bytes(0, x, y, True), text)
        def code():
            found = re.search(r'P2-[0-9A-Z]{4}-[0-9A-Z]{4}-[0-9A-Z]{5}', s.model.text())
            assert found, s.model.text()
            return found[0]
        s.wait('Выбери игру', 15); s.snapshot('main')
        notice = [row[18:82] for row in s.model.cells[10:30]]
        left = [row[:10] for row in s.model.cells[27:29]]
        s.pump(0.4)
        assert [row[:10] for row in s.model.cells[27:29]] != left
        assert [row[18:82] for row in s.model.cells[10:30]] == notice
        s.snapshot('rolling-left')
        assert all(name in s.model.text() for name in ['01 / КОВБОЙ ПАША', '02 / КОВБОЙ ТЁМА', '03 / КОВБОЙ ДИМА'])
        assert 'разработке' not in s.model.text()
        assert all(cell[0] == ' ' and cell[2] == 234 for y, row in enumerate(wide.cells)
                   for x, cell in enumerate(row) if x >= 100 or y >= 40)
        wide.snapshot(artifacts / 'screens', 'viewport-wide')
        click(30, 15, 'Меняет начальный темп'); click(30, 23, 'КАССА САЛУНА')
        s.wait('Убери 1 ряд'); s.snapshot('tetris')
        tetris_code = code()
        s.send(b'c', 'Скопировано'); assert code() == tetris_code
        s.send(b'q', 'Выбери игру')
        click(30, 17, 'Меняет долю мин')
        for _ in range(4): click(64, 19, 'Размер:')
        s.wait('16x16')
        click(42, 19, '14x14'); click(64, 19, '16x16')
        s.snapshot('settings')
        click(30, 23, 'Сапёр')
        s.send(mouse_bytes(35, 24, 13)); s.snapshot('mines-mouse')
        assert 'разработке' not in s.model.text() and 'Механика' not in s.model.text()
        assert 'seed:' not in s.model.text() and 'События:' not in s.model.text()
        s.send(b'q', 'Выбери игру')
        click(30, 19, 'Меняет только точность')
        click(64, 19, 'Уровень:'); click(64, 19, 'Уровень:')
        s.snapshot('pong-settings'); click(30, 23, 'Матч до 5 очков')
        s.wait('Уровень: 3'); original = code()
        start = player_left(s.model)
        s.send(mouse_bytes(35, 5, 20))
        s.wait('Матч до 5 очков', condition=lambda m: 0 <= player_left(m) < start)
        s.snapshot('pong')
        s.send(b'c', 'Скопировано'); assert code() == original
        click(35, 38, 'Скопировано')
        s.send(b'p', 'Пауза'); s.snapshot('pause')
        click(30, 22, 'Скопировано')  # Copy item in pause, not a resume.
        s.wait('Пауза')
        s.send(b'r', 'Матч до 5 очков'); assert code() == original
        s.send(b'q', 'Выбери игру')
        click(30, 19, 'Меняет только точность'); click(30, 23, 'Матч до 5 очков')
        assert code() != original
        s.send(b'q', 'Выбери игру'); click(30, 21, 'Введи код друга')
        legacy = b'P1-041020-R00007-GNHM2B-ZZYZJ4'
        s.send(legacy + b'\r', 'Нужен полный код P2'); s.snapshot('legacy-rejected')
        valid = b'P2-4SW5-CD0J-0228E'
        s.send(b'\x7f' * len(legacy) + valid[:-1] + b'G\r', 'Код повреждён'); s.snapshot('invalid-code')
        s.send(b'\x7f' * len(valid) + valid + b'\r', 'Код проверен')
        s.wait('Начальный уровень: 3'); s.snapshot('code-confirm')
        s.send(b'\r', 'Матч до 5 очков'); assert code() == 'P2-4SW5-CD0J-0228E'
        s.snapshot('pong-code')
        s.send(mouse_bytes(35, 3, 20))
        s.wait('', 40, condition=lambda m: 'Партия окончена' in m.text() or 'Уровень пройден' in m.text())
        s.snapshot('pong-result')
        s.send(b'q', 'Выбери игру'); click(30, 21, 'Введи код друга')
        # Independently generated P2 packet for level 4, normal, seed 0x12345678.
        s.send(EXTREME + b'\r', 'Начальный уровень: 4'); s.send(b'\r', 'БОТЫ')
        s.snapshot('pong-extreme')
        top = s.model.text().splitlines()[6]
        assert top.count('==========') == 2, top
        s.send(b'q', 'Выбери игру'); click(30, 23, 'ПРАВИЛА САЛУНА')
        s.wait('ПКМ / F — флаг'); s.snapshot('help')
        s.send(b'\x1b', 'Выбери игру')
        print('PASS: 100x40, real Tetris and Pong, Minesweeper stub, Pong keyboard/mouse, level selection, replay/fresh seed, P2/CRC and P1 rejected, copy/footer/pause, extreme two bots, clean panels, full viewport background')
    with Qemu(debug=True, port=5556):
        debugger = shutil.which('xtensa-esp32-elf-gdb'); assert debugger
        script = artifacts / 'step.gdb'
        script.write_text('set pagination off\nset confirm off\nset remotetimeout 15\n'
            'target remote 127.0.0.1:3333\nthbreak app_main\ncontinue\nlist\n'
            'p/x $pc\nnext\np/x $pc\nnext\np/x $pc\nframe\ndetach\n')
        result = subprocess.run([debugger, '--nx', '--batch', str(ROOT / 'build/gaming_mps.elf'), '-x', str(script)],
            capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
        output = result.stdout + result.stderr
        (artifacts / 'gdb-step.log').write_text(output, encoding='utf-8')
        pcs = re.findall(r'\$[123] = (0x[0-9a-f]+)', output)
        assert result.returncode == 0 and len(set(pcs)) == 3 and 'main.cpp' in output, output
        print('PASS: GDB app_main and two source steps:', ' -> '.join(pcs))


if __name__ == '__main__':
    main()
