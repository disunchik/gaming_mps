"""Bounded OSC52 transport. The PC copies text; the ESP32 owns the game and code."""
import base64
import binascii
import ctypes as C
import os
import re
import time


def copy_text(text):
    """Write CF_UNICODETEXT to the native Windows clipboard; return actual success."""
    if os.name != 'nt':
        return False
    from ctypes import wintypes as W
    user = C.WinDLL('user32', use_last_error=True)
    kernel = C.WinDLL('kernel32', use_last_error=True)
    user.CreateWindowExW.argtypes = [W.DWORD, W.LPCWSTR, W.LPCWSTR, W.DWORD, C.c_int, C.c_int,
        C.c_int, C.c_int, W.HWND, W.HMENU, W.HINSTANCE, C.c_void_p]
    user.CreateWindowExW.restype = W.HWND
    user.OpenClipboard.argtypes = [W.HWND]
    user.SetClipboardData.argtypes = [W.UINT, W.HANDLE]
    user.SetClipboardData.restype = W.HANDLE
    user.DestroyWindow.argtypes = [W.HWND]
    kernel.GlobalAlloc.argtypes = [W.UINT, C.c_size_t]; kernel.GlobalAlloc.restype = W.HGLOBAL
    kernel.GlobalLock.argtypes = [W.HGLOBAL]; kernel.GlobalLock.restype = C.c_void_p
    kernel.GlobalUnlock.argtypes = [W.HGLOBAL]
    kernel.GlobalFree.argtypes = [W.HGLOBAL]; kernel.GlobalFree.restype = W.HGLOBAL
    # A message-only window owns the clipboard even under ConPTY/VS Code.
    window = user.CreateWindowExW(0, 'STATIC', 'gaming_mps clipboard', 0, 0, 0, 0, 0,
                                  W.HWND(-3), None, None, None)
    if not window:
        return False
    memory = None
    opened = False
    try:
        deadline = time.monotonic() + 0.25
        while not user.OpenClipboard(window):
            if time.monotonic() >= deadline: return False
            time.sleep(0.01)
        opened = True
        data = text.encode('utf-16-le') + b'\0\0'
        memory = kernel.GlobalAlloc(0x2, len(data))
        if not memory: return False
        pointer = kernel.GlobalLock(memory)
        if not pointer: return False
        C.memmove(pointer, data, len(data))
        kernel.GlobalUnlock(memory)
        if not user.EmptyClipboard(): return False
        if not user.SetClipboardData(13, memory): return False
        memory = None  # Windows now owns the memory.
        return True
    finally:
        if memory: kernel.GlobalFree(memory)
        if opened: user.CloseClipboard()
        user.DestroyWindow(window)


class ClipboardBridge:
    """Strip OSC52 from arbitrary UART chunks and return a success/failure response."""
    prefix = b'\x1b]52;c;'

    def __init__(self, copier=copy_text):
        self.pending = bytearray()
        self.copier = copier
        self.discarding = False

    def feed(self, data):
        """Return normal terminal bytes and optional firmware acknowledgements."""
        self.pending.extend(data)
        output = bytearray()
        replies = bytearray()
        while self.pending:
            if self.discarding:
                end = self.pending.find(b'\x07')
                if end < 0: self.pending.clear(); break
                del self.pending[:end + 1]; self.discarding = False
                continue
            position = self.pending.find(self.prefix)
            if position < 0:
                keep = 0
                for count in range(1, min(len(self.pending), len(self.prefix) - 1) + 1):
                    if self.pending[-count:] == self.prefix[:count]: keep = count
                stop = len(self.pending) - keep
                output.extend(self.pending[:stop]); del self.pending[:stop]
                break
            output.extend(self.pending[:position]); del self.pending[:position]
            end = self.pending.find(b'\x07', len(self.prefix))
            if end < 0:
                if len(self.pending) <= 128: break
                self.pending.clear(); self.discarding = True
                replies.extend(b'\x1b[902~')
                continue
            if end > 128:
                del self.pending[:end + 1]; replies.extend(b'\x1b[902~'); continue
            encoded = bytes(self.pending[len(self.prefix):end])
            del self.pending[:end + 1]
            success = False
            try:
                text = base64.b64decode(encoded, validate=True).decode('ascii')
                if re.fullmatch(r'P2-[0-9A-Z]{4}-[0-9A-Z]{4}-[0-9A-Z]{5}', text):
                    success = bool(self.copier(text))
            except (ValueError, UnicodeError, binascii.Error, OSError):
                pass
            replies.extend(b'\x1b[901~' if success else b'\x1b[902~')
        return bytes(output), bytes(replies)
