"""Find the MD2WeChat window and capture it to a PNG."""
import ctypes
import ctypes.wintypes as wt
import sys
import time

user32 = ctypes.windll.user32
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    user32.SetProcessDPIAware()

needle = sys.argv[1] if len(sys.argv) > 1 else "MD2WeChat"
out = sys.argv[2] if len(sys.argv) > 2 else r"F:\mine\code\qt\md2wechat\shot.png"

found = []
WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)


def cb(hwnd, lparam):
    if user32.IsWindowVisible(hwnd):
        n = user32.GetWindowTextLengthW(hwnd)
        if n:
            buf = ctypes.create_unicode_buffer(n + 1)
            user32.GetWindowTextW(hwnd, buf, n + 1)
            if needle in buf.value:
                found.append((hwnd, buf.value))
    return True


user32.EnumWindows(WNDENUMPROC(cb), 0)
if not found:
    print("WINDOW_NOT_FOUND")
    sys.exit(2)

hwnd, title = found[0]
print("TITLE:", title)

user32.ShowWindow(hwnd, 5)          # SW_RESTORE
user32.SetForegroundWindow(hwnd)
time.sleep(1.5)

rect = wt.RECT()
user32.GetWindowRect(hwnd, ctypes.byref(rect))
w = rect.right - rect.left
h = rect.bottom - rect.top
print("RECT:", rect.left, rect.top, w, h)

from PIL import ImageGrab
img = ImageGrab.grab(bbox=(rect.left, rect.top, rect.right, rect.bottom))
img.save(out)
print("SAVED:", out, img.size)
