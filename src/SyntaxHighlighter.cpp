#include "SyntaxHighlighter.h"

#include <QColor>
#include <QSet>
#include <QStringList>

namespace md {

CodeColors codeColorsDark()
{
    CodeColors c;
    c.bg      = QStringLiteral("#282c34");
    c.text    = QStringLiteral("#abb2bf");
    c.keyword = QStringLiteral("#c678dd");
    c.string  = QStringLiteral("#98c379");
    c.comment = QStringLiteral("#7f848e");
    c.number  = QStringLiteral("#d19a66");
    c.type    = QStringLiteral("#e5c07b");
    c.func    = QStringLiteral("#61afef");
    c.symbol  = QStringLiteral("#56b6c2");
    return c;
}

CodeColors codeColorsLight()
{
    CodeColors c;
    c.bg      = QStringLiteral("#f6f8fa");
    c.text    = QStringLiteral("#24292f");
    c.keyword = QStringLiteral("#cf222e");
    c.string  = QStringLiteral("#0a3069");
    c.comment = QStringLiteral("#6e7781");
    c.number  = QStringLiteral("#0550ae");
    c.type    = QStringLiteral("#953800");
    c.func    = QStringLiteral("#8250df");
    c.symbol  = QStringLiteral("#0550ae");
    return c;
}

bool isDarkBg(const QString &hexBg)
{
    const QColor c(hexBg);
    if (!c.isValid())
        return true;
    const double lum = (0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue()) / 255.0;
    return lum < 0.5;
}

// ------------------------------------------------------------

namespace {

QString esc(const QString &s)
{
    QString o;
    o.reserve(s.size() + 8);
    for (QChar c : s) {
        if (c == QLatin1Char('&'))
            o += QStringLiteral("&amp;");
        else if (c == QLatin1Char('<'))
            o += QStringLiteral("&lt;");
        else if (c == QLatin1Char('>'))
            o += QStringLiteral("&gt;");
        else
            o += c;
    }
    return o;
}

struct Spec
{
    int         mode = 0; // 0=类 C, 1=标记语言, 2=纯文本
    QStringList lineComments;
    bool        blockComment = false;
    bool        preproc      = false;
    bool        hashComment  = false; // python 风格 # 注释
    bool        tripleQuote  = false; // python 三引号
    bool        backtick     = false; // js 模板串
    QSet<QString> kw;
    QSet<QString> types;
};

Spec specFor(const QString &langIn)
{
    Spec s;
    const QString lang = langIn.trimmed().toLower();

    // ---- 纯文本 ----
    if (lang.isEmpty() || lang == QLatin1String("text") || lang == QLatin1String("plain")
        || lang == QLatin1String("txt") || lang == QLatin1String("log")
        || lang == QLatin1String("output")) {
        s.mode = 2;
        return s;
    }

    // ---- 标记语言 ----
    if (lang == QLatin1String("html") || lang == QLatin1String("xml") || lang == QLatin1String("vue")
        || lang == QLatin1String("svg") || lang == QLatin1String("markup") || lang == QLatin1String("xhtml")) {
        s.mode = 1;
        return s;
    }

    // ---- 井号注释家族 ----
    if (lang == QLatin1String("python") || lang == QLatin1String("py")) {
        s.hashComment = true;
        s.tripleQuote = true;
        s.kw = QSet<QString>({
            "and","as","assert","async","await","break","class","continue","def","del","elif",
            "else","except","finally","for","from","global","if","import","in","is","lambda",
            "nonlocal","not","or","pass","raise","return","try","while","with","yield","match","case"
        });
        s.types = QSet<QString>({
            "True","False","None","self","cls","int","str","float","list","dict","set","tuple",
            "bool","bytes","object","print","len","range","super","enumerate","zip","Exception"
        });
        return s;
    }
    if (lang == QLatin1String("yaml") || lang == QLatin1String("yml") || lang == QLatin1String("toml")
        || lang == QLatin1String("ini") || lang == QLatin1String("conf") || lang == QLatin1String("properties")
        || lang == QLatin1String("makefile") || lang == QLatin1String("dockerfile")
        || lang == QLatin1String("cmake") || lang == QLatin1String("nginx")
        || lang == QLatin1String("r") || lang == QLatin1String("perl") || lang == QLatin1String("pl")) {
        s.hashComment = true;
        return s;
    }
    if (lang == QLatin1String("bash") || lang == QLatin1String("sh") || lang == QLatin1String("shell")
        || lang == QLatin1String("zsh") || lang == QLatin1String("console")) {
        s.hashComment = true;
        s.kw = QSet<QString>({
            "if","then","else","elif","fi","for","while","do","done","case","esac","function",
            "in","select","until","return","break","continue","local","export","source","echo",
            "exit","set","unset","read","cd","printf","sudo","grep","awk","sed","cat","touch"
        });
        return s;
    }
    if (lang == QLatin1String("powershell") || lang == QLatin1String("ps1")) {
        s.hashComment = true;
        return s;
    }
    if (lang == QLatin1String("ruby") || lang == QLatin1String("rb")) {
        s.hashComment = true;
        s.kw = QSet<QString>({
            "def","end","if","elsif","else","unless","while","until","for","in","do","class",
            "module","return","yield","begin","rescue","ensure","require","require_relative","puts","new"
        });
        return s;
    }

    // ---- SQL ----
    if (lang == QLatin1String("sql") || lang == QLatin1String("mysql") || lang == QLatin1String("postgresql")) {
        s.lineComments = QStringList{QStringLiteral("--")};
        s.blockComment = true;
        s.kw = QSet<QString>({
            "SELECT","FROM","WHERE","INSERT","INTO","VALUES","UPDATE","SET","DELETE","CREATE",
            "TABLE","ALTER","DROP","INDEX","JOIN","LEFT","RIGHT","INNER","OUTER","ON","GROUP",
            "BY","ORDER","HAVING","LIMIT","OFFSET","AND","OR","NOT","NULL","AS","DISTINCT","UNION",
            "COUNT","SUM","AVG","MIN","MAX","PRIMARY","KEY","FOREIGN","REFERENCES","DEFAULT"
        });
        for (const QString &k : QSet<QString>(s.kw)) {
            s.types << k.toLower();
        }
        return s;
    }

    // ---- CSS ----
    if (lang == QLatin1String("css") || lang == QLatin1String("scss") || lang == QLatin1String("less")) {
        s.blockComment = true;
        return s;
    }

    // ---- JSON ----
    if (lang == QLatin1String("json") || lang == QLatin1String("json5")) {
        s.kw = QSet<QString>({"true","false","null"});
        return s;
    }

    // ---- 类 C 语言 (默认) ----
    s.lineComments = QStringList{QStringLiteral("//")};
    s.blockComment = true;
    s.preproc      = true;
    s.backtick     = (lang == QLatin1String("js") || lang == QLatin1String("javascript")
                      || lang == QLatin1String("ts") || lang == QLatin1String("typescript")
                      || lang == QLatin1String("jsx") || lang == QLatin1String("tsx"));

    QSet<QString> kw {
        "if","else","for","while","do","switch","case","default","break","continue","return",
        "class","struct","enum","union","namespace","using","public","private","protected",
        "static","const","constexpr","volatile","mutable","virtual","override","final","new",
        "delete","this","super","try","catch","finally","throw","throws","extends","implements",
        "interface","package","import","export","from","as","async","await","yield","function",
        "var","let","def","lambda","with","pass","raise","global","in","is","not","and","or",
        "template","typename","typedef","sizeof","extern","inline","friend","operator","explicit",
        "auto","void","fn","let","mut","pub","crate","impl","trait","match","loop","mod","where",
        "goto","register","signed","unsigned","short","long","do","then","end","begin","record"
    };
    QSet<QString> ty {
        "int","char","float","double","bool","boolean","short","long","void","string","String",
        "wchar_t","size_t","int8_t","int16_t","int32_t","int64_t","uint8_t","uint16_t","uint32_t",
        "uint64_t","true","false","null","nullptr","NULL","undefined","NaN","Infinity",
        "self","None","True","False","i8","i16","i32","i64","u8","u16","u32","u64","f32","f64",
        "usize","isize","str","Vec","Option","Result","any","never","unknown","object","number"
    };
    s.kw    = kw;
    s.types = ty;
    return s;
}

QString span(const QString &cls, const QString &text, const CodeColors &c)
{
    QString color = c.text;
    if (cls == QLatin1String("k"))        color = c.keyword;
    else if (cls == QLatin1String("s"))   color = c.string;
    else if (cls == QLatin1String("c"))   color = c.comment;
    else if (cls == QLatin1String("n"))   color = c.number;
    else if (cls == QLatin1String("t"))   color = c.type;
    else if (cls == QLatin1String("f"))   color = c.func;
    else if (cls == QLatin1String("o"))   color = c.symbol;
    if (text.isEmpty())
        return QString();
    if (cls.isEmpty())
        return esc(text);
    return QStringLiteral("<span style=\"color:%1\">%2</span>").arg(color, esc(text));
}

bool isIdentStart(QChar c)
{
    return c.isLetter() || c == QLatin1Char('_') || c == QLatin1Char('$');
}
bool isIdentChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('$');
}

bool startsWithAt(const QString &s, int i, const QString &needle)
{
    return s.midRef(i, needle.size()) == needle;
}

} // namespace

