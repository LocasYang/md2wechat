"""Capture a specific window's own content via PrintWindow (works when the desktop is locked)."""
import ctypes
import ctypes.wintypes as wt
import sys
from PIL import Image

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    user32.SetProcessDPIAware()

needle = sys.argv[1] if len(sys.argv) > 1 else "MD2WeChat"
out = sys.argv[2] if len(sys.argv) > 2 else r"F:\mine\code\qt\md2wechat\shot.png"
grab_child = len(sys.argv) > 3 and sys.argv[3] == "child"

found = []
EnumProc = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)


def enum_cb(hwnd, lparam):
    n = user32.GetWindowTextLengthW(hwnd)
    if n:
        buf = ctypes.create_unicode_buffer(n + 1)
        user32.GetWindowTextW(hwnd, buf, n + 1)
        if needle in buf.value:
            found.append((hwnd, buf.value, user32.IsWindowVisible(hwnd)))
    return True


user32.EnumWindows(EnumProc(enum_cb), 0)
if not found:
    print("WINDOW_NOT_FOUND")
    sys.exit(2)

# 优先选可见窗口
found.sort(key=lambda x: (not x[2],))
hwnd, title, vis = found[0]
print("TITLE:", title, "visible:", vis)


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG),
                ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", wt.LONG),
                ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.DWORD),
                ("biClrImportant", wt.DWORD)]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wt.DWORD * 3)]


def capture(h):
    rect = wt.RECT()
    user32.GetWindowRect(h, ctypes.byref(rect))
    w = rect.right - rect.left
    hh = rect.bottom - rect.top
    if w <= 0 or hh <= 0:
        return None
    hdc = user32.GetWindowDC(h)
    mdc = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, hh)
    gdi32.SelectObject(mdc, bmp)

    ok = user32.PrintWindow(h, mdc, 2)  # PW_RENDERFULLCONTENT
    if not ok:
        ok = user32.PrintWindow(h, mdc, 0)

    bmi = BITMAPINFO()
    bmi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.bmiHeader.biWidth = w
    bmi.bmiHeader.biHeight = -hh          # top-down
    bmi.bmiHeader.biPlanes = 1
    bmi.bmiHeader.biBitCount = 32
    bmi.bmiHeader.biCompression = 0

    buf = ctypes.create_string_buffer(w * hh * 4)
    got = gdi32.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bmi), 0)

    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mdc)
    user32.ReleaseDC(h, hdc)

    if not got:
        return None
    img = Image.frombuffer("RGB", (w, hh), buf, "raw", "BGRX", 0, 1)
    return img


img = capture(hwnd)
if img is None:
    print("CAPTURE_FAILED")
    sys.exit(3)

# 统计非黑像素比例, 判断是否抓到了内容
extrema = img.convert("L").getextrema()
ratio = sum(img.convert("L").point(lambda v: 255 if v > 12 else 0).getdata()) / (255.0 * img.width * img.height)
print("CONTENT_RATIO: %.4f" % ratio, "size:", img.size)

img.save(out)
print("SAVED:", out)
