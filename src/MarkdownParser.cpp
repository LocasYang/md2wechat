#include "MarkdownParser.h"

#include <QRegularExpression>

namespace md {

// ------------------------------------------------------------
//  基础工具
// ------------------------------------------------------------

static bool isBlank(const QString &s)
{
    for (QChar c : s)
        if (c != QLatin1Char(' ') && c != QLatin1Char('\t'))
            return false;
    return true;
}

static int leadingSpaces(const QString &s)
{
    int n = 0;
    for (QChar c : s) {
        if (c == QLatin1Char(' '))
            n += 1;
        else if (c == QLatin1Char('\t'))
            n += 4;
        else
            break;
    }
    return n;
}

// 去掉行首最多 n 列的缩进
static QString stripIndent(const QString &s, int n)
{
    int removed = 0, i = 0;
    while (i < s.size() && removed < n) {
        if (s.at(i) == QLatin1Char(' ')) {
            removed += 1;
            ++i;
        } else if (s.at(i) == QLatin1Char('\t')) {
            removed += 4;
            ++i;
        } else {
            break;
        }
    }
    return s.mid(i);
}

static QString stripIndentMax(const QString &s, int n)
{
    int i = 0, removed = 0;
    while (i < s.size() && removed < n && s.at(i) == QLatin1Char(' ')) {
        ++i;
        ++removed;
    }
    return s.mid(i);
}

bool isEscapable(QChar c)
{
    static const QString punct = QStringLiteral("\\`*_{}[]()#+-.!|~<>\"'$%&,/:;=?@^");
    return punct.contains(c);
}

QString escapeHtml(const QString &text, bool preserveEntities)
{
    // 注意: 这里不能用 ^ 锚点 —— 一旦带锚点, AnchoredMatchOption 会被忽略,
    // 从字符串中间(offset != 0)匹配时就永远命中不了, 导致 &emsp; 之类被误转义。
    static const QRegularExpression ent(
        QStringLiteral("&(#\\d{1,7}|#[xX][0-9a-fA-F]{1,6}|[a-zA-Z][a-zA-Z0-9]{1,31});"));

    QString out;
    out.reserve(text.size() + 16);
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('<')) {
            out += QStringLiteral("&lt;");
        } else if (c == QLatin1Char('>')) {
            out += QStringLiteral("&gt;");
        } else if (c == QLatin1Char('"')) {
            out += QStringLiteral("&quot;");
        } else if (c == QLatin1Char('&')) {
            if (preserveEntities) {
                const auto m = ent.match(text, i, QRegularExpression::NormalMatch,
                                         QRegularExpression::AnchoredMatchOption);
                if (m.hasMatch()) {
                    out += m.captured(0);
                    i += m.capturedLength(0) - 1;
                    continue;
                }
            }
            out += QStringLiteral("&amp;");
        } else {
            out += c;
        }
    }
    return out;
}

QString escapeAttr(const QString &text)
{
    QString out;
    out.reserve(text.size() + 8);
    for (QChar c : text) {
        if (c == QLatin1Char('&'))
            out += QStringLiteral("&amp;");
        else if (c == QLatin1Char('"'))
            out += QStringLiteral("&quot;");
        else if (c == QLatin1Char('<'))
            out += QStringLiteral("&lt;");
        else if (c == QLatin1Char('>'))
            out += QStringLiteral("&gt;");
        else if (c == QLatin1Char('\n') || c == QLatin1Char('\r'))
            out += QLatin1Char(' ');
        else
            out += c;
    }
    return out;
}

// ------------------------------------------------------------
//  行级结构识别
// ------------------------------------------------------------

// 围栏代码块起始行
static bool matchFence(const QString &line, QChar &ch, int &len, QString &info)
{
    int i = 0;
    while (i < line.size() && i < 3 && line.at(i) == QLatin1Char(' '))
        ++i;
    if (i >= line.size())
        return false;
    const QChar c = line.at(i);
    if (c != QLatin1Char('`') && c != QLatin1Char('~'))
        return false;
    int j = i;
    while (j < line.size() && line.at(j) == c)
        ++j;
    if (j - i < 3)
        return false;
    const QString rest = line.mid(j);
    if (c == QLatin1Char('`') && rest.contains(QLatin1Char('`')))
        return false;
    ch   = c;
    len  = j - i;
    info = rest.trimmed();
    return true;
}

