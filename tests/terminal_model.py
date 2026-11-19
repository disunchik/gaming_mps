"""A small model of the exact ANSI output emitted by common/screen.cpp."""
import codecs
import html
import json
from pathlib import Path


class TerminalModel:
    """Recover actual emulated screen cells from fragmented UART output."""
    def __init__(self, width=100, height=44, background=234):
        """Start with configurable viewport geometry and the terminal default background."""
        self.decoder = codecs.getincrementaldecoder('utf-8')('replace')
        self.width, self.height = width, height
        self.default_bg = background
        self.cells = [[[' ', 223, background] for _ in range(width)] for _ in range(height)]
        self.x = self.y = 0
        self.fg, self.bg = 223, background
        self.escape = ''
        self.complete = False

    def feed(self, data):
        """Accept arbitrary UART chunks, preserving incomplete CSI and UTF-8."""
        for char in self.decoder.decode(data):
            if self.escape:
                self.escape += char
                if len(self.escape) >= 3 and '@' <= char <= '~':
                    if self.escape.startswith('\x1b['): self.csi(self.escape[2:-1], char)
                    self.escape = ''
                elif len(self.escape) > 128: self.escape = ''
            elif char == '\x1b': self.escape = char
            elif char == '\r': self.x = 0
            elif char == '\n': self.y = min(self.height - 1, self.y + 1)
            elif ord(char) >= 32:
                if self.x < self.width: self.cells[self.y][self.x] = [char, self.fg, self.bg]
                self.x = min(self.width, self.x + 1)

    def csi(self, parameters, command):
        """Implement cursor placement, clear and indexed foreground/background."""
        if parameters.startswith('?'): return
        args = [int(value or '0') for value in parameters.split(';')]
        if command in ('H', 'f'):
            self.complete = False
            self.y = max(0, min(self.height - 1, (args[0] or 1) - 1))
            self.x = max(0, min(self.width - 1, (args[1] if len(args) > 1 and args[1] else 1) - 1))
        elif command == 'J' and args == [2]:
            self.cells = [[[' ', self.fg, self.bg] for _ in range(self.width)] for _ in range(self.height)]
        elif command == 'm':
            i = 0
            while i < len(args):
                if args[i] in (38, 48) and args[i + 1:i + 2] == [5] and i + 2 < len(args):
                    if args[i] == 38: self.fg = args[i + 2]
                    else: self.bg = args[i + 2]
                    i += 3
                else:
                    if args[i] == 0:
                        self.fg, self.bg = 223, self.default_bg
                        self.complete = True
                    i += 1

    def text(self):
        """Return the visible screen without ANSI or colour metadata."""
        return '\n'.join(''.join(cell[0] for cell in row) for row in self.cells)

    def snapshot(self, directory, name):
        """Save actual firmware cells as text, JSON and a labelled SVG rendering."""
        directory = Path(directory)
        directory.mkdir(parents=True, exist_ok=True)
        (directory / f'{name}.txt').write_text(self.text() + '\n', encoding='utf-8')
        (directory / f'{name}.json').write_text(json.dumps(self.cells, ensure_ascii=False), encoding='utf-8')
        def color(index):
            """Convert an xterm palette index to its specified RGB colour."""
            if index >= 232:
                v = 8 + 10 * (index - 232)
                return f'#{v:02x}{v:02x}{v:02x}'
            if index >= 16:
                index -= 16
                levels = [0, 95, 135, 175, 215, 255]
                return '#%02x%02x%02x' % (levels[index // 36], levels[(index // 6) % 6], levels[index % 6])
            return '#ffffff'
        svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.width * 12}" height="{self.height * 22}" viewBox="0 0 {self.width * 12} {self.height * 22}">',
               '<title>Кадр прошивки QEMU, восстановленный из UART</title>']
        for y, row in enumerate(self.cells):
            for x, (char, fg, bg) in enumerate(row):
                svg.append(f'<rect x="{x * 12}" y="{y * 22}" width="12" height="22" fill="{color(bg)}"/>')
                if char != ' ':
                    svg.append(f'<text x="{x * 12}" y="{y * 22 + 17}" fill="{color(fg)}" font-family="Consolas,monospace" font-size="18">{html.escape(char)}</text>')
        svg.append('</svg>')
        (directory / f'{name}.svg').write_text('\n'.join(svg), encoding='utf-8')
