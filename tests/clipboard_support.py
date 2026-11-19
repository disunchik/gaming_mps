"""Read back a real clipboard write while restoring all original clipboard formats."""
import ctypes as C
from ctypes import wintypes as W
from clipboard import copy_text


def verified_copy(text):
    """Duplicate all formats before testing, then restore the user's clipboard."""
    user = C.WinDLL('user32', use_last_error=True)
    kernel = C.WinDLL('kernel32', use_last_error=True)
    ole = C.WinDLL('ole32', use_last_error=True)
    user.GetClipboardData.argtypes = [W.UINT]; user.GetClipboardData.restype = W.HANDLE
    user.SetClipboardData.argtypes = [W.UINT, W.HANDLE]; user.SetClipboardData.restype = W.HANDLE
    user.OpenClipboard.argtypes = [W.HWND]
    user.CreateWindowExW.argtypes = [W.DWORD, W.LPCWSTR, W.LPCWSTR, W.DWORD, C.c_int, C.c_int,
        C.c_int, C.c_int, W.HWND, W.HMENU, W.HINSTANCE, C.c_void_p]
    user.CreateWindowExW.restype = W.HWND
    user.DestroyWindow.argtypes = [W.HWND]
    ole.OleDuplicateData.argtypes = [W.HANDLE, W.WORD, W.UINT]; ole.OleDuplicateData.restype = W.HANDLE
    kernel.GlobalLock.argtypes = [W.HANDLE]; kernel.GlobalLock.restype = C.c_void_p
    kernel.GlobalUnlock.argtypes = [W.HANDLE]
    saved = []
    window = user.CreateWindowExW(0, 'STATIC', 'clipboard test restore', 0, 0, 0, 0, 0, W.HWND(-3), None, None, None)
    assert window and user.OpenClipboard(window)
    try:
        fmt = user.EnumClipboardFormats(0)
        while fmt:
            source = user.GetClipboardData(fmt)
            assert source, ('cannot preserve clipboard format', fmt)
            duplicate = ole.OleDuplicateData(source, fmt, 2)
            assert duplicate, ('cannot duplicate clipboard format', fmt)
            saved.append((fmt, duplicate))
            fmt = user.EnumClipboardFormats(fmt)
    finally:
        user.CloseClipboard()
    try:
        assert copy_text(text), 'SetClipboardData failed'
        assert user.OpenClipboard(window)
        try:
            handle = user.GetClipboardData(13)
            pointer = kernel.GlobalLock(handle)
            assert pointer
            try: assert C.wstring_at(pointer) == text
            finally: kernel.GlobalUnlock(handle)
        finally:
            user.CloseClipboard()
        return True
    finally:
        assert user.OpenClipboard(window), 'Cannot restore clipboard'
        try:
            assert user.EmptyClipboard()
            for fmt, handle in saved:
                assert user.SetClipboardData(fmt, handle), ('cannot restore clipboard format', fmt)
        finally:
            user.CloseClipboard(); user.DestroyWindow(window)
