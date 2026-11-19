"""Native terminal input. Only transport and key/mouse encoding live on the PC."""
import os
import sys
import codecs


def mouse_bytes(button, x, y, release=False):
    """Encode zero-based terminal coordinates as an SGR mouse sequence."""
    return f'\x1b[<{button};{x + 1};{y + 1}{"m" if release else "M"}'.encode('ascii')


def key_bytes(key, char='\0', repeat=1, controls=0):
    """Encode physical Windows keys independently of the active keyboard layout."""
    count = max(1, repeat)
    if key == 121: return b'\x1d'  # F10 exits the transport.
    arrows = {37: b'\x1b[D', 38: b'\x1b[A', 39: b'\x1b[C', 40: b'\x1b[B'}
    if key in arrows: return arrows[key] * count
    # Ctrl/Alt combinations keep their native character (including Ctrl+C/]).
    if 65 <= key <= 90 and not controls & 0x0f:
        char = chr(key if bool(controls & 0x10) != bool(controls & 0x80) else key + 32)
    return char.encode('utf-8') * count if char and char != '\0' else b''


class KeyboardInput:
    """Encode press/release state; movement and timing stay on the ESP32."""
    directions = {37: -1, 39: 1, 65: -1, 68: 1}

    def __init__(self):
        self.held = []

    def axis_bytes(self):
        direction = self.directions[self.held[-1]] if self.held else 0
        return {-1: b'\x1b[910~', 1: b'\x1b[911~', 0: b'\x1b[912~'}[direction]

    def feed(self, key, char='\0', down=True, repeat=1, controls=0):
        """Keep ordinary taps for menus/Tetris alongside optional held events."""
        state = b''
        if key in self.directions:
            if down and not controls & 0x0f:
                if key not in self.held:
                    self.held.append(key)
                state = self.axis_bytes()
            elif not down and key in self.held:
                self.held.remove(key)
                state = self.axis_bytes()
        return state + (key_bytes(key, char, repeat, controls) if down else b'')

    def release_all(self):
        """Prevent a stuck direction when the console loses focus."""
        active = bool(self.held)
        self.held.clear()
        return self.axis_bytes() if active else b''


