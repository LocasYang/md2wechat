<div align="center">

# MD2WeChat

**Markdown → WeChat Official Account formatter**

Turn Markdown into typeset content you can paste straight into the WeChat
Official Account editor.
100% local · no third-party service · offline math rendering

[Features](#-features) · [How it works](#-how-it-works) · [Usage](#-usage) · [Build from source](#-build-from-source)

![Screenshot](docs/ui-dark.png)

</div>

---

## What is this

Most people write technical articles in Markdown, but the WeChat Official
Account editor understands neither Markdown nor standard HTML — it strips
`<style>` blocks, `class` attributes and external CSS, and **keeps only the
inline `style="..."` attributes**.

**MD2WeChat** converts Markdown into "WeChat-friendly HTML" locally:
every style is inlined, containers use `<section>`, and images are embedded.
Press one button — *Copy for WeChat* — then `Ctrl+V` in the WeChat editor.

Written in pure C++ / Qt. It **never calls a web API and never depends on a
third-party formatting service** — your content never touches anyone else's server.

## ✨ Features

**Full Markdown support**

Headings · paragraphs · bold · italic · bold-italic · strikethrough ·
inline code · links · images · block quotes (nested) · ordered / unordered
lists (nested) · task lists · tables (with alignment) · thematic breaks ·
fenced code blocks · reference links · raw HTML fragments

**Math formulas (rendered offline)**

- `$...$`, `$$...$$`, `\(...\)`, `\[...\]`
- Bundled MathJax 3, rendered to 3× supersampled PNG and inlined
- **No external service required — works offline**
- Inline formulas are baseline-aligned automatically

**Chinese typography conventions**

- A paragraph starting with `$\qquad$` / `$~$` / `&emsp;` becomes
  `text-indent:2em` (first-line indent)
- `::: hljs-center` / `hljs-right` / `hljs-left` containers are rendered as
  centered, small, light captions
- An `alt` that is just a file name (e.g. `![image.png](…)`) is not shown as a caption

**Code blocks**

- A hand-written syntax highlighter: C / C++ / Java / C# / JS / TS / Python /
  Go / Rust / PHP / Swift / Kotlin / SQL / JSON / Bash / YAML / HTML / XML…
- Optional horizontal scrolling (preserves line breaks) or soft wrapping
  (mobile friendly)

**Images never break**

- Local images are inlined as Base64
- Remote images (image hosts / your own OSS / MinIO) are downloaded and
  inlined too — so it does not matter whether WeChat can fetch them,
  or whether the remote server sends a proper `Content-Type`

**One-click copy**

- Standard `CF_HTML` plus a readable plain-text fallback
- Copy automatically waits for formula rendering and image downloads

**UI**

- Frameless window with a custom-drawn title bar, Ant Design look
  (aligned with the HuskarUI design tokens)
- 5 article themes: Classic Blue / WeChat Green / Minimal / Warm Orange / Night
- Light / dark UI toggle
- Two-way synchronized scrolling between editor and preview
- Adapts to window size and display scaling

## 🧩 How it works

### 1. Why every style is inline

The WeChat editor applies a "sanitizer" to pasted content. This tool simply
never emits the things that would be dropped:

| Dropped by WeChat | Kept by WeChat |
| --- | --- |
| `<style>` tags, `class` attributes | inline `style="..."` on each tag |
| external CSS, CSS variables | `<section>`, `<p>`, `<strong>` |
| `position: fixed`, pseudo elements | tables, images, lists |

Measured payload: `styleTag=0  classAttr=0` — not a single `<style>` or `class`.

### 2. A hand-written Markdown parser

`src/MarkdownParser.cpp`, no third-party dependencies.

It only does **structural recognition** and never rewrites content — the output
is a block-level AST, and inline parsing happens during rendering. That keeps
the source text byte-for-byte intact. It also supports container syntax
(`::: name`), task lists, reference links and math delimiters.

### 3. The math rendering pipeline

WeChat does not support MathJax / MathML, so **formulas must be images**:

```
$...$ / $$...$$
   → bundled MathJax 3 (tex-svg) produces SVG
   → Blob → canvas 3× supersample → PNG dataURI
   → cached, then rendered as <img>
```

Details worth knowing:

- MathJax SVG `viewBox` units are **1000 = 1em**; size in pixels is
  `viewBox width / 1000 × font size`, and the baseline drop is
  `-viewBox.minY / 1000 × font size`
- `fill="currentColor"` inside the SVG must be given an explicit color before
  serialization, otherwise it renders black
- `QWebEngineView::loadFinished` fires once for `about:blank` first — you must
  confirm the page is actually ready before starting the conversion
- Do not inline large JS into `setHtml()` (it breaks document parsing);
  write it to a temp file and load it via `file://`

### 4. Clipboard: hand-built CF_HTML

Qt builds the Windows `CF_HTML` blob for you, and its behaviour is not fully
controllable. This app builds it by hand — the four offsets
(`StartHTML` / `EndHTML` / `StartFragment` / `EndFragment`) are
**byte offsets from the start**, padded to 10 digits, computed and then patched
back in, and the blob is placed on the clipboard with
`SetClipboardData(RegisterClipboardFormat("HTML Format"))`.

The **plain-text fallback is also handled**: if the receiver falls back to
plain text (it does happen), you get the rendered, readable article
(`md::htmlToPlainText`) instead of the raw Markdown source.

### 5. Scroll synchronization

There is no built-in C++ signal for web page scrolling. `PreviewPage` overrides
`QWebEnginePage::javaScriptConsoleMessage`, and the preview page reports its
scroll ratio with `console.log("md2wx:scroll:0.42")` — fewer moving parts than
QWebChannel and much cheaper than polling.

Two-way sync uses a 220 ms time lock so the two panes do not fight each other.

### 6. Image inlining

Local images are read and converted to Base64; remote images are fetched with
`QNetworkAccessManager` and inlined too (4 MB per image cap, failures keep the
original URL). After inlining, WeChat **does not need to fetch any external
link at all** — wrong `Content-Type`, blocked hosts or rate limiting do not matter.

## 🚀 Usage

**Publish in three steps**

1. Click **Copy for WeChat** in the toolbar (it waits for formula rendering
   and image downloads automatically)
2. Click **WeChat MP Console** (opens mp.weixin.qq.com), sign in and create a
   new article
3. Put the caret in the **body area** and press `Ctrl+V`

**UI map**

| Area | Description |
| --- | --- |
| Left · Themes | 5 built-in article themes, click to switch |
| Left · Layout | Body font size, line height |
| Left · Options | syntax highlighting / math as image / code wrapping / scroll sync / image inlining … |
| Middle · Source | edit directly, or drop a `.md` file onto the window |
| Right · Preview | 677 px (the official article width), WYSIWYG |

**Markdown tips**

````markdown
$\qquad$A paragraph starting with `$\qquad$` gets a two-character indent.

![figure](https://example.com/a.png)
::: hljs-center
A caption wrapped in hljs-center is centred, small and light
:::

$$
|V_{grid}| > 30V
$$
````

## 🔨 Build from source

**Requirements**

| Component | Note |
| --- | --- |
| Qt | 5.15.x, **MSVC build** (not MinGW) |
| Compiler | MSVC v142 / v143 (VS 2019 / 2022 or Build Tools, C++ workload) |
| Windows SDK | 10.0.x |
| Qt modules | widgets · webenginewidgets · quickwidgets · pdf · svg · network |

**Option 1 (recommended)**: open `md2wechat.pro` in Qt Creator, pick an MSVC
kit and build.

**Option 2**: command line

```bat
git clone https://github.com/LocasYang/md2wechat.git
cd md2wechat
build.bat
```

`build.bat` auto-detects MSVC and the Windows SDK. If detection fails, set the
paths yourself:

```bat
set QTDIR=D:\Qt\5.15.2\msvc2019_64
build.bat
```

> ⚠️ You must use the **MSVC** flavour of Qt. MinGW Qt is not ABI-compatible
> with MSVC objects and linking will fail.

## ⚠️ Known limitations

- Inline formulas are inlined Base64 images; in rare cases WeChat reports
  "image paste failed" — retrying usually helps, and using a display formula
  for that spot is safer
- Non-inlined external images must be publicly reachable, and the remote server
  must send `Content-Type: image/*` (MinIO / OSS often default to
  `application/octet-stream` — fix it in the object metadata)
- Footnotes, definition lists and mermaid diagrams are not supported yet
- No image-host integration and no draft-box API yet

## 📁 Project layout

```
md2wechat/
├── md2wechat.pro            qmake project
├── build.bat                Windows / MSVC build script (auto-detects the toolchain)
├── run.bat                  launcher (sets up the Qt runtime path)
├── src/                     C++ sources
│   ├── MarkdownParser.*     hand-written Markdown parser
│   ├── WeChatRenderer.*     inline-style HTML renderer + math / image pipeline
│   ├── SyntaxHighlighter.*  code block highlighting
│   ├── StyleTheme.*         the 5 article themes
│   ├── AntTheme.*           Ant Design tokens + QSS
│   ├── AntWidgets.*         custom widgets (buttons / switches / theme cards / title bar)
│   ├── PreviewPage.*        preview page (scroll-sync bridge)
│   └── MainWindow.*         main window and layout
├── res/                     resources: icons / sample document / MathJax
├── tools/                   dev helper scripts (screenshots / clipboard checks / icon generation)
└── docs/                    screenshots
```

## 🙏 Acknowledgements

- [HuskarUI](https://github.com/mengps/HuskarUI_Qt5) — design tokens reference
- [MathJax](https://www.mathjax.org/) — formula rendering
- [Ant Design](https://ant.design/) — design language

## 📄 License

[MIT](LICENSE)

---

中文文档：[README.md](README.md)
