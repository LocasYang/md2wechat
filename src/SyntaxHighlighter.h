#pragma once

// ============================================================
//  SyntaxHighlighter —— 代码块语法着色
//  只包裹 <span>, 不增删任何一个字符, 保证代码内容原样保留
// ============================================================

#include <QString>

namespace md {

// 代码着色配色
struct CodeColors
{
    QString bg;
    QString text;
    QString keyword;
    QString string;
    QString comment;
    QString number;
    QString type;
    QString func;
    QString symbol;
};

// 常见浅色 / 深色配色
CodeColors codeColorsDark();
CodeColors codeColorsLight();

// 对代码做着色, 返回 HTML 片段(已转义, 带内联样式 span)
QString highlightCode(const QString &code, const QString &lang, const CodeColors &c);

// 是否为深色配色
bool isDarkBg(const QString &hexBg);

} // namespace md