if os.name == 'nt':
    import ctypes as C
    from ctypes import wintypes as W

    class Coord(C.Structure):
        """Windows console column and row."""
        _fields_ = [('X', W.SHORT), ('Y', W.SHORT)]

    class SmallRect(C.Structure):
        """Visible console rectangle."""
        _fields_ = [('Left', W.SHORT), ('Top', W.SHORT), ('Right', W.SHORT), ('Bottom', W.SHORT)]

    class BufferInfo(C.Structure):
        """Console dimensions and scroll offsets."""
        _fields_ = [('size', Coord), ('cursor', Coord), ('attributes', W.WORD),
                    ('window', SmallRect), ('maximum', Coord)]

    class KeyChar(C.Union):
        """Unicode/ANSI character union in KEY_EVENT_RECORD."""
        _fields_ = [('UnicodeChar', W.WCHAR), ('AsciiChar', C.c_char)]

    class KeyEvent(C.Structure):
        """Windows keyboard input record."""
        _fields_ = [('down', W.BOOL), ('repeat', W.WORD), ('key', W.WORD),
                    ('scan', W.WORD), ('character', KeyChar), ('controls', W.DWORD)]

    class MouseEvent(C.Structure):
        """Windows mouse button/motion input record."""
        _fields_ = [('position', Coord), ('buttons', W.DWORD), ('controls', W.DWORD), ('flags', W.DWORD)]

    class Event(C.Union):
        """Largest supported INPUT_RECORD event storage."""
        _fields_ = [('key', KeyEvent), ('mouse', MouseEvent), ('raw', C.c_byte * 16)]

    class InputRecord(C.Structure):
        """Native ReadConsoleInputW / WriteConsoleInputW record."""
        _fields_ = [('type', W.WORD), ('event', Event)]

    kernel = C.WinDLL('kernel32', use_last_error=True)
    kernel.GetStdHandle.argtypes = [W.DWORD]
    kernel.GetStdHandle.restype = W.HANDLE
    kernel.GetConsoleMode.argtypes = [W.HANDLE, C.POINTER(W.DWORD)]
    kernel.SetConsoleMode.argtypes = [W.HANDLE, W.DWORD]
    kernel.ReadConsoleInputW.argtypes = [W.HANDLE, C.POINTER(InputRecord), W.DWORD, C.POINTER(W.DWORD)]
    kernel.WriteConsoleInputW.argtypes = [W.HANDLE, C.POINTER(InputRecord), W.DWORD, C.POINTER(W.DWORD)]
    kernel.WaitForSingleObject.argtypes = [W.HANDLE, W.DWORD]
    kernel.GetConsoleScreenBufferInfo.argtypes = [W.HANDLE, C.POINTER(BufferInfo)]
    kernel.SetConsoleScreenBufferSize.argtypes = [W.HANDLE, Coord]
    kernel.SetConsoleWindowInfo.argtypes = [W.HANDLE, W.BOOL, C.POINTER(SmallRect)]
    kernel.WriteConsoleW.argtypes = [W.HANDLE, W.LPCWSTR, W.DWORD, C.POINTER(W.DWORD), C.c_void_p]

    class Terminal:
        """Read real Windows console events, including right click and hover."""
        def __init__(self):
            """Capture standard handles without changing their modes yet."""
            self.input = kernel.GetStdHandle(-10 & 0xffffffff)
            self.output = kernel.GetStdHandle(-11 & 0xffffffff)
            self.input_mode = W.DWORD()
            self.output_mode = W.DWORD()
            self.previous_buttons = 0
            self.keyboard = KeyboardInput()
            self.codepage = kernel.GetConsoleOutputCP()
            self.decoder = codecs.getincrementaldecoder('utf-8')('replace')

        def __enter__(self):
            """Enable mouse events and UTF-8 VT output; disable Quick Edit."""
            if not kernel.GetConsoleMode(self.input, C.byref(self.input_mode)) or not kernel.GetConsoleMode(self.output, C.byref(self.output_mode)):
                raise RuntimeError('Запусти в Windows Terminal, PowerShell или терминале VS Code.')
            info = BufferInfo()
            kernel.GetConsoleScreenBufferInfo(self.output, C.byref(info))
            width = info.window.Right - info.window.Left + 1
            height = info.window.Bottom - info.window.Top + 1
            if width < 100 or height < 44:
                kernel.SetConsoleScreenBufferSize(self.output, Coord(max(100, info.size.X), max(44, info.size.Y)))
                window = SmallRect(0, 0, max(100, width) - 1, max(44, height) - 1)
                kernel.SetConsoleWindowInfo(self.output, True, C.byref(window))
                kernel.GetConsoleScreenBufferInfo(self.output, C.byref(info))
                if info.window.Right - info.window.Left + 1 < 100 or info.window.Bottom - info.window.Top + 1 < 44:
                    raise RuntimeError('Увеличь терминал минимум до 100 столбцов и 44 строки.')
            if not kernel.SetConsoleMode(self.output, self.output_mode.value | 0x4):
                raise C.WinError(C.get_last_error())
            mode = (self.input_mode.value | 0x8 | 0x10 | 0x80) & ~(0x2 | 0x4 | 0x40 | 0x200)
            if not kernel.SetConsoleMode(self.input, mode):
                kernel.SetConsoleMode(self.output, self.output_mode.value)
                raise C.WinError(C.get_last_error())
            kernel.SetConsoleOutputCP(65001)
            return self

        def write(self, data):
            """Keep UTF-8 state across UART chunks and write native Unicode VT text."""
            text = self.decoder.decode(data)
            if not text:
                return
            # WriteConsoleW counts UTF-16 code units, rather than Python characters.
            encoded = text.encode('utf-16-le')
            buffer = C.create_string_buffer(encoded)
            offset = 0
            units = len(encoded) // 2
            while offset < units:
                written = W.DWORD()
                pointer = C.cast(C.byref(buffer, offset * 2), W.LPCWSTR)
                if not kernel.WriteConsoleW(self.output, pointer, units - offset, C.byref(written), None):
                    raise C.WinError(C.get_last_error())
                if not written.value:
                    raise OSError('WriteConsoleW wrote no characters')
                offset += written.value

        def read(self):
            """Return encoded input, an exit marker, or no event after 50 ms."""
            if kernel.WaitForSingleObject(self.input, 50) != 0:
                return b''
            record = InputRecord()
            count = W.DWORD()
            if not kernel.ReadConsoleInputW(self.input, C.byref(record), 1, C.byref(count)):
                raise C.WinError(C.get_last_error())
            if record.type == 1:
                event = record.event.key
                return self.keyboard.feed(event.key, event.character.UnicodeChar,
                                          bool(event.down), event.repeat, event.controls)
            if record.type == 16 and not record.event.raw[0]:
                return self.keyboard.release_all()
            if record.type == 2:
                event = record.event.mouse
                info = BufferInfo()
                kernel.GetConsoleScreenBufferInfo(self.output, C.byref(info))
                x = event.position.X - info.window.Left
                y = event.position.Y - info.window.Top
                buttons = event.buttons
                result = b''
                if event.flags == 1:
                    result = mouse_bytes(32 if buttons & 1 else 34 if buttons & 2 else 35, x, y)
                elif event.flags in (0, 2):
                    for mask, button in [(1, 0), (2, 2)]:
                        if buttons & mask and (not self.previous_buttons & mask or event.flags == 2):
                            result += mouse_bytes(button, x, y)
                        elif self.previous_buttons & mask and not buttons & mask:
                            result += mouse_bytes(button, x, y, True)
                self.previous_buttons = buttons
                return result
            return b''

        def __exit__(self, *_):
            """Restore the terminal even after an exception or Ctrl+C."""
            self.write(b'\x1b[?1003l\x1b[?1006l\x1b[?25h\x1b[0m\x1b[?1049l')
            kernel.SetConsoleMode(self.input, self.input_mode.value)
            kernel.SetConsoleMode(self.output, self.output_mode.value)
            kernel.SetConsoleOutputCP(self.codepage)
else:
    import select
    import termios
    import tty

    class Terminal:
        """POSIX raw terminal; the primary supported team platform is Windows."""
        def __enter__(self):
            """Save input mode and enable immediate byte delivery."""
            self.mode = termios.tcgetattr(sys.stdin.fileno())
            tty.setraw(sys.stdin.fileno())
            return self

        def read(self):
            """Forward native VT sequences, including SGR mouse reports."""
            if select.select([sys.stdin], [], [], 0.05)[0]:
                return os.read(sys.stdin.fileno(), 4096)
            return b''

        def write(self, data):
            """Forward raw UTF-8 to a POSIX terminal, which retains decoder state."""
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()

        def __exit__(self, *_):
            """Restore input and disable the application's terminal modes."""
            sys.stdout.buffer.write(b'\x1b[?1003l\x1b[?1006l\x1b[?25h\x1b[0m\x1b[?1049l')
            sys.stdout.buffer.flush()
            termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, self.mode)