static bool matchAtx(const QString &line, int &level, QString &text)
{
    int i = 0;
    while (i < line.size() && i < 3 && line.at(i) == QLatin1Char(' '))
        ++i;
    int s = i;
    while (i < line.size() && line.at(i) == QLatin1Char('#'))
        ++i;
    const int n = i - s;
    if (n < 1 || n > 6)
        return false;
    if (i < line.size() && line.at(i) != QLatin1Char(' '))
        return false;

    QString t = line.mid(i).trimmed();
    // 去掉结尾的闭合 # 序列
    int e = t.size();
    while (e > 0 && t.at(e - 1) == QLatin1Char('#'))
        --e;
    if (e < t.size()) {
        if (e == 0)
            t.clear();
        else if (t.at(e - 1) == QLatin1Char(' '))
            t = t.left(e).trimmed();
    }
    level = n;
    text  = t;
    return true;
}

// 分割线
static bool matchHr(const QString &line)
{
    int i = 0;
    while (i < line.size() && i < 3 && line.at(i) == QLatin1Char(' '))
        ++i;
    if (i >= line.size())
        return false;
    const QChar c = line.at(i);
    if (c != QLatin1Char('*') && c != QLatin1Char('-') && c != QLatin1Char('_'))
        return false;
    int count = 0;
    for (; i < line.size(); ++i) {
        const QChar x = line.at(i);
        if (x == c)
            ++count;
        else if (x != QLatin1Char(' ') && x != QLatin1Char('\t'))
            return false;
    }
    return count >= 3;
}

// setext 下划线
static bool matchSetext(const QString &line, int &level)
{
    int i = 0;
    while (i < line.size() && i < 3 && line.at(i) == QLatin1Char(' '))
        ++i;
    if (i >= line.size())
        return false;
    const QChar c = line.at(i);
    if (c != QLatin1Char('=') && c != QLatin1Char('-'))
        return false;
    int count = 0;
    for (; i < line.size(); ++i) {
        if (line.at(i) == c)
            ++count;
        else if (line.at(i) != QLatin1Char(' ') && line.at(i) != QLatin1Char('\t'))
            return false;
    }
    if (count == 0)
        return false;
    level = (c == QLatin1Char('=')) ? 1 : 2;
    return true;
}

static bool matchQuote(const QString &line, QString &rest)
{
    int i = 0;
    while (i < line.size() && i < 3 && line.at(i) == QLatin1Char(' '))
        ++i;
    if (i >= line.size() || line.at(i) != QLatin1Char('>'))
        return false;
    ++i;
    if (i < line.size() && line.at(i) == QLatin1Char(' '))
        ++i;
    rest = line.mid(i);
    return true;
}

struct Marker
{
    int     indent     = 0;
    int     contentCol = 0;
    bool    ordered    = false;
    int     num        = 1;
    QString text;
};

static bool matchMarker(const QString &line, Marker &m)
{
    int i = 0, ind = 0;
    while (i < line.size()) {
        if (line.at(i) == QLatin1Char(' ')) {
            ++ind;
            ++i;
        } else if (line.at(i) == QLatin1Char('\t')) {
            ind += 4;
            ++i;
        } else {
            break;
        }
    }
    if (ind >= 4 || i >= line.size())
        return false;

    const QChar c = line.at(i);

    if (c == QLatin1Char('*') || c == QLatin1Char('+') || c == QLatin1Char('-')) {
        if (i + 1 < line.size() && line.at(i + 1) != QLatin1Char(' '))
            return false;
        m.indent     = ind;
        m.ordered    = false;
        m.num        = 1;
        int c2       = (i + 1 < line.size()) ? i + 2 : i + 1;
        // 允许 "-   content" 多个空格
        while (c2 < line.size() && line.at(c2) == QLatin1Char(' '))
            ++c2;
        m.contentCol = c2;
        m.text       = line.mid(c2);
        return true;
    }

    if (c.isDigit()) {
        int j = i;
        while (j < line.size() && line.at(j).isDigit())
            ++j;
        if (j - i > 9 || j >= line.size())
            return false;
        if (line.at(j) != QLatin1Char('.') && line.at(j) != QLatin1Char(')'))
            return false;
        if (j + 1 < line.size() && line.at(j + 1) != QLatin1Char(' '))
            return false;
        int c2 = (j + 1 < line.size()) ? j + 2 : j + 1;
        while (c2 < line.size() && line.at(c2) == QLatin1Char(' '))
            ++c2;
        m.indent     = ind;
        m.contentCol = c2;
        m.ordered    = true;
        m.num        = line.mid(i, j - i).toInt();
        m.text       = line.mid(c2);
        return true;
    }
    return false;
}