QString highlightCode(const QString &code, const QString &lang, const CodeColors &c)
{
    const Spec sp = specFor(lang);
    if (sp.mode == 2)
        return esc(code);

    QString out;
    out.reserve(code.size() * 2);

    const int n = code.size();
    int i = 0;
    bool lineStart = true;

    // ---- 标记语言 ----
    if (sp.mode == 1) {
        while (i < n) {
            if (startsWithAt(code, i, QStringLiteral("<!--"))) {
                int e = code.indexOf(QStringLiteral("-->"), i + 4);
                e = (e < 0) ? n : e + 3;
                out += span(QStringLiteral("c"), code.mid(i, e - i), c);
                i = e;
                continue;
            }
            if (code.at(i) == QLatin1Char('<')) {
                int start = i;
                ++i;
                out += span(QStringLiteral("o"), QStringLiteral("<"), c);
                if (i < n && code.at(i) == QLatin1Char('/')) {
                    out += span(QStringLiteral("o"), QStringLiteral("/"), c);
                    ++i;
                }
                int id0 = i;
                while (i < n && (code.at(i).isLetterOrNumber() || code.at(i) == QLatin1Char('-')
                                 || code.at(i) == QLatin1Char(':') || code.at(i) == QLatin1Char('_')))
                    ++i;
                out += span(QStringLiteral("k"), code.mid(id0, i - id0), c);
                // 属性区
                int guard = 0;
                while (i < n && code.at(i) != QLatin1Char('>') && guard < 100000) {
                    ++guard;
                    const QChar ch = code.at(i);
                    if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
                        const QChar q = ch;
                        int e = i + 1;
                        while (e < n && code.at(e) != q)
                            ++e;
                        e = qMin(e + 1, n);
                        out += span(QStringLiteral("s"), code.mid(i, e - i), c);
                        i = e;
                    } else if (isIdentStart(ch)) {
                        int a = i;
                        while (i < n && (isIdentChar(code.at(i)) || code.at(i) == QLatin1Char('-')
                                         || code.at(i) == QLatin1Char(':')))
                            ++i;
                        out += span(QStringLiteral("t"), code.mid(a, i - a), c);
                    } else {
                        out += span(QStringLiteral("o"), QString(ch), c);
                        ++i;
                    }
                }
                if (i < n && code.at(i) == QLatin1Char('>')) {
                    out += span(QStringLiteral("o"), QStringLiteral(">"), c);
                    ++i;
                }
                Q_UNUSED(start)
                continue;
            }
            out += esc(QString(code.at(i)));
            ++i;
        }
        return out;
    }

    // ---- 类 C / 脚本语言 ----
    while (i < n) {
        const QChar ch = code.at(i);

        if (ch == QLatin1Char('\n')) {
            out += QLatin1Char('\n');
            ++i;
            lineStart = true;
            continue;
        }

        // 行注释
        bool matchedLine = false;
        for (const QString &lc : sp.lineComments) {
            if (startsWithAt(code, i, lc)) {
                int e = code.indexOf(QLatin1Char('\n'), i);
                e = (e < 0) ? n : e;
                out += span(QStringLiteral("c"), code.mid(i, e - i), c);
                i = e;
                matchedLine = true;
                break;
            }
        }
        if (matchedLine)
            continue;

        if (sp.hashComment && ch == QLatin1Char('#')) {
            int e = code.indexOf(QLatin1Char('\n'), i);
            e = (e < 0) ? n : e;
            out += span(QStringLiteral("c"), code.mid(i, e - i), c);
            i = e;
            continue;
        }

        // 预处理指令
        if (sp.preproc && ch == QLatin1Char('#') && lineStart) {
            int e = code.indexOf(QLatin1Char('\n'), i);
            e = (e < 0) ? n : e;
            out += span(QStringLiteral("k"), code.mid(i, e - i), c);
            i = e;
            continue;
        }

        // 块注释
        if (sp.blockComment && startsWithAt(code, i, QStringLiteral("/*"))) {
            int e = code.indexOf(QStringLiteral("*/"), i + 2);
            e = (e < 0) ? n : e + 2;
            out += span(QStringLiteral("c"), code.mid(i, e - i), c);
            i = e;
            continue;
        }

        // 字符串
        if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')
            || (sp.backtick && ch == QLatin1Char('`'))) {
            // Python 三引号
            if (sp.tripleQuote && startsWithAt(code, i, QString(ch).repeated(3))) {
                const QString q3 = QString(ch).repeated(3);
                int e = code.indexOf(q3, i + 3);
                e = (e < 0) ? n : e + 3;
                out += span(QStringLiteral("s"), code.mid(i, e - i), c);
                i = e;
                continue;
            }
            const QChar q = ch;
            int e = i + 1;
            while (e < n) {
                if (code.at(e) == QLatin1Char('\\')) {
                    e += 2;
                    continue;
                }
                if (code.at(e) == q)
                    break;
                if (code.at(e) == QLatin1Char('\n') && q != QLatin1Char('`'))
                    break;
                ++e;
            }
            e = qMin(e + 1, n);
            out += span(QStringLiteral("s"), code.mid(i, e - i), c);
            i = e;
            continue;
        }

        // 数字
        if (ch.isDigit() || (ch == QLatin1Char('.') && i + 1 < n && code.at(i + 1).isDigit())) {
            int e = i;
            while (e < n) {
                const QChar x = code.at(e);
                if (x.isLetterOrNumber() || x == QLatin1Char('.') || x == QLatin1Char('_'))
                    ++e;
                else if ((x == QLatin1Char('+') || x == QLatin1Char('-'))
                         && e > i && (code.at(e - 1) == QLatin1Char('e') || code.at(e - 1) == QLatin1Char('E')))
                    ++e;
                else
                    break;
            }
            out += span(QStringLiteral("n"), code.mid(i, e - i), c);
            i = e;
            continue;
        }

        // 标识符
        if (isIdentStart(ch)) {
            int e = i;
            while (e < n && isIdentChar(code.at(e)))
                ++e;
            const QString id = code.mid(i, e - i);
            QString cls;
            if (sp.kw.contains(id))
                cls = QStringLiteral("k");
            else if (sp.types.contains(id))
                cls = QStringLiteral("t");
            else {
                int k = e;
                while (k < n && code.at(k) == QLatin1Char(' '))
                    ++k;
                if (k < n && code.at(k) == QLatin1Char('('))
                    cls = QStringLiteral("f");
            }
            out += span(cls, id, c);
            i = e;
            lineStart = false;
            continue;
        }

        // 符号
        static const QString syms = QStringLiteral("+-*/%=<>!&|^~?:;,.()[]{}\\@");
        if (syms.contains(ch)) {
            out += span(QStringLiteral("o"), QString(ch), c);
        } else {
            out += esc(QString(ch));
        }
        ++i;
        if (!ch.isSpace())
            lineStart = false;
    }
    return out;
}

} // namespace md
