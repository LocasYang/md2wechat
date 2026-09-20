#include "WeChatRenderer.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include "SyntaxHighlighter.h"

namespace md {

// ============================================================
//  公式 key:  "D|#1a1a1a|tex"   (D=块级, I=行内)
// ============================================================

QString mathKey(const QString &tex, bool display, const QString &color)
{
    return (display ? QStringLiteral("D|") : QStringLiteral("I|")) + color
           + QLatin1Char('|') + tex;
}

static void splitMathKey(const QString &key, QString &color, QString &tex)
{
    const int i = key.indexOf(QLatin1Char('|'));
    const int j = key.indexOf(QLatin1Char('|'), i + 1);
    if (i < 0 || j < 0) {
        color.clear();
        tex.clear();
        return;
    }
    color = key.mid(i + 1, j - i - 1);
    tex   = key.mid(j + 1);
}

bool mathDisplayFromKey(const QString &key)
{
    return key.startsWith(QStringLiteral("D|"));
}

QString mathColorFromKey(const QString &key)
{
    QString c, t;
    splitMathKey(key, c, t);
    return c;
}

QString mathTexFromKey(const QString &key)
{
    QString c, t;
    splitMathKey(key, c, t);
    return t;
}

namespace {

// ============================================================
//  小工具
// ============================================================

inline QString openTag(const QString &name, const QString &style)
{
    return QLatin1Char('<') + name + QStringLiteral(" style=\"") + style + QStringLiteral("\">");
}
inline QString closeTag(const QString &name)
{
    return QStringLiteral("</") + name + QLatin1Char('>');
}

// 链接需要 href 属性, 不能直接用 openTag(那样会多出一个 '>')
inline QString anchorOpen(const QString &href, const QString &style)
{
    return QStringLiteral("<a style=\"") + style + QStringLiteral("\" href=\"")
           + escapeAttr(href) + QStringLiteral("\">");
}

QString num(double v, int precision = -1)
{
    QString s = QString::number(v, 'f', precision < 0 ? 2 : precision);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    return s;
}

bool startsWithAt(const QString &s, int i, const QString &needle)
{
    return s.midRef(i, needle.size()) == needle;
}

// 去掉 HTML 中的危险标签
QString stripDangerous(const QString &html)
{
    static const QRegularExpression re(
        QStringLiteral("<\\s*(script|style|iframe|object|embed|link|meta|form|input)[^>]*>.*?<\\s*/\\s*\\1\\s*>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression re2(
        QStringLiteral("<\\s*/?\\s*(script|style|iframe|object|embed|link|meta|form|input)[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    QString s = html;
    s.remove(re);
    s.remove(re2);
    return s;
}

// ============================================================
//  内联样式片段
// ============================================================

struct Ctx
{
    const StyleTheme  *t   = nullptr;
    RenderOptions      opt;
    QHash<QString, QString> refs;      // 引用式链接: id -> url
    int   listDepth  = 0;
    int   paraMargin = 16;             // 段落底部间距(px)
    int   images     = 0;
    // 容器(::: hljs-center 之类)里的文字按图注处理: 0=普通 1=居中 2=右 3=左
    int   captionMode = 0;
};

QString rootFont(const Ctx &c)
{
    return c.t->fontFamily;
}

// ---- 段落 ----
QString styleP(const Ctx &c)
{
    return QStringLiteral("margin:0 0 ") + QString::number(c.paraMargin)
           + QStringLiteral("px;font-size:") + QString::number(c.opt.fontSize)
           + QStringLiteral("px;line-height:") + num(c.opt.lineHeight)
           + QStringLiteral(";color:") + c.t->text
           + QStringLiteral(";letter-spacing:.5px;word-break:break-word");
}

// ---- 图注段落(容器 ::: hljs-center 里的文字) ----
QString styleCaptionP(const Ctx &c)
{
    return QStringLiteral("margin:0 0 ") + QString::number(qMax(8, c.paraMargin - 4))
           + QStringLiteral("px;font-size:") + QString::number(qMax(11, c.opt.fontSize - 2))
           + QStringLiteral("px;line-height:1.7;color:") + c.t->textLight
           + QStringLiteral(";letter-spacing:.5px;word-break:break-word");
}

// 中文排版习惯: 段落以 $\qquad$ / $~$ / &emsp; 起头表示首行缩进
bool stripLeadingIndent(QString &text)
{
    static const QStringList marks = {
        QStringLiteral("$\\qquad$"), QStringLiteral("$\\quad$"), QStringLiteral("$~$"),
        QStringLiteral("&emsp;&emsp;"), QStringLiteral("&emsp;"),
        QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;"), QStringLiteral("&nbsp;&nbsp;")
    };
    const QString t = text.trimmed();
    for (const QString &m : marks) {
        if (t.startsWith(m)) {
            QString rest = t.mid(m.size());
            while (!rest.isEmpty()
                   && (rest.at(0) == QLatin1Char(' ') || rest.at(0) == QChar(0x3000)))
                rest.remove(0, 1);
            text = rest;
            return true;
        }
    }
    return false;
}

// 正文中间出现的 \qquad / \quad 直接当空白处理, 别渲染成一张空白图片
QString normalizeLatexSpacers(const QString &s)
{
    if (!s.contains(QLatin1Char('$')))
        return s;
    QString t = s;
    t.replace(QStringLiteral("$\\qquad$"), QStringLiteral("&emsp;&emsp;"));
    t.replace(QStringLiteral("$\\quad$"),  QStringLiteral("&emsp;"));
    t.replace(QStringLiteral("$\\;$"),     QStringLiteral("&thinsp;"));
    return t;
}

// alt 是文件名(如 image.png)时不当作图注显示
bool looksLikeFileName(const QString &s)
{
    static const QRegularExpression re(
        QStringLiteral("\\.(png|jpe?g|gif|webp|bmp|svg|tiff?|heic|avif)$"),
        QRegularExpression::CaseInsensitiveOption);
    return re.match(s.trimmed()).hasMatch();
}

// ---- 标题 ----
QString styleHeading(const Ctx &c, int level)
{
    const StyleTheme &t = *c.t;
    const int base = c.opt.fontSize;
    const QString common = QStringLiteral("line-height:1.45;letter-spacing:.5px;word-break:break-word;font-weight:600");

    if (level == 1) {
        // 大标题: 居中 + 主题色
        return QStringLiteral("margin:30px 0 22px;padding-bottom:10px;border-bottom:2px solid ")
               + t.accent + QStringLiteral(";text-align:center;font-size:") + QString::number(base + 7)
               + QStringLiteral("px;color:") + t.accentDark + QLatin1Char(';') + common;
    }
    if (level == 2) {
        switch (t.h2Style) {
        case 1: // 底色块
            return QStringLiteral("margin:34px 0 16px;padding:8px 14px;background:")
                   + t.accentSoft + QStringLiteral(";border-radius:4px;font-size:")
                   + QString::number(base + 4) + QStringLiteral("px;color:") + t.accentDark
                   + QLatin1Char(';') + common;
        case 2: // 居中下划线
            return QStringLiteral("margin:36px 0 18px;padding-bottom:8px;border-bottom:1px solid ")
                   + t.border + QStringLiteral(";text-align:center;font-size:")
                   + QString::number(base + 4) + QStringLiteral("px;color:") + t.textStrong
                   + QLatin1Char(';') + common;
        case 3: // 左条 + 浅底
            return QStringLiteral("margin:34px 0 16px;padding:7px 14px;border-left:4px solid ")
                   + t.accent + QStringLiteral(";background:") + t.accentSoft
                   + QStringLiteral(";border-radius:0 4px 4px 0;font-size:")
                   + QString::number(base + 4) + QStringLiteral("px;color:") + t.accentDark
                   + QLatin1Char(';') + common;
        case 0:
        default: // 左侧色条
            return QStringLiteral("margin:34px 0 16px;padding-left:12px;border-left:4px solid ")
                   + t.accent + QStringLiteral(";font-size:") + QString::number(base + 4)
                   + QStringLiteral("px;color:") + t.textStrong + QLatin1Char(';') + common;
        }
    }
    if (level == 3) {
        if (t.h3Style == 3)
            return QStringLiteral("margin:26px 0 12px;padding:5px 12px;border-left:3px solid ")
                   + t.accent + QStringLiteral(";background:") + t.accentSoft
                   + QStringLiteral(";border-radius:0 4px 4px 0;font-size:") + QString::number(base + 2)
                   + QStringLiteral("px;color:") + t.textStrong + QLatin1Char(';') + common;
        if (t.h3Style == 2)
            return QStringLiteral("margin:26px 0 12px;padding-bottom:6px;border-bottom:1px dashed ")
                   + t.border + QStringLiteral(";font-size:") + QString::number(base + 2)
                   + QStringLiteral("px;color:") + t.textStrong + QLatin1Char(';') + common;
        return QStringLiteral("margin:26px 0 12px;padding-left:10px;border-left:3px solid ")
               + t.accent + QStringLiteral(";font-size:") + QString::number(base + 2)
               + QStringLiteral("px;color:") + t.textStrong + QLatin1Char(';') + common;
    }
    // 4 / 5 / 6
    return QStringLiteral("margin:22px 0 10px;padding-left:9px;border-left:3px solid ")
           + t.border + QStringLiteral(";font-size:") + QString::number(base + 1)
           + QStringLiteral("px;color:") + t.textStrong + QLatin1Char(';') + common;
}

// ---- 行内代码 ----
QString styleInlineCode(const Ctx &c)
{
    const QString bg = c.t->codeDark ? QStringLiteral("#f1f2f4") : QStringLiteral("#eef1f4");
    Q_UNUSED(bg)
    return QStringLiteral("background:") + c.t->bgSoft
           + QStringLiteral(";color:") + c.t->accentDark
           + QStringLiteral(";padding:2px 6px;border-radius:4px;font-family:")
           + QStringLiteral("Menlo,Consolas,'Courier New',monospace;font-size:")
           + QString::number(c.opt.fontSize - 1) + QStringLiteral("px;word-break:break-all");
}

// ---- 强调 ----
QString styleStrong(const Ctx &c)
{
    return QStringLiteral("font-weight:600;color:") + c.t->accentDark;
}
QString styleEm(const Ctx &c)
{
    return QStringLiteral("font-style:italic;color:") + c.t->text;
}
QString styleDel(const Ctx &c)
{
    return QStringLiteral("text-decoration:line-through;color:") + c.t->textLight;
}
QString styleLink(const Ctx &c)
{
    return QStringLiteral("color:") + c.t->link
           + QStringLiteral(";text-decoration:none;border-bottom:1px solid ") + c.t->link
           + QStringLiteral(";font-weight:500");
}

// ---- 公式 ----
QString mathColorOf(const Ctx &c)
{
    return c.t->textStrong;
}

// 公式还没转好(或转换失败)时的占位显示: 直接把 TeX 原文显示出来, 保证内容不丢
QString mathPlaceholder(const QString &tex, bool display, const Ctx &c)
{
    const QString style =
        QStringLiteral("background:") + c.t->bgSoft
        + QStringLiteral(";color:") + c.t->textLight
        + QStringLiteral(";padding:1px 6px;border-radius:4px;border:1px dashed ") + c.t->border
        + QStringLiteral(";font-family:Menlo,Consolas,'Courier New',monospace;font-size:")
        + QString::number(c.opt.fontSize - 2) + QStringLiteral("px;word-break:break-all");
    return QStringLiteral("<span data-md2wx-math=\"1\" data-display=\"")
           + (display ? QStringLiteral("1") : QStringLiteral("0"))
           + QStringLiteral("\" data-color=\"") + c.t->textStrong
           + QStringLiteral("\" data-tex=\"") + escapeAttr(tex)
           + QStringLiteral("\" style=\"") + style + QStringLiteral("\">")
           + escapeHtml(tex) + QStringLiteral("</span>");
}

QString mathHtml(const QString &tex, bool display, Ctx &c)
{
    if (c.opt.mathToImage && c.opt.math) {
        const QString key = mathKey(tex, display, mathColorOf(c));
        if (const MathImage *mi = c.opt.math->find(key)) {
            return QStringLiteral("<img src=\"") + mi->dataUri
                   + QStringLiteral("\" width=\"") + num(mi->w, 1)
                   + QStringLiteral("\" height=\"") + num(mi->h, 1)
                   + QStringLiteral("\" alt=\"") + escapeAttr(tex)
                   + QStringLiteral("\" style=\"max-width:100%;height:auto;")
                   + (display ? QStringLiteral("display:inline-block;vertical-align:middle")
                              : QStringLiteral("vertical-align:") + num(-mi->depth, 2)
                                    + QStringLiteral("px"))
                   + QStringLiteral("\"/>");
        }
        c.opt.math->require(key);
    }
    return mathPlaceholder(tex, display, c);
}

// ============================================================
//  图片
// ============================================================

QString mimeOf(const QString &path)
{
    QString p = path;
    const int q = p.indexOf(QLatin1Char('?'));
    if (q > 0)
        p = p.left(q);                       // 去掉 URL 上的查询串
    const QString e = QFileInfo(p).suffix().toLower();
    if (e == QLatin1String("png"))  return QStringLiteral("image/png");
    if (e == QLatin1String("jpg") || e == QLatin1String("jpeg")) return QStringLiteral("image/jpeg");
    if (e == QLatin1String("gif"))  return QStringLiteral("image/gif");
    if (e == QLatin1String("webp")) return QStringLiteral("image/webp");
    if (e == QLatin1String("bmp"))  return QStringLiteral("image/bmp");
    if (e == QLatin1String("svg"))  return QStringLiteral("image/svg+xml");
    return QStringLiteral("image/png");
}

// 远程图片: 若界面已预下载好, 就内联成 base64, 免得微信去抓外链
QString remoteImageToDataUri(const QString &url, const RenderOptions &opt)
{
    if (!opt.inlineRemoteImages || !opt.remoteCache)
        return QString();
    const auto it = opt.remoteCache->constFind(url);
    if (it == opt.remoteCache->constEnd() || it.value().isEmpty())
        return QString();
    return QStringLiteral("data:") + mimeOf(url) + QStringLiteral(";base64,")
           + QString::fromLatin1(it.value().toBase64());
}

// 相对路径 -> 绝对路径 -> base64 data URI
QString localImageToDataUri(const QString &src, const QString &basePath)
{
    if (src.startsWith(QStringLiteral("http://")) || src.startsWith(QStringLiteral("https://"))
        || src.startsWith(QStringLiteral("data:")))
        return QString();

    QString path = src;
    // 去掉可能的 file:/// 前缀
    if (path.startsWith(QStringLiteral("file:///")))
        path = path.mid(8);
    // 支持 Qt 资源路径 (内置示例图):  qrc:/examples/x.png  ->  :/examples/x.png
    if (path.startsWith(QStringLiteral("qrc:")))
        path = path.mid(3);
    path = QDir::fromNativeSeparators(path);

    QString abs;
    if (path.startsWith(QLatin1Char(':')))
        abs = path;                       // :/examples/demo.png
    else if (QFileInfo(path).isAbsolute())
        abs = path;
    else if (!basePath.isEmpty())
        abs = QDir(basePath).absoluteFilePath(path);
    else
        abs = QFileInfo(path).absoluteFilePath();

    QFile f(abs);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return QString();

    const QByteArray data = f.readAll();
    f.close();
    if (data.isEmpty() || data.size() > 8 * 1024 * 1024)   // 单图上限 8MB
        return QString();

    return QStringLiteral("data:") + mimeOf(abs) + QStringLiteral(";base64,")
           + QString::fromLatin1(data.toBase64());
}

QString imageHtml(const QString &srcIn, const QString &alt, const QString &title, Ctx &c)
{
    QString src = srcIn;
    if (c.opt.inlineRemoteImages) {
        const QString data = remoteImageToDataUri(srcIn, c.opt);
        if (!data.isEmpty())
            src = data;
    }
    if (src == srcIn && c.opt.inlineLocalImages) {
        const QString data = localImageToDataUri(srcIn, c.opt.basePath);
        if (!data.isEmpty())
            src = data;
    }
    ++c.images;

    QString style = QStringLiteral("max-width:100%;height:auto;display:block;margin:14px auto;border-radius:")
                    + c.t->imgRadius;
    QString html = QStringLiteral("<img src=\"") + escapeAttr(src) + QStringLiteral("\" style=\"")
                   + style + QStringLiteral("\"");
    if (!alt.isEmpty())
        html += QStringLiteral(" alt=\"") + escapeAttr(alt) + QStringLiteral("\"");
    if (!title.isEmpty())
        html += QStringLiteral(" title=\"") + escapeAttr(title) + QStringLiteral("\"");
    html += QStringLiteral("/>");

    // alt 作为图注展示(像 image.png 这种文件名形式的跳过, 免得正文冒出一句文件名)
    if (c.opt.showImageAlt && !alt.isEmpty() && alt.size() <= 60 && !looksLikeFileName(alt)) {
        html += QStringLiteral("<span style=\"display:block;text-align:center;font-size:12px;color:")
                + c.t->imgCaptionColor + QStringLiteral(";margin:-6px 0 18px;letter-spacing:.5px\">")
                + escapeHtml(alt) + QStringLiteral("</span>");
    }
    return html;
}

// ============================================================
//  行内解析
// ============================================================

QString renderInline(const QString &s, Ctx &c, int depth);

int findClosingBracket(const QString &s, int openIdx)
{
    int depth = 0;
    for (int i = openIdx; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        if (ch == QLatin1Char('\\')) {
            ++i;
            continue;
        }
        if (ch == QLatin1Char('[')) {
            ++depth;
        } else if (ch == QLatin1Char(']')) {
            --depth;
            if (depth == 0)
                return i;
        }
    }
    return -1;
}

int findClosingParen(const QString &s, int openIdx)
{
    int depth = 0;
    for (int i = openIdx; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        if (ch == QLatin1Char('\\')) {
            ++i;
            continue;
        }
        if (ch == QLatin1Char('(')) {
            ++depth;
        } else if (ch == QLatin1Char(')')) {
            --depth;
            if (depth == 0)
                return i;
        }
    }
    return -1;
}

void parseLinkDest(const QString &inner, QString &url, QString &title)
{
    QString t = inner.trimmed();
    if (t.isEmpty())
        return;
    if (t.startsWith(QLatin1Char('<'))) {
        const int e = t.indexOf(QLatin1Char('>'));
        if (e > 0) {
            url = t.mid(1, e - 1);
            t   = t.mid(e + 1).trimmed();
        }
    } else {
        int i = 0;
        while (i < t.size() && !t.at(i).isSpace())
            ++i;
        url = t.left(i);
        t   = t.mid(i).trimmed();
    }
    if (t.size() >= 2) {
        const QChar q = t.at(0);
        if (q == QLatin1Char('"') || q == QLatin1Char('\'') || q == QLatin1Char('(')) {
            const QChar endq = (q == QLatin1Char('(')) ? QLatin1Char(')') : q;
            const int e = t.indexOf(endq, 1);
            if (e > 0)
                title = t.mid(1, e - 1);
        }
    }
}

bool looksLikeUrl(const QString &s)
{
    return s.startsWith(QStringLiteral("http://")) || s.startsWith(QStringLiteral("https://"))
           || s.startsWith(QStringLiteral("mailto:")) || s.startsWith(QStringLiteral("tel:"))
           || s.startsWith(QStringLiteral("ftp://"));
}

bool isIdentChar(QChar ch)
{
    return ch.isLetterOrNumber() || ch == QLatin1Char('_');
}

bool isAutoLinkTail(QChar ch)
{
    return ch.isSpace() || ch == QLatin1Char('<') || ch == QLatin1Char('"')
           || ch == QLatin1Char('\'') || ch == QChar(0x3002) /*。*/;
}

QString renderInline(const QString &s, Ctx &c, int depth)
{
    if (depth > 12)
        return escapeHtml(s);

    QString out;
    out.reserve(s.size() + 32);
    QString lit;

    auto flush = [&out, &lit]() {
        if (!lit.isEmpty()) {
            out += escapeHtml(lit);
            lit.clear();
        }
    };

    const int n = s.size();
    int i = 0;

    while (i < n) {
        const QChar ch = s.at(i);

        // ---------- 数学公式 ----------
        if (c.opt.mathToImage) {
            // \( ... \)  行内公式
            if (ch == QLatin1Char('\\') && i + 1 < n && s.at(i + 1) == QLatin1Char('(')) {
                const int cl = s.indexOf(QStringLiteral("\\)"), i + 2);
                if (cl > i + 1) {
                    const QString tex = s.mid(i + 2, cl - i - 2).trimmed();
                    if (!tex.isEmpty()) {
                        flush();
                        out += mathHtml(tex, false, c);
                        i = cl + 2;
                        continue;
                    }
                }
            }
            // \[ ... \]  块级公式(出现在段落中间时也按块级处理)
            if (ch == QLatin1Char('\\') && i + 1 < n && s.at(i + 1) == QLatin1Char('[')) {
                const int cl = s.indexOf(QStringLiteral("\\]"), i + 2);
                if (cl > i + 1) {
                    const QString tex = s.mid(i + 2, cl - i - 2).trimmed();
                    if (!tex.isEmpty()) {
                        flush();
                        out += mathHtml(tex, true, c);
                        i = cl + 2;
                        continue;
                    }
                }
            }
            if (ch == QLatin1Char('$')) {
                if (i + 1 < n && s.at(i + 1) == QLatin1Char('$')) {
                    const int cl = s.indexOf(QStringLiteral("$$"), i + 2);
                    if (cl > i + 1) {
                        const QString tex = s.mid(i + 2, cl - i - 2).trimmed();
                        if (!tex.isEmpty()) {
                            flush();
                            out += mathHtml(tex, true, c);
                            i = cl + 2;
                            continue;
                        }
                    }
                } else if (i + 1 < n && !s.at(i + 1).isSpace()) {
                    // 规则: 开 $ 后不能是空白, 闭 $ 前不能是空白且后面不能紧跟数字(避免误伤价格)
                    int j  = i + 1;
                    int cl = -1;
                    while (j < n) {
                        const QChar x = s.at(j);
                        if (x == QLatin1Char('\\')) {
                            j += 2;
                            continue;
                        }
                        if (x == QLatin1Char('\n'))
                            break;
                        if (x == QLatin1Char('$')) {
                            if (!s.at(j - 1).isSpace()
                                && !(j + 1 < n && s.at(j + 1).isDigit()))
                                cl = j;
                            break;
                        }
                        ++j;
                    }
                    if (cl > i + 1) {
                        const QString tex = s.mid(i + 1, cl - i - 1);
                        flush();
                        out += mathHtml(tex, false, c);
                        i = cl + 1;
                        continue;
                    }
                }
            }
        }

        // ---------- 反斜杠转义 ----------
        if (ch == QLatin1Char('\\') && i + 1 < n && isEscapable(s.at(i + 1))) {
            lit += s.at(i + 1);
            i += 2;
            continue;
        }

        // ---------- 行内代码 ----------
        if (ch == QLatin1Char('`')) {
            int run = 0;
            while (i + run < n && s.at(i + run) == QLatin1Char('`'))
                ++run;
            const QString fence(run, QLatin1Char('`'));
            const int close = s.indexOf(fence, i + run);
            if (close > i) {
                QString code = s.mid(i + run, close - i - run);
                if (code.size() > 1 && code.startsWith(QLatin1Char(' ')) && code.endsWith(QLatin1Char(' '))
                    && !code.trimmed().isEmpty())
                    code = code.mid(1, code.size() - 2);
                flush();
                out += openTag(QStringLiteral("code"), styleInlineCode(c))
                       + escapeHtml(code, false) + closeTag(QStringLiteral("code"));
                i = close + run;
                continue;
            }
        }

        // ---------- 图片 / 链接 ----------
        {
            const bool isImg = (ch == QLatin1Char('!') && i + 1 < n && s.at(i + 1) == QLatin1Char('['));
            if (isImg || ch == QLatin1Char('[')) {
                const int lb = isImg ? i + 1 : i;
                const int rb = findClosingBracket(s, lb);
                if (rb > 0) {
                    const QString label = s.mid(lb + 1, rb - lb - 1);
                    QString url, title;
                    int after = -1;

                    if (rb + 1 < n && s.at(rb + 1) == QLatin1Char('(')) {
                        const int rp = findClosingParen(s, rb + 1);
                        if (rp > 0) {
                            parseLinkDest(s.mid(rb + 2, rp - rb - 2), url, title);
                            after = rp + 1;
                        }
                    }
                    if (after < 0) {
                        QString id;
                        int a2 = -1;
                        if (rb + 1 < n && s.at(rb + 1) == QLatin1Char('[')) {
                            const int r2 = s.indexOf(QLatin1Char(']'), rb + 1);
                            if (r2 > 0) {
                                id  = s.mid(rb + 2, r2 - rb - 2);
                                a2  = r2 + 1;
                            }
                        } else {
                            id = label;
                            a2 = rb + 1;
                        }
                        const QString key = id.trimmed().toLower();
                        const auto it = c.refs.constFind(key);
                        if (it != c.refs.constEnd()) {
                            url   = it.value();
                            after = a2;
                        }
                    }

                    if (after > 0 && !url.isEmpty()) {
                        flush();
                        if (isImg) {
                            out += imageHtml(url, label, title, c);
                        } else {
                            out += anchorOpen(url, styleLink(c));
                            out += renderInline(label, c, depth + 1);
                            out += closeTag(QStringLiteral("a"));
                        }
                        i = after;
                        continue;
                    }
                }
            }
        }

        // ---------- <url> 自动链接 ----------
        if (ch == QLatin1Char('<')) {
            const int e = s.indexOf(QLatin1Char('>'), i);
            if (e > i && e - i < 300) {
                const QString inner = s.mid(i + 1, e - i - 1);
                const QRegularExpression mail(QStringLiteral("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$"));
                if (looksLikeUrl(inner)) {
                    flush();
                    out += anchorOpen(inner, styleLink(c)) + escapeHtml(inner)
                           + closeTag(QStringLiteral("a"));
                    i = e + 1;
                    continue;
                }
                if (mail.match(inner).hasMatch()) {
                    flush();
                    out += anchorOpen(QStringLiteral("mailto:") + inner, styleLink(c))
                           + escapeHtml(inner) + closeTag(QStringLiteral("a"));
                    i = e + 1;
                    continue;
                }
            }
        }

        // ---------- 强调 ----------
        if (ch == QLatin1Char('*') || ch == QLatin1Char('_')) {
            const QChar d = ch;
            // 同一字符连写时只在起始位置处理
            if (!(i > 0 && s.at(i - 1) == d)) {
                int run = 0;
                while (i + run < n && s.at(i + run) == d)
                    ++run;
                const int maxLen = qMin(run, 3);
                bool done = false;

                for (int len = maxLen; len >= 1 && !done; --len) {
                    if (d == QLatin1Char('_') && len == 1 && i > 0 && isIdentChar(s.at(i - 1)))
                        continue;

                    const QString fence(len, d);
                    int pos = i + run;

                    while (true) {
                        const int cl = s.indexOf(fence, pos);
                        if (cl < 0)
                            break;
                        const QString content = s.mid(i + run, cl - i - run);
                        const bool bad =
                            content.isEmpty() || content.at(0).isSpace() || content.at(content.size() - 1).isSpace()
                            || content.contains(QLatin1Char('\n'))
                            || (d == QLatin1Char('_') && cl + len < n && isIdentChar(s.at(cl + len)))
                            || (d == QLatin1Char('_') && cl > 0 && s.at(cl - 1).isSpace());
                        if (bad) {
                            pos = cl + 1;
                            continue;
                        }
                        const QString inner = renderInline(content, c, depth + 1);
                        flush();
                        if (len == 3)
                            out += openTag(QStringLiteral("strong"), styleStrong(c))
                                   + openTag(QStringLiteral("em"), styleEm(c)) + inner
                                   + closeTag(QStringLiteral("em")) + closeTag(QStringLiteral("strong"));
                        else if (len == 2)
                            out += openTag(QStringLiteral("strong"), styleStrong(c)) + inner
                                   + closeTag(QStringLiteral("strong"));
                        else
                            out += openTag(QStringLiteral("em"), styleEm(c)) + inner
                                   + closeTag(QStringLiteral("em"));
                        i    = cl + len;
                        done = true;
                        break;
                    }
                }
                if (done)
                    continue;
            }
        }

        // ---------- 删除线 ----------
        if (ch == QLatin1Char('~') && i + 1 < n && s.at(i + 1) == QLatin1Char('~')
            && !(i > 0 && s.at(i - 1) == QLatin1Char('~'))) {
            const int cl = s.indexOf(QStringLiteral("~~"), i + 2);
            if (cl > i + 2) {
                flush();
                out += openTag(QStringLiteral("del"), styleDel(c))
                       + renderInline(s.mid(i + 2, cl - i - 2), c, depth + 1)
                       + closeTag(QStringLiteral("del"));
                i = cl + 2;
                continue;
            }
        }

        // ---------- 换行 ----------
        if (ch == QLatin1Char('\n')) {
            bool hard = false;
            if (lit.size() >= 2 && lit.at(lit.size() - 1) == QLatin1Char(' ')
                && lit.at(lit.size() - 2) == QLatin1Char(' ')) {
                lit.chop(2);
                hard = true;
            } else if (!lit.isEmpty() && lit.at(lit.size() - 1) == QLatin1Char('\\')) {
                lit.chop(1);
                hard = true;
            }
            flush();
            out += hard ? QStringLiteral("<br/>") : QStringLiteral(" ");
            ++i;
            continue;
        }

        // ---------- 裸链接 ----------
        if (c.opt.autoLink && ch == QLatin1Char('h') && (i == 0 || !isIdentChar(s.at(i - 1)))
            && (startsWithAt(s, i, QStringLiteral("http://")) || startsWithAt(s, i, QStringLiteral("https://")))) {
            int e = i;
            while (e < n && !isAutoLinkTail(s.at(e)))
                ++e;
            while (e > i && QStringLiteral(".,;:!?)]}\"'、，。").contains(s.at(e - 1)))
                --e;
            if (e > i + 8) {
                flush();
                const QString u = s.mid(i, e - i);
                out += anchorOpen(u, styleLink(c)) + escapeHtml(u) + closeTag(QStringLiteral("a"));
                i = e;
                continue;
            }
        }

        lit += ch;
        ++i;
    }

    flush();
    return out;
}

// ============================================================
//  块级渲染
// ============================================================

void renderBlocks(const QVector<Block> &blocks, Ctx &c, QString &out);

QString renderCodeBlock(const Block &b, Ctx &c)
{
    const int fs = c.opt.fontSize - 2;
    const bool dark = c.t->codeDark;
    const QString textColor = dark ? c.t->code.text : c.t->code.text;

    QString body;
    if (c.opt.syntaxHighlight && !b.info.isEmpty() && b.code.size() < 20000) {
        body = highlightCode(b.code, b.info, c.t->code);
    } else {
        body = escapeHtml(b.code, false);
    }

    // 顶部语言条
    QString header;
    {
        const QString label = b.info.isEmpty() ? QStringLiteral("CODE") : b.info.toUpper();
        const QString dotColor = dark ? QStringLiteral("#5c6370") : QStringLiteral("#c9ced6");
        const QString labelColor = dark ? QStringLiteral("#8b93a7") : QStringLiteral("#8a8f98");
        header = QStringLiteral("<section style=\"padding:7px 14px;background:")
                 + c.t->codeHeaderBg
                 + QStringLiteral(";font-size:11px;letter-spacing:1.5px;color:") + labelColor
                 + QStringLiteral(";font-family:Menlo,Consolas,monospace\">")
                 + QStringLiteral("<span style=\"display:inline-block;width:8px;height:8px;border-radius:50%;background:")
                 + dotColor + QStringLiteral(";margin-right:5px\"></span>")
                 + QStringLiteral("<span style=\"display:inline-block;width:8px;height:8px;border-radius:50%;background:")
                 + dotColor + QStringLiteral(";margin-right:8px\"></span>")
                 + escapeHtml(label) + QStringLiteral("</section>");
    }

    // 代码块默认保留原始换行(横向滚动), 可选自动换行以适配手机屏
    const QString wsMode = c.opt.codeWrap
                               ? QStringLiteral("white-space:pre-wrap;word-break:break-all;word-wrap:break-word")
                               : QStringLiteral("white-space:pre;word-break:normal;word-wrap:normal;overflow-x:auto");

    const QString preStyle =
        QStringLiteral("margin:0;padding:14px 16px;background:") + c.t->code.bg
        + QStringLiteral(";color:") + textColor
        + QStringLiteral(";font-size:") + QString::number(fs)
        + QStringLiteral("px;line-height:1.65;font-family:Menlo,Consolas,'Courier New',monospace;")
        + wsMode + QStringLiteral(";letter-spacing:0");

    const QString codeStyle =
        QStringLiteral("font-family:inherit;font-size:inherit;color:inherit;background:none;padding:0;")
        + wsMode;

    return QStringLiteral("<section style=\"margin:0 0 ")
           + QString::number(c.paraMargin) + QStringLiteral("px;border-radius:6px;overflow:hidden;background:")
           + c.t->code.bg + QStringLiteral("\">")
           + header
           + openTag(QStringLiteral("pre"), preStyle)
           + QStringLiteral("<code data-lang=\"") + escapeAttr(b.info) + QStringLiteral("\" style=\"")
           + codeStyle + QStringLiteral("\">")
           + body + closeTag(QStringLiteral("code")) + closeTag(QStringLiteral("pre"))
           + closeTag(QStringLiteral("section"));
}

QString renderTable(const Block &b, Ctx &c)
{
    if (b.rows.isEmpty())
        return QString();

    const QString border = QStringLiteral("1px solid ") + c.t->tdBorder;
    const int fs = c.opt.fontSize - 1;

    auto alignOf = [&](int col) -> QString {
        if (col >= b.aligns.size())
            return QStringLiteral("left");
        switch (b.aligns.at(col)) {
        case Align::Center: return QStringLiteral("center");
        case Align::Right:  return QStringLiteral("right");
        default:            return QStringLiteral("left");
        }
    };

    QString html = QStringLiteral("<section style=\"margin:0 0 ") + QString::number(c.paraMargin)
                   + QStringLiteral("px;overflow-x:auto\">")
                   + QStringLiteral("<table style=\"border-collapse:collapse;width:100%;font-size:")
                   + QString::number(fs) + QStringLiteral("px;line-height:1.6;color:") + c.t->text
                   + QStringLiteral("\">");

    for (int r = 0; r < b.rows.size(); ++r) {
        const QStringList &row = b.rows.at(r);
        html += r == 0 ? QStringLiteral("<thead>") : (r == 1 ? QStringLiteral("<tbody>") : QString());
        html += QStringLiteral("<tr>");
        for (int col = 0; col < row.size(); ++col) {
            const QString cellStyle =
                QStringLiteral("border:") + border + QStringLiteral(";padding:8px 10px;text-align:")
                + alignOf(col)
                + (r == 0 ? QStringLiteral(";background:") + c.t->thBg + QStringLiteral(";color:") + c.t->thText
                              + QStringLiteral(";font-weight:600")
                          : QString());
            const QString tag = (r == 0) ? QStringLiteral("th") : QStringLiteral("td");
            html += openTag(tag, cellStyle) + renderInline(row.at(col), c, 0) + closeTag(tag);
        }
        html += QStringLiteral("</tr>");
        if (r == 0)
            html += QStringLiteral("</thead>");
    }
    html += QStringLiteral("</tbody></table></section>");
    return html;
}

QString renderList(const Block &b, Ctx &c)
{
    ++c.listDepth;
    const bool ordered = b.ordered;
    const QString tag  = ordered ? QStringLiteral("ol") : QStringLiteral("ul");
    const int indent   = qMax(c.opt.fontSize + 8, 22);

    QString listStyle = QStringLiteral("margin:0 0 ") + QString::number(c.paraMargin)
                        + QStringLiteral("px;padding-left:") + QString::number(indent)
                        + QStringLiteral("px;list-style-type:")
                        + (ordered ? QStringLiteral("decimal") : QStringLiteral("disc"));

    QString html = QStringLiteral("<") + tag + QStringLiteral(" style=\"") + listStyle + QStringLiteral("\"");
    if (ordered && b.start != 1)
        html += QStringLiteral(" start=\"") + QString::number(b.start) + QStringLiteral("\"");
    html += QLatin1Char('>');

    for (const Block &item : b.children) {
        if (item.type != Block::ListItem)
            continue;

        QString inner;
        if (item.isTask) {
            inner += QStringLiteral("<span style=\"color:")
                     + (item.checked ? c.t->accent : c.t->textLight)
                     + QStringLiteral(";margin-right:6px;font-weight:600\">")
                     + (item.checked ? QStringLiteral("&#9745;") : QStringLiteral("&#9744;"))
                     + QStringLiteral("</span>");
        }

        const bool onlyPara = (item.children.size() == 1 && item.children.first().type == Block::Paragraph);
        if (b.tight && onlyPara) {
            inner += renderInline(item.children.first().text, c, 0);
        } else {
            const int saved = c.paraMargin;
            c.paraMargin    = 8;
            QString sub;
            renderBlocks(item.children, c, sub);
            inner += sub;
            c.paraMargin = saved;
        }

        if (inner.isEmpty())
            inner = QStringLiteral("&#8203;");

        html += QStringLiteral("<li style=\"margin:6px 0;font-size:") + QString::number(c.opt.fontSize)
                + QStringLiteral("px;line-height:") + num(c.opt.lineHeight)
                + QStringLiteral(";color:") + c.t->text
                + QStringLiteral(";letter-spacing:.5px;word-break:break-word\">")
                + inner + closeTag(QStringLiteral("li"));
    }

    html += closeTag(tag);
    --c.listDepth;
    return html;
}

void renderBlocks(const QVector<Block> &blocks, Ctx &c, QString &out)
{
    for (const Block &b : blocks) {
        switch (b.type) {
        case Block::Paragraph: {
            if (b.text.trimmed().isEmpty())
                break;

            QString    text   = b.text;
            const bool indent = stripLeadingIndent(text);
            if (text.trimmed().isEmpty())
                break;                        // 整段只有一个缩进标记, 忽略

            QString st = (c.captionMode != 0) ? styleCaptionP(c) : styleP(c);
            if (indent)
                st += QStringLiteral(";text-indent:2em");

            out += openTag(QStringLiteral("p"), st)
                   + renderInline(normalizeLatexSpacers(text), c, 0)
                   + closeTag(QStringLiteral("p"));
            break;
        }
        case Block::Heading: {
            const int lvl = qBound(1, b.level, 6);
            const QString tag = QStringLiteral("h") + QString::number(qMin(lvl, 4));
            out += openTag(tag, styleHeading(c, lvl)) + renderInline(b.text, c, 0)
                   + closeTag(tag);
            break;
        }
        case Block::CodeBlock:
            out += renderCodeBlock(b, c);
            break;
        case Block::Math: {
            // 块级公式: 居中独立显示
            out += QStringLiteral("<section style=\"margin:0 0 ") + QString::number(c.paraMargin)
                   + QStringLiteral("px;padding:4px 0;text-align:center;line-height:1.9\">")
                   + mathHtml(b.code, true, c)
                   + QStringLiteral("</section>");
            break;
        }
        case Block::Quote: {
            QString inner;
            const int saved = c.paraMargin;
            c.paraMargin    = 10;
            renderBlocks(b.children, c, inner);
            c.paraMargin = saved;

            const QString style = QStringLiteral("margin:0 0 ") + QString::number(c.paraMargin)
                                  + QStringLiteral("px;padding:12px 16px;background:") + c.t->quoteBg
                                  + QStringLiteral(";border-left:4px solid ") + c.t->quoteBorder
                                  + QStringLiteral(";border-radius:0 6px 6px 0;color:") + c.t->quoteText
                                  + QStringLiteral(";font-size:") + QString::number(c.opt.fontSize - 1)
                                  + QStringLiteral("px;line-height:") + num(c.opt.lineHeight + 0.05)
                                  + QStringLiteral(";word-break:break-word");
            out += openTag(QStringLiteral("blockquote"), style) + inner
                   + closeTag(QStringLiteral("blockquote"));
            break;
        }
        case Block::List:
            out += renderList(b, c);
            break;
        case Block::Container: {
            // ::: hljs-center 之类的容器, 内容按"图注"排版(居中 + 小字浅色)
            const QString n = b.info.toLower();
            int mode = 0;
            if (n.contains(QStringLiteral("center")))
                mode = 1;
            else if (n.contains(QStringLiteral("right")))
                mode = 2;
            else if (n.contains(QStringLiteral("left")))
                mode = 3;

            const int saved = c.captionMode;
            c.captionMode   = mode;
            QString inner;
            renderBlocks(b.children, c, inner);
            c.captionMode = saved;

            QString align = QStringLiteral("left");
            if (mode == 1)
                align = QStringLiteral("center");
            else if (mode == 2)
                align = QStringLiteral("right");

            out += QStringLiteral("<section style=\"margin:0 0 ")
                   + QString::number(c.paraMargin) + QStringLiteral("px;text-align:")
                   + align + QStringLiteral(";word-break:break-word\">")
                   + inner + QStringLiteral("</section>");
            break;
        }
        case Block::Table:
            out += renderTable(b, c);
            break;
        case Block::Hr:
            out += QStringLiteral("<section style=\"margin:28px 0;border-top:1px solid ") + c.t->border
                   + QStringLiteral(";height:0;font-size:0;line-height:0;overflow:hidden\">&#8203;</section>");
            break;
        case Block::Html:
            if (c.opt.allowRawHtml)
                out += stripDangerous(b.code);
            else
                out += openTag(QStringLiteral("p"), styleP(c)) + escapeHtml(b.code) + closeTag(QStringLiteral("p"));
            break;
        case Block::ListItem:
        default:
            break;
        }
    }
}

// ============================================================
//  引用式链接定义
// ============================================================

QHash<QString, QString> collectRefs(const QString &src)
{
    QHash<QString, QString> refs;
    static const QRegularExpression re(
        QStringLiteral("^ {0,3}\\[([^\\]]+)\\]:[ \\t]*(<?[^\\s>]+>?)(?:[ \\t]+\"([^\"]*)\")?[ \\t]*$"),
        QRegularExpression::MultilineOption);
    auto it = re.globalMatch(src);
    while (it.hasNext()) {
        const auto m = it.next();
        QString url = m.captured(2).trimmed();
        if (url.startsWith(QLatin1Char('<')) && url.endsWith(QLatin1Char('>')))
            url = url.mid(1, url.size() - 2);
        refs.insert(m.captured(1).trimmed().toLower(), url);
    }
    return refs;
}

void collectStats(const QVector<Block> &blocks, int &paragraphs, int &chars)
{
    for (const Block &b : blocks) {
        switch (b.type) {
        case Block::Paragraph:
            ++paragraphs;
            chars += b.text.size();
            break;
        case Block::Heading:
            chars += b.text.size();
            break;
        case Block::CodeBlock:
            chars += b.code.size();
            break;
        case Block::Quote:
        case Block::List:
        case Block::ListItem:
        case Block::Container:
            collectStats(b.children, paragraphs, chars);
            break;
        case Block::Table:
            for (const auto &r : b.rows)
                for (const auto &cell : r)
                    chars += cell.size();
            break;
        default:
            break;
        }
    }
}

} // namespace

// ============================================================
//  对外接口
// ============================================================

RenderResult renderMarkdown(const QString &source, const RenderOptions &opt)
{
    RenderResult res;
    const StyleTheme &t = styleThemeById(opt.themeId);

    Ctx c;
    c.t   = &t;
    c.opt = opt;
    c.refs = collectRefs(source);

    const QVector<Block> blocks = parse(source);

    c.images = 0;
    renderBlocks(blocks, c, res.body);

    res.images = c.images;

    int paragraphs = 0, chars = 0;
    collectStats(blocks, paragraphs, chars);
    res.paragraphs = paragraphs;

    // 字数: 去掉空白后的字符数
    int effective = 0;
    for (QChar ch : source)
        if (!ch.isSpace())
            ++effective;
    res.chars = effective;
    res.readMinutes = qMax(1, int(effective / 400.0 + 0.5));

    return res;
}

QString toWeChatHtml(const RenderResult &r, const StyleTheme &t, int fontSize)
{
    QString style = QStringLiteral("font-family:") + t.fontFamily
                    + QStringLiteral(";font-size:") + QString::number(fontSize)
                    + QStringLiteral("px;line-height:1.75;color:") + t.text
                    + QStringLiteral(";letter-spacing:.5px;word-break:break-word;text-align:left");

    if (t.pageBg.compare(QStringLiteral("#ffffff"), Qt::CaseInsensitive) != 0) {
        style += QStringLiteral(";background:") + t.pageBg
                 + QStringLiteral(";padding:22px 16px;border-radius:8px");
    }

    QString html;
    html.reserve(r.body.size() + 256);
    html += QStringLiteral("<section style=\"") + style + QStringLiteral("\">");
    html += r.body;
    html += QStringLiteral("</section>");
    return html;
}

QString htmlToPlainText(const QString &html)
{
    QString s = html;
    const auto ci = QRegularExpression::CaseInsensitiveOption;

    auto rep = [&s](const QString &pat, const QString &to, bool remove = false) {
        const QRegularExpression re(pat, QRegularExpression::CaseInsensitiveOption);
        if (remove)
            s.remove(re);
        else
            s.replace(re, to);
    };

    rep(QStringLiteral("<\\s*br\\s*/?\\s*>"), QStringLiteral("\n"));
    rep(QStringLiteral("<\\s*/\\s*(p|section|div|blockquote|h[1-6]|tr)\\s*>"), QStringLiteral("\n"));
    rep(QStringLiteral("<\\s*li[^>]*>"), QStringLiteral("· "));
    rep(QStringLiteral("<\\s*/\\s*li\\s*>"), QStringLiteral("\n"));
    rep(QStringLiteral("<\\s*/?\\s*(td|th)[^>]*>"), QStringLiteral("\t"));
    rep(QStringLiteral("<\\s*img[^>]*>"), QStringLiteral("[图片]"));
    rep(QStringLiteral("<[^>]+>"), QString(), true);          // 剩下所有标签

    s.replace(QStringLiteral("&emsp;&emsp;"), QStringLiteral("　　"));
    s.replace(QStringLiteral("&emsp;"), QStringLiteral("　"));
    s.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    s.replace(QStringLiteral("&thinsp;"), QStringLiteral(" "));
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));  // 必须放最后

    // 去掉每行首尾多余空白, 并把 3 个以上连续换行压成 2 个
    QStringList lines = s.split(QLatin1Char('\n'));
    for (QString &l : lines)
        l = l.trimmed();
    s = lines.join(QLatin1Char('\n'));
    s.replace(QRegularExpression(QStringLiteral("\n{3,}")), QStringLiteral("\n\n"));
    return s.trimmed();
}

QString mathTypesetFunctionName()
{
    return QStringLiteral("md2wxTypesetMath");
}

// 预览页里把 [data-md2wx-math] 占位符批量转成 PNG 的脚本。
// 走 MathJax(SVG) -> Blob -> canvas -> PNG dataURI, 全部离线完成。
static QString mathTypesetScript()
{
    // 用 fromUtf8 而不是 QStringLiteral: 原始字符串字面量不能和 u"" 拼接
    return QString::fromUtf8(R"JS(
window.__md2wxMathResult = null;
window.md2wxTypesetMath = function () {
  var nodes = Array.prototype.slice.call(document.querySelectorAll('[data-md2wx-math]'));
  if (!nodes.length) { window.__md2wxMathResult = '[]'; return; }
  var out = [];
  var SS  = 3;                       // 3 倍超采样, 手机上不糊
  var finish = function () { window.__md2wxMathResult = JSON.stringify(out); };

  function one(el) {
    var tex   = el.getAttribute('data-tex') || '';
    var disp  = el.getAttribute('data-display') === '1';
    var color = el.getAttribute('data-color') || '#1a1a1a';
    var push  = function (obj) { obj.tex = tex; obj.display = disp; obj.color = color; out.push(obj); };
    try {
      var node = MathJax.tex2svg(tex, { display: disp, em: 16, ex: 8, containerWidth: 1000000 });
      var svg  = null;
      if (node && node.tagName && String(node.tagName).toLowerCase() === 'svg') svg = node;
      else if (node && node.querySelector) svg = node.querySelector('svg');
      if (!svg) throw new Error('no svg');

      var vb = (svg.getAttribute('viewBox') || '').split(/[\s,]+/).map(Number);
      if (vb.length < 4 || !(vb[2] > 0) || !(vb[3] > 0)) throw new Error('bad viewBox');

      var U = 1000, EM = 16;                  // MathJax 内部单位: 1000 = 1em
      var cssW  = vb[2] / U * EM;
      var cssH  = vb[3] / U * EM;
      var depth = (-vb[1]) / U * EM;

      svg.setAttribute('xmlns', 'http://www.w3.org/2000/svg');
      svg.setAttribute('width',  cssW + 'px');
      svg.setAttribute('height', cssH + 'px');
      svg.style.color = color;

      var xml = new XMLSerializer().serializeToString(svg);
      var url = URL.createObjectURL(new Blob([xml], { type: 'image/svg+xml;charset=utf-8' }));
      return new Promise(function (resolve) {
        var im = new Image();
        im.onload = function () {
          try {
            var cv = document.createElement('canvas');
            cv.width  = Math.max(2, Math.round(cssW * SS));
            cv.height = Math.max(2, Math.round(cssH * SS));
            var ctx = cv.getContext('2d');
            ctx.drawImage(im, 0, 0, cv.width, cv.height);
            push({ png: cv.toDataURL('image/png'), w: cssW, h: cssH, depth: depth });
          } catch (e) { push({ err: '' + e }); }
          URL.revokeObjectURL(url);
          resolve();
        };
        im.onerror = function () { push({ err: 'svg image load failed' }); URL.revokeObjectURL(url); resolve(); };
        im.src = url;
      });
    } catch (e) {
      push({ err: '' + e });
      return Promise.resolve();
    }
  }

  var start = function () {
    var chain = Promise.resolve();
    nodes.forEach(function (el) { chain = chain.then(function () { return one(el); }); });
    chain.then(finish).catch(finish);
  };

  // MathJax 是异步加载的, 等它就绪(最多约 10s), 就绪前不要武断地标记失败
  var tries = 0;
  var boot = function () {
    if (window.MathJax && MathJax.startup && MathJax.startup.promise) {
      MathJax.startup.promise.then(start).catch(start);
    } else if (tries++ < 100) {
      setTimeout(boot, 100);
    } else {
      window.__md2wxMathResult = '[]';
    }
  };
  boot();
};
)JS");
}

// 预览页里的滚动同步脚本: 节流后把滚动比例 console.log 出去, 由 PreviewPage 接住
static QString scrollBridgeScript()
{
    return QString::fromUtf8(R"JS(
(function () {
  var timer = null;
  function report() {
    var m = document.documentElement.scrollHeight - window.innerHeight;
    if (m > 0)
      console.log('md2wx:scroll:' + (window.scrollY / m).toFixed(5));
  }
  window.addEventListener('scroll', function () {
    if (timer) return;
    timer = setTimeout(function () { timer = null; report(); }, 90);
  }, { passive: true });

  // 供 C++ 侧调用
  window.md2wxScrollTo = function (ratio) {
    var m = document.documentElement.scrollHeight - window.innerHeight;
    if (m > 0) window.scrollTo(0, m * ratio);
  };

  console.log('md2wx:ready');
})();
)JS");
}

QString toPreviewDocument(const QString &wechatHtml, const StyleTheme &t, bool darkCanvas,
                          const QString &mathScriptUrl, bool withScrollBridge)
{
    const bool withMath = !mathScriptUrl.isEmpty();
    const QString canvas = darkCanvas ? QStringLiteral("#1a1a1c") : t.canvasBg;
    const QString shadow = darkCanvas ? QStringLiteral("0 8px 30px rgba(0,0,0,.55)")
                                      : QStringLiteral("0 6px 26px rgba(0,0,0,.10)");

    QString doc;
    doc += QStringLiteral("<!DOCTYPE html><html><head><meta charset=\"utf-8\">");
    doc += QStringLiteral("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");

    if (withMath) {
        doc += QStringLiteral("<script>window.MathJax={startup:{typeset:false},"
                              "svg:{fontCache:'local',scale:1}};</script>");
        doc += QStringLiteral("<script src=\"") + escapeAttr(mathScriptUrl)
               + QStringLiteral("\"></script>");
    }

    doc += QStringLiteral("<style>");
    doc += QStringLiteral("html,body{margin:0;padding:0;background:") + canvas + QStringLiteral(";}");
    doc += QStringLiteral("body{padding:26px 0 70px;font-family:") + t.fontFamily + QStringLiteral(";}");
    doc += QStringLiteral("img{max-width:100%;}");
    doc += QStringLiteral("#paper{width:677px;max-width:calc(100% - 40px);margin:0 auto;background:")
           + t.pageBg + QStringLiteral(";border-radius:10px;box-shadow:") + shadow
           + QStringLiteral(";padding:34px 24px;box-sizing:border-box;}");
    doc += QStringLiteral("#paper table{width:100%;}");
    doc += QStringLiteral("::-webkit-scrollbar{width:6px;height:6px;}");
    doc += QStringLiteral("::-webkit-scrollbar-thumb{background:rgba(128,128,128,.42);border-radius:3px;}");
    doc += QStringLiteral("::-webkit-scrollbar-thumb:hover{background:rgba(128,128,128,.65);}");
    doc += QStringLiteral("::-webkit-scrollbar-track{background:transparent;}");
    doc += QStringLiteral("</style>");

    if (withMath)
        doc += QStringLiteral("<script>") + mathTypesetScript() + QStringLiteral("</script>");

    if (withScrollBridge)
        doc += QStringLiteral("<script>") + scrollBridgeScript() + QStringLiteral("</script>");

    doc += QStringLiteral("</head><body>");
    doc += QStringLiteral("<section id=\"paper\">") + wechatHtml + QStringLiteral("</section>");
    doc += QStringLiteral("</body></html>");
    return doc;
}

} // namespace md