static bool matchHtmlStart(const QString &line)
{
    const QString t = line.trimmed();
    if (t.isEmpty() || t.at(0) != QLatin1Char('<'))
        return false;
    if (t.startsWith(QStringLiteral("<!--")))
        return true;
    if (t.size() < 2)
        return false;
    const QChar c = t.at(1);
    if (c == QLatin1Char('/'))
        return t.size() > 2 && t.at(2).isLetter();
    return c.isLetter();
}

// 块级公式: $$ ... $$  或  \[ ... \]
// 支持单行 $$x$$ 与跨行, 未闭合则不当公式处理
static bool scanMathBlock(const QStringList &lines, int i, int end, QString &tex, int &next)
{
    const QString l0 = stripIndentMax(lines.at(i), 3);
    QString open, close;
    if (l0.startsWith(QStringLiteral("$$"))) {
        open  = QStringLiteral("$$");
        close = QStringLiteral("$$");
    } else if (l0.startsWith(QStringLiteral("\\["))) {
        open  = QStringLiteral("\\[");
        close = QStringLiteral("\\]");
    } else {
        return false;
    }

    const QString body = l0.mid(open.size());
    const int p = body.indexOf(close);
    if (p >= 0) {
        if (!body.mid(p + close.size()).trimmed().isEmpty())
            return false;                      // 闭合后还有内容 -> 交给段落
        tex  = body.left(p).trimmed();
        next = i + 1;
        return true;
    }

    QStringList parts;
    if (!body.trimmed().isEmpty())
        parts << body.trimmed();

    int j = i + 1;
    while (j < end) {
        const QString cur = lines.at(j);
        const int q = cur.indexOf(close);
        if (q >= 0) {
            const QString head = cur.left(q).trimmed();
            if (!head.isEmpty())
                parts << head;
            if (!cur.mid(q + close.size()).trimmed().isEmpty())
                return false;
            tex  = parts.join(QLatin1Char('\n'));
            next = j + 1;
            return true;
        }
        parts << cur;
        ++j;
    }
    return false;                              // 未闭合
}

// 表格分隔行: | --- | :--: |
static bool isTableDelim(const QString &line, QVector<Align> &aligns)
{
    const QString t = line.trimmed();
    if (t.isEmpty() || !t.contains(QLatin1Char('-')))
        return false;

    QStringList cells = t.split(QLatin1Char('|'));
    // 去掉首尾空单元(由管头/管尾造成)
    if (!cells.isEmpty() && cells.first().trimmed().isEmpty())
        cells.removeFirst();
    if (!cells.isEmpty() && cells.last().trimmed().isEmpty())
        cells.removeLast();
    if (cells.isEmpty())
        return false;

    QVector<Align> a;
    for (const QString &raw : cells) {
        const QString c = raw.trimmed();
        if (c.isEmpty())
            return false;
        const bool left  = c.startsWith(QLatin1Char(':'));
        const bool right = c.endsWith(QLatin1Char(':'));
        int dashes = 0;
        for (QChar x : c) {
            if (x == QLatin1Char('-'))
                ++dashes;
            else if (x != QLatin1Char(':'))
                return false;
        }
        if (dashes < 1)
            return false;
        a << (left && right ? Align::Center : (right ? Align::Right : Align::Left));
    }
    aligns = a;
    return true;
}

static QStringList splitTableRow(const QString &line)
{
    QString t = line.trimmed();
    if (t.startsWith(QLatin1Char('|')))
        t.remove(0, 1);
    if (t.endsWith(QLatin1Char('|')))
        t.chop(1);

    QStringList cells;
    QString cur;
    for (int i = 0; i < t.size(); ++i) {
        const QChar c = t.at(i);
        if (c == QLatin1Char('\\') && i + 1 < t.size()) {
            const QChar n = t.at(i + 1);
            if (n == QLatin1Char('|')) {
                cur += QLatin1Char('|');
                ++i;
                continue;
            }
            cur += c;
            cur += n;
            ++i;
            continue;
        }
        if (c == QLatin1Char('|')) {
            cells << cur.trimmed();
            cur.clear();
        } else {
            cur += c;
        }
    }
    cells << cur.trimmed();
    return cells;
}

// ::: hljs-center ... :::   容器语法(mdnice / hexo 常用)
static bool scanContainer(const QStringList &lines, int i, int end,
                          QString &name, QStringList &body, int &next)
{
    const QString head = lines.at(i).trimmed();
    if (!head.startsWith(QStringLiteral(":::")))
        return false;

    const QString nm = head.mid(3).trimmed();
    if (nm.isEmpty())
        return false;                       // 单独的 ::: 是闭合标记, 不是开头

    int j = i + 1;
    while (j < end) {
        if (lines.at(j).trimmed() == QStringLiteral(":::")) {
            name = nm;
            next = j + 1;
            return true;
        }
        body << lines.at(j);
        ++j;
    }
    return false;                           // 没闭合就不当容器
}

