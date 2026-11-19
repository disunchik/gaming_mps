"""The same UART/clipboard output path for QEMU and native-console verification."""
from pathlib import Path
import socket
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from clipboard import ClipboardBridge
from terminal_model import TerminalModel


class Session:
    """Observe complete firmware frames without inventing game state."""
    def __init__(self, qemu, sink=None, copier=lambda _: True):
        self.qemu = qemu
        self.sink = sink
        self.bridge = ClipboardBridge(copier)
        self.model = TerminalModel()
        self.copied = []

    def pump(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                data = self.qemu.serial.recv(16384)
                if not data: raise RuntimeError('UART closed')
                output, reply = self.bridge.feed(data)
                self.model.feed(output)
                if self.sink: self.sink(output)
                if reply: self.qemu.serial.sendall(reply)
            except socket.timeout:
                pass

    def wait(self, text, seconds=10, condition=lambda _: True):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            self.pump(0.08)
            if self.model.complete and text in self.model.text() and condition(self.model): return
        self.model.snapshot(ROOT / 'artifacts/screens', 'failure')
        raise AssertionError(f'Expected: {text}\n{self.model.text()}')

    def send(self, data, expected=None):
        self.qemu.serial.sendall(data)
        if expected: self.wait(expected)
        else: self.pump(0.15)

    def snapshot(self, name):
        self.model.snapshot(ROOT / 'artifacts/screens', name)


def player_left(model):
    """Find the real bottom paddle in the firmware frame, using local row 28."""
    return model.text().splitlines()[33].find('==========')
