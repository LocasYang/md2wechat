"""整理产物, 清理开发过程的临时文件。"""
import os
import shutil
from PIL import Image

BASE = r"F:\mine\code\qt\md2wechat"
docs = os.path.join(BASE, "docs")
TMP = os.environ.get("TEMP", r"C:\Users\yangfan\AppData\Local\Temp")
os.makedirs(docs, exist_ok=True)


def crop(src, dst, h):
    p = os.path.join(BASE, src)
    if os.path.exists(p):
        im = Image.open(p)
        im.crop((0, 0, im.width, min(h, im.height))).save(os.path.join(docs, dst))
        os.remove(p)
        print("kept docs/" + dst)


crop("cp.png-article.png", "article-classic.png", 8800)

# 「复制到公众号」的真实载荷
p = os.path.join(BASE, "cp.png.wechat.html")
if os.path.exists(p):
    wrap = (
        '<!DOCTYPE html><html><head><meta charset="utf-8">'
        '<meta name="viewport" content="width=device-width,initial-scale=1">'
        '<style>body{margin:0;padding:24px;background:#f2f3f5}'
        '#wrap{width:677px;max-width:100%;margin:0 auto;background:#fff;border-radius:10px;'
        'padding:34px 24px;box-sizing:border-box;box-shadow:0 6px 26px rgba(0,0,0,.10)}</style>'
        '</head><body><section id="wrap">' + open(p, encoding="utf-8").read() + "</section></body></html>"
    )
    open(os.path.join(docs, "copy-payload.html"), "w", encoding="utf-8").write(wrap)
    os.remove(p)
    print("kept docs/copy-payload.html")

temps = []
for stem in ["cp.png"]:
    for suffix in ["", ".log", ".html", ".pdf", "-article.png", ".preview.html", ".wechat.html"]:
        temps.append(os.path.join(BASE, stem + suffix))
temps += [os.path.join(BASE, "build.log"), os.path.join(TMP, "md2wx-copytest.log")]

for t in temps:
    if os.path.exists(t):
        os.remove(t)
        print("removed", os.path.basename(t))

print("--- docs ---")
for n in sorted(os.listdir(docs)):
    print("   ", n)