static bool isBlockStart(const QStringList &lines, int i, int end)
{
    const QString &line = lines.at(i);
    if (isBlank(line))
        return true;
    QChar ch;
    int   len;
    QString info;
    if (matchFence(line, ch, len, info))
        return true;
    int lvl;
    QString txt;
    if (matchAtx(line, lvl, txt))
        return true;
    if (matchHr(line))
        return true;
    QString rest;
    if (matchQuote(line, rest))
        return true;
    Marker m;
    if (matchMarker(line, m))
        return true;
    if (matchHtmlStart(line))
        return true;
    const QString t = line.trimmed();
    if (t.startsWith(QStringLiteral("$$")) || t.startsWith(QStringLiteral("\\[")))
        return true;
    if (t.startsWith(QStringLiteral(":::")))
        return true;
    Q_UNUSED(end)
    return false;
}

// ------------------------------------------------------------
//  块级解析
// ------------------------------------------------------------

static QVector<Block> parseBlocks(const QStringList &lines, int &i, int end);

static Block parseList(const QStringList &lines, int &i, int end)
{
    Block list;
    list.type = Block::List;

    Marker first;
    matchMarker(lines.at(i), first);
    list.ordered = first.ordered;
    list.start   = first.ordered ? first.num : 1;

    const int baseIndent = first.indent;
    bool loose = false;

    while (i < end) {
        // 跳过项之间的空行
        int blanks = 0;
        while (i < end && isBlank(lines.at(i))) {
            ++blanks;
            ++i;
        }
        if (i >= end)
            break;

        Marker m;
        if (!matchMarker(lines.at(i), m))
            break;
        if (m.indent != baseIndent || m.ordered != list.ordered)
            break;
        if (blanks > 0 && !list.children.isEmpty())
            loose = true;

        Block item;
        item.type = Block::ListItem;
        item.num  = m.num;

        QString firstLine = m.text;
        static const QRegularExpression taskRe(QStringLiteral("^\\[([ xX])\\]\\s+(.*)$"));
        const auto tm = taskRe.match(firstLine);
        if (tm.hasMatch()) {
            item.isTask  = true;
            item.checked = (tm.captured(1).compare(QStringLiteral("x"), Qt::CaseInsensitive) == 0);
            firstLine    = tm.captured(2);
        }

        const int contentIndent = qMax(m.contentCol, m.indent + 2);

        QStringList itemLines;
        itemLines << firstLine;
        ++i;

        while (i < end) {
            if (isBlank(lines.at(i))) {
                int j = i;
                while (j < end && isBlank(lines.at(j)))
                    ++j;
                if (j >= end)
                    break;
                Marker nm;
                const bool isM = matchMarker(lines.at(j), nm);
                const int  ind = leadingSpaces(lines.at(j));
                if ((isM && nm.indent == baseIndent) || ind >= contentIndent) {
                    itemLines << QString();
                    ++i;
                    continue;
                }
                break;
            }
            Marker nm;
            const bool isM = matchMarker(lines.at(i), nm);
            if (isM && nm.indent <= baseIndent)
                break;
            const int ind = leadingSpaces(lines.at(i));
            if (ind >= contentIndent || (!isM && ind > baseIndent)) {
                if (itemLines.contains(QString()))
                    loose = true;
                itemLines << stripIndent(lines.at(i), contentIndent);
                ++i;
                continue;
            }
            break;
        }

        int k = 0;
        item.children = parseBlocks(itemLines, k, itemLines.size());
        if (itemLines.contains(QString()))
            loose = true;
        list.children << item;
    }

    list.tight = !loose;
    return list;
}

