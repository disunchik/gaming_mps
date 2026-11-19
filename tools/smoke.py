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
        s = Session(qemu)
        def click(x, y, text):
            s.send(mouse_bytes(0, x, y) + mouse_bytes(0, x, y, True), text)
        def code():
            found = re.search(r'P2-[0-9A-Z]{4}-[0-9A-Z]{4}-[0-9A-Z]{5}', s.model.text())
            assert found, s.model.text()
            return found[0]
        s.wait('Выбери игру', 15); s.snapshot('main')
        click(30, 15, 'Меняет начальный темп'); click(30, 21, 'КАССА САЛУНА')
        s.wait('Убери 1 ряд'); s.snapshot('tetris')
        tetris_code = code()
        s.send(b'c', 'Скопировано'); assert code() == tetris_code
        s.send(b'q', 'Выбери игру')
        click(30, 17, 'Меняет долю мин')
        for _ in range(4): click(30, 17, 'Размер:')
        s.wait('16x16'); s.snapshot('settings')
        click(30, 21, 'Сапёр — в разработке')
        s.send(mouse_bytes(35, 24, 13)); s.snapshot('mines-mouse')
        assert 'seed:' not in s.model.text() and 'События:' not in s.model.text()
        s.send(b'q', 'Выбери игру')
        click(30, 19, 'Меняет только точность')
        click(30, 17, 'Уровень: < 2 >'); click(30, 17, 'Уровень: < 3 >')
        s.snapshot('pong-settings'); click(30, 21, 'Матч до 5 очков')
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
        click(30, 19, 'Меняет только точность'); click(30, 21, 'Матч до 5 очков')
        assert code() != original
        s.send(b'q', 'Выбери игру'); click(30, 21, 'Введи код друга')
        legacy = b'P1-041020-R00007-GNHM2B-ZZYZJ4'
        s.send(legacy[:-1] + b'5\r', 'Код повреждён'); s.snapshot('invalid-code')
        s.send(b'\x7f' * len(legacy) + legacy + b'\r', 'Код проверен')
        s.wait('Начальный уровень: 3'); s.snapshot('code-confirm')
        s.send(b'\r', 'Матч до 5 очков'); assert code() == 'P2-4SW5-CD0J-0228E'
        s.snapshot('pong-code')
        s.send(mouse_bytes(35, 3, 20))
        s.wait('', 40, condition=lambda m: 'Партия окончена' in m.text() or 'Уровень пройден' in m.text())
        s.snapshot('pong-result')
        s.send(b'q', 'Выбери игру'); click(30, 21, 'Введи код друга')
        # Independently generated P2 packet for level 4, normal, seed 0x12345678.
        s.send(EXTREME + b'\r', 'Начальный уровень: 4'); s.send(b'\r', 'ТЫ       БОТЫ')
        s.snapshot('pong-extreme')
        top = s.model.text().splitlines()[6]
        assert top.count('==========') == 2, top
        s.send(b'q', 'Выбери игру'); click(30, 23, 'Управление')
        s.wait('ПКМ / F — флаг'); s.snapshot('help')
        s.send(b'\x1b', 'Выбери игру')
        print('PASS: 100x40, real Tetris and Pong, Minesweeper stub, Pong keyboard/mouse, level selection, replay/fresh seed, P1/P2/CRC, copy/footer/pause, extreme two bots, clean panels')
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
