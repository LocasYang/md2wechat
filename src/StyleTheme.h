#pragma once

// ============================================================
//  StyleTheme —— 微信公众号「排版样式」预设
//  每个主题只提供颜色 / 圆角 / 标题装饰方式, 渲染器据此生成内联样式
// ============================================================

#include <QString>
#include <QVector>

#include "SyntaxHighlighter.h"

namespace md {

struct StyleTheme
{
    QString id;          // 标识
    QString name;        // 名称
    QString desc;        // 一句话描述
    QString swatch;      // 卡片主色(用于 UI 预览)

    // ---- 正文色板 ----
    QString pageBg;      // 正文卡片底色
    QString canvasBg;    // 卡片外画布底色
    QString text;        // 正文文字
    QString textStrong;  // 加粗/标题文字
    QString textLight;   // 次要文字
    QString link;        // 链接
    QString accent;      // 主强调色
    QString accentSoft;  // 主色浅底
    QString accentDark;  // 主色深色
    QString border;      // 常规边框
    QString bgSoft;      // 浅灰底

    // ---- 标题装饰 ----
    // 0 = 左侧色条  1 = 底色块  2 = 居中下划线  3 = 左条+浅底
    int h2Style = 0;
    int h3Style = 0;

    // ---- 引用 ----
    QString quoteBg;
    QString quoteBorder;
    QString quoteText;

    // ---- 代码 ----
    CodeColors code;
    bool codeDark = true;
    QString codeHeaderBg;

    // ---- 表格 ----
    QString thBg;
    QString thText;
    QString tdBorder;

    // ---- 图片 ----
    QString imgRadius = QStringLiteral("6px");
    QString imgCaptionColor;

    QString fontFamily;
};

// 内置主题列表
const QVector<StyleTheme> &styleThemes();
// 按 id 取主题, 找不到返回第一个
const StyleTheme &styleThemeById(const QString &id);

} // namespace md
