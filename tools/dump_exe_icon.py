"""把 Windows 认为的 exe 图标抠出来存成 PNG, 用来确认资源真的嵌进去了。"""
import ctypes
import ctypes.wintypes as wt
from PIL import Image

EXE = r"F:\mine\code\qt\md2wechat\bin\md2wechat.exe"
OUT = r"F:\mine\code\qt\md2wechat\exe_icon.png"

shell32 = ctypes.windll.shell32
user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32


class SHFILEINFOW(ctypes.Structure):
    _fields_ = [("hIcon", wt.HICON), ("iIcon", ctypes.c_int),
                ("dwAttributes", wt.DWORD),
                ("szDisplayName", wt.WCHAR * 260), ("szTypeName", wt.WCHAR * 80)]


SHGFI_ICON = 0x100
SHGFI_LARGEICON = 0x0

info = SHFILEINFOW()
r = shell32.SHGetFileInfoW(EXE, 0, ctypes.byref(info), ctypes.sizeof(info),
                           SHGFI_ICON | SHGFI_LARGEICON)
print("SHGetFileInfo ->", r, "hIcon:", info.hIcon)
if not info.hIcon:
    raise SystemExit("no icon")


class ICONINFO(ctypes.Structure):
    _fields_ = [("fIcon", wt.BOOL), ("xHotspot", wt.DWORD), ("yHotspot", wt.DWORD),
                ("hbmMask", wt.HBITMAP), ("hbmColor", wt.HBITMAP)]


ii = ICONINFO()
user32.GetIconInfo(info.hIcon, ctypes.byref(ii))


class BITMAP(ctypes.Structure):
    _fields_ = [("bmType", wt.LONG), ("bmWidth", wt.LONG), ("bmHeight", wt.LONG),
                ("bmWidthBytes", wt.LONG), ("bmPlanes", wt.WORD), ("bmBitsPixel", wt.WORD),
                ("bmBits", ctypes.c_void_p)]


bm = BITMAP()
gdi32.GetObjectW(ii.hbmColor, ctypes.sizeof(bm), ctypes.byref(bm))
w, h = bm.bmWidth, bm.bmHeight
print("bitmap:", w, "x", h)


class BMIH(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG),
                ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", wt.LONG),
                ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.DWORD),
                ("biClrImportant", wt.DWORD)]


class BMI(ctypes.Structure):
    _fields_ = [("bmiHeader", BMIH), ("bmiColors", wt.DWORD * 3)]


bmi = BMI()
bmi.bmiHeader.biSize = ctypes.sizeof(BMIH)
bmi.bmiHeader.biWidth = w
bmi.bmiHeader.biHeight = -h
bmi.bmiHeader.biPlanes = 1
bmi.bmiHeader.biBitCount = 32
bmi.bmiHeader.biCompression = 0

hdc = user32.GetDC(0)
buf = ctypes.create_string_buffer(w * h * 4)
n = gdi32.GetDIBits(hdc, ii.hbmColor, 0, h, buf, ctypes.byref(bmi), 0)
user32.ReleaseDC(0, hdc)
print("GetDIBits scanlines:", n)

img = Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA", 0, 1)
img.save(OUT)
print("saved", OUT, img.size)
