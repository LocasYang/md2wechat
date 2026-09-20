"""读出剪贴板里的 HTML Format(CF_HTML), 校验偏移并预览内容。

注意: ctypes 默认把返回值当 int(32 位), 在 64 位下会把 HANDLE 截断,
必须显式声明 argtypes/restype, 否则读出来永远是空的。
"""
import ctypes
import ctypes.wintypes as wt
import re

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

user32.RegisterClipboardFormatW.argtypes = [wt.LPCWSTR]
user32.RegisterClipboardFormatW.restype = wt.UINT
user32.OpenClipboard.argtypes = [wt.HWND]
user32.OpenClipboard.restype = wt.BOOL
user32.CloseClipboard.restype = wt.BOOL
user32.GetClipboardData.argtypes = [wt.UINT]
user32.GetClipboardData.restype = wt.HANDLE

kernel32.GlobalLock.argtypes = [wt.HGLOBAL]
kernel32.GlobalLock.restype = ctypes.c_void_p
kernel32.GlobalSize.argtypes = [wt.HGLOBAL]
kernel32.GlobalSize.restype = ctypes.c_size_t
kernel32.GlobalUnlock.argtypes = [wt.HGLOBAL]
kernel32.GlobalUnlock.restype = wt.BOOL

CF_UNICODETEXT = 13
FMT_HTML = user32.RegisterClipboardFormatW("HTML Format")


def read(fmt):
    h = user32.GetClipboardData(fmt)
    if not h:
        return None
    p = kernel32.GlobalLock(h)
    if not p:
        return None
    try:
        size = kernel32.GlobalSize(h)
        return ctypes.string_at(p, size)
    finally:
        kernel32.GlobalUnlock(h)


if not user32.OpenClipboard(None):
    raise SystemExit("打开剪贴板失败: %d" % ctypes.get_last_error())
raw = read(FMT_HTML)
txt = read(CF_UNICODETEXT)
user32.CloseClipboard()

print("=" * 62)
print("剪贴板检查 (HTML Format id = %d)" % FMT_HTML)
print("=" * 62)

if raw is None:
    print("[X] 没有 HTML Format —— 接收方只能拿到纯文本")
else:
    print("[OK] HTML Format 共 %d 字节" % len(raw))

    j = raw.find(b"<html")
    head = raw[:j if j > 0 else 200].decode("ascii", "replace")
    print("\n--- 头部 ---")
    for line in head.splitlines():
        if line.strip():
            print("   ", line)

    vals = {}
    for key in (b"StartHTML:", b"EndHTML:", b"StartFragment:", b"EndFragment:"):
        k = raw.find(key)
        vals[key.decode().rstrip(":")] = int(raw[k + len(key):k + len(key) + 10]) if k >= 0 else -1

    sh, eh = vals["StartHTML"], vals["EndHTML"]
    sf, ef = vals["StartFragment"], vals["EndFragment"]
    print("\n--- 偏移校验 ---")
    print("   [%s] 偏移顺序" % ("OK" if 0 <= sh < sf < ef <= eh <= len(raw) else "X"), vals)
    print("   [%s] StartHTML 指向 %r" % ("OK" if raw[sh:sh+5].lower() == b"<html" else "X", raw[sh:sh+5]))

    frag = raw[sf:ef].decode("utf-8", "replace")
    print("   [OK] 正文片段 %d 字符" % len(frag))

    print("\n--- 正文开头 180 字 ---")
    print("   ", frag[:180].replace("\n", " "))
    print("\n--- 检查 ---")
    print("    出现 Markdown 记号:", "无" if not any(m in frag for m in ("![](", "](", "## ")) else "有!")
    print("    <style> 数量:", frag.count("<style"))
    print("    class= 数量 :", frag.count("class="))
    print("    <img> 数量  :", frag.count("<img"))
    print("    含 section :", "是" if frag.lstrip().startswith("<section") else "否")

print("\n--- 纯文本兜底 ---")
if txt is None:
    print("    [X] 没有 CF_UNICODETEXT")
else:
    t = txt.decode("utf-16-le", "replace").rstrip("\x00")
    print("    共 %d 字符" % len(t))
    print("   ", t[:160].replace("\n", " / "))
    print("    含 Markdown 记号:",
          "无" if not any(m in t for m in ("![", "](", "## ")) else "有!")
