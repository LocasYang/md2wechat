#pragma once

// ============================================================
//  MarkdownParser —— 轻量 Markdown 块级解析器
//  设计原则: 只做结构识别, 不做任何内容改写, 保证原文一字不差
// ============================================================

#include <QString>
#include <QStringList>
#include <QVector>

namespace md {

// 表格对齐方式
enum class Align { None, Left, Center, Right };

// 块级节点
struct Block
{
    enum Type {
        Paragraph,   // 段落
        Heading,     // 标题 (level: 1-6)
        CodeBlock,   // 围栏代码块 (info / code)
        Quote,       // 引用块
        List,        // 列表 (ordered / start / children = 若干 ListItem)
        ListItem,    // 列表项 (children 存放项内块级内容)
        Table,       // 表格 (rows / aligns)
        Hr,          // 分割线
        Math,        // 块级公式 (code = 原始 TeX)
        Container,   // ::: hljs-center 之类的容器 (info = 容器名)
        Html         // 原始 HTML 块
    };

    Type type = Paragraph;
    int  level = 0;            // 标题层级
    QString text;              // 段落 / 标题的原始行内文本(未解析)
    QString info;              // 代码块语言标记
    QString code;              // 代码块原始内容
    QVector<Block> children;   // 引用块内容 / 列表项
    bool ordered = false;      // 有序列表
    int  start   = 1;          // 有序列表起始序号
    int  num     = 0;          // 列表项自身序号
    bool tight   = true;       // 紧凑列表(项内不裹 <p>)
    bool isTask  = false;      // 任务列表项
    bool checked = false;      // 任务勾选状态
    QVector<QStringList> rows; // 表格: 首行为表头
    QVector<Align> aligns;     // 表格列对齐
};

// 解析入口
QVector<Block> parse(const QString &markdown);

// ---- 供渲染器复用的行内解析辅助 ----
// 判断一个字符是否为 Markdown 可转义标点
bool isEscapable(QChar c);
// 把纯文本转义为 HTML 安全文本(保留已有实体)
QString escapeHtml(const QString &text, bool preserveEntities = true);
// 把纯文本转义为 HTML 属性值
QString escapeAttr(const QString &text);

} // namespace md