static QVector<Block> parseBlocks(const QStringList &lines, int &i, int end)
{
    QVector<Block> out;

    while (i < end) {
        const QString &line = lines.at(i);
        if (isBlank(line)) {
            ++i;
            continue;
        }

        // --- 围栏代码块 ---
        {
            QChar   fc;
            int     flen;
            QString finfo;
            if (matchFence(line, fc, flen, finfo)) {
                Block b;
                b.type = Block::CodeBlock;
                b.info = finfo;
                ++i;
                QStringList code;
                while (i < end) {
                    QString cur = lines.at(i);
                    // 判断结束围栏
                    int s = 0;
                    while (s < cur.size() && s < 3 && cur.at(s) == QLatin1Char(' '))
                        ++s;
                    int t = s;
                    while (t < cur.size() && cur.at(t) == fc)
                        ++t;
                    if (t - s >= flen && cur.mid(t).trimmed().isEmpty())
                        break;
                    code << stripIndentMax(cur, 3);
                    ++i;
                }
                if (i < end)
                    ++i; // 吃掉结束围栏
                b.code = code.join(QLatin1Char('\n'));
                out << b;
                continue;
            }
        }

        // --- ATX 标题 ---
        {
            int     lvl;
            QString txt;
            if (matchAtx(line, lvl, txt)) {
                Block b;
                b.type  = Block::Heading;
                b.level = lvl;
                b.text  = txt;
                out << b;
                ++i;
                continue;
            }
        }

        // --- 分割线 ---
        if (matchHr(line)) {
            Block b;
            b.type = Block::Hr;
            out << b;
            ++i;
            continue;
        }

        // --- 引用块 ---
        {
            QString rest;
            if (matchQuote(line, rest)) {
                Block b;
                b.type = Block::Quote;
                QStringList inner;
                while (i < end) {
                    QString r;
                    if (matchQuote(lines.at(i), r)) {
                        inner << r;
                        ++i;
                        continue;
                    }
                    // 惰性续行
                    if (!isBlank(lines.at(i)) && !inner.isEmpty()
                        && !isBlockStart(lines, i, end)) {
                        inner << lines.at(i);
                        ++i;
                        continue;
                    }
                    break;
                }
                int k = 0;
                b.children = parseBlocks(inner, k, inner.size());
                out << b;
                continue;
            }
        }

        // --- 列表 ---
        {
            Marker m;
            if (matchMarker(line, m)) {
                out << parseList(lines, i, end);
                continue;
            }
        }

        // --- 块级公式 ---
        {
            QString tex;
            int     next = 0;
            if (scanMathBlock(lines, i, end, tex, next)) {
                Block b;
                b.type = Block::Math;
                b.code = tex;
                out << b;
                i = next;
                continue;
            }
        }

        // --- 容器 ::: xxx ... ::: ---
        {
            QString     cname;
            QStringList cbody;
            int         next = 0;
            if (scanContainer(lines, i, end, cname, cbody, next)) {
                Block b;
                b.type = Block::Container;
                b.info = cname;
                int k = 0;
                b.children = parseBlocks(cbody, k, cbody.size());
                out << b;
                i = next;
                continue;
            }
        }

        // --- 表格 ---
        if (i + 1 < end && line.contains(QLatin1Char('|'))) {
            QVector<Align> aligns;
            if (isTableDelim(lines.at(i + 1), aligns)) {
                Block b;
                b.type   = Block::Table;
                b.aligns = aligns;
                b.rows << splitTableRow(line);
                i += 2;
                while (i < end && !isBlank(lines.at(i)) && lines.at(i).contains(QLatin1Char('|'))) {
                    b.rows << splitTableRow(lines.at(i));
                    ++i;
                }
                // 列数对齐
                const int cols = aligns.size();
                for (auto &r : b.rows) {
                    while (r.size() < cols)
                        r << QString();
                    while (r.size() > cols)
                        r.removeLast();
                }
                out << b;
                continue;
            }
        }

        // --- HTML 块 ---
        if (matchHtmlStart(line)) {
            Block b;
            b.type = Block::Html;
            QStringList h;
            while (i < end && !isBlank(lines.at(i))) {
                h << lines.at(i);
                ++i;
            }
            b.code = h.join(QLatin1Char('\n'));
            out << b;
            continue;
        }

        // --- 段落 (含 setext 标题) ---
        {
            QStringList para;
            int j = i;
            while (j < end) {
                if (isBlank(lines.at(j)))
                    break;
                if (j > i && isBlockStart(lines, j, end))
                    break;
                para << stripIndentMax(lines.at(j), 3);
                ++j;
            }

            int lvl = 0;
            if (j < end && !para.isEmpty() && matchSetext(lines.at(j), lvl)) {
                Block b;
                b.type  = Block::Heading;
                b.level = lvl;
                b.text  = para.join(QLatin1Char('\n'));
                out << b;
                i = j + 1;
                continue;
            }

            Block b;
            b.type = Block::Paragraph;
            b.text = para.join(QLatin1Char('\n'));
            out << b;
            i = j;
            continue;
        }
    }
    return out;
}

QVector<Block> parse(const QString &markdown)
{
    QString src = markdown;
    src.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    src.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QStringList lines = src.split(QLatin1Char('\n'));
    int i = 0;
    return parseBlocks(lines, i, lines.size());
}

} // namespace md
