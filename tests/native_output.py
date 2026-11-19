"""Check actual Windows console cells after deliberately fragmented UTF-8."""
from pathlib import Path
import json
import os
import subprocess
import sys
import traceback

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))


def console_rows(terminal):
    """Read the visible 100x44 characters from the real console, not a VT model."""
    from terminal import C, W, kernel, Coord, BufferInfo
    kernel.ReadConsoleOutputCharacterW.argtypes = [W.HANDLE, W.LPWSTR, W.DWORD, Coord, C.POINTER(W.DWORD)]
    info = BufferInfo()
    assert kernel.GetConsoleScreenBufferInfo(terminal.output, C.byref(info))
    rows = []
    for y in range(44):
        buffer = C.create_unicode_buffer(101)
        count = W.DWORD()
        assert kernel.ReadConsoleOutputCharacterW(terminal.output, buffer, 100,
            Coord(info.window.Left, info.window.Top + y), C.byref(count))
        assert count.value == 100
        rows.append(buffer[:100])
    return rows


def worker(legacy=False):
    """Split every Cyrillic and box-drawing glyph between output writes."""
    from terminal import Terminal
    expected = '╔══ САЛУН «ТРИ СТВОЛА» ══╗'
    with Terminal() as terminal:
        payload = ('\x1b[?1049h\x1b[2J\x1b[1;1H' + expected + '\x1b[2;100H║').encode('utf-8')
        for byte in payload:
            if legacy:
                sys.stdout.buffer.write(bytes([byte]))
                sys.stdout.buffer.flush()
            else:
                terminal.write(bytes([byte]))
        rows = console_rows(terminal)
        assert rows[0].startswith(expected), repr(rows[0])
        assert rows[1][99] == '║', repr(rows[1])
        assert not any('\ufffd' in row for row in rows)
    return {'passed': True, 'checks': ['UTF-8 split at every byte', 'ReadConsoleOutputCharacterW', 'right edge column 100']}


def main():
    """Use a separate owned console, without touching the user's running game."""
    if os.name != 'nt':
        print('SKIP: Windows console required')
        return
    report = ROOT / 'artifacts/native-output.json'
    report.parent.mkdir(exist_ok=True)
    if '--worker' in sys.argv:
        try:
            result = worker('--legacy' in sys.argv)
        except BaseException:
            result = {'passed': False, 'error': traceback.format_exc()}
        report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        sys.exit(0 if result['passed'] else 1)
    report.unlink(missing_ok=True)
    args = [sys.executable, str(Path(__file__)), '--worker']
    if '--legacy' in sys.argv: args.append('--legacy')
    proc = subprocess.Popen(args, creationflags=subprocess.CREATE_NEW_CONSOLE)
    try:
        code = proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
        proc.kill()
        raise
    result = json.loads(report.read_text(encoding='utf-8'))
    assert code == 0 and result.get('passed'), result
    print('PASS: actual Windows console preserves fragmented UTF-8 and right edge')


if __name__ == '__main__':
    main()
