#pragma once

// ============================================================
//  WeChatRenderer —— 把 Markdown 渲染成「公众号友好」的 HTML
//
//  关键点: 微信公众号编辑器会清掉 <style> / class / 外部样式,
//          只保留标签上的内联 style。所以这里所有样式一律内联,
//          并且统一使用 <section> 作为容器。
// ============================================================

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

#include "MarkdownParser.h"
#include "StyleTheme.h"

namespace md {

// ---- 公式渲染结果(一张 PNG) ----
struct MathImage
{
    QString dataUri;      // data:image/png;base64,...
    double  w     = 0;    // 显示宽度(CSS px)
    double  h     = 0;    // 显示高度(CSS px)
    double  depth = 0;    // 基线以下深度(CSS px), 用于行内公式对齐
};

// ---- 公式缓存 ----
// 渲染时查询缓存; 未命中的会登记到 pending, 由界面负责用 MathJax 转换后回填
struct MathCache
{
    QHash<QString, MathImage> images;
    QSet<QString>             failed;      // 转换失败, 直接退回文本显示
    mutable QStringList       pending;     // 本次渲染未命中的 key

    const MathImage *find(const QString &key) const
    {
        const auto it = images.constFind(key);
        return it == images.constEnd() ? nullptr : &it.value();
    }
    void require(const QString &key) const
    {
        if (!images.contains(key) && !failed.contains(key) && !pending.contains(key))
            pending << key;
    }
};

QString mathKey(const QString &tex, bool display, const QString &color);
QString mathTexFromKey(const QString &key);
bool    mathDisplayFromKey(const QString &key);
QString mathColorFromKey(const QString &key);

struct RenderOptions
{
    QString themeId           = QStringLiteral("classic");
    int     fontSize          = 15;      // 正文字号
    double  lineHeight        = 1.75;    // 行高倍数
    bool    syntaxHighlight   = true;    // 代码高亮
    bool    allowRawHtml      = true;    // 允许原文中的 HTML 片段直通
    bool    autoLink          = true;    // 裸链接自动识别
    bool    inlineLocalImages = true;    // 本地图片内联为 base64(便于预览)
    // 远程图片也内联为 base64: 微信抓不到外链(比如对方 Content-Type 不对)时用这个绕过。
    // 默认开启 —— 保证粘贴不依赖外链可达性; 剪贴板会变大, 可在左侧关掉。
    bool    inlineRemoteImages = true;
    QHash<QString, QByteArray> *remoteCache = nullptr;   // url -> 图片字节(由界面预下载)
    bool    mathToImage       = true;    // 公式转图片(公众号唯一可行方式)
    bool    codeWrap          = false;   // 代码块自动换行(移动端友好)
    bool    showImageAlt      = true;    // 图片 alt 作为图注(文件名形式的会自动跳过)
    QString basePath;                    // md 文件所在目录, 用于解析相对图片路径
    MathCache *math           = nullptr; // 公式缓存, 可为空
};

struct RenderResult
{
    QString body;          // 公众号正文 HTML
    QString document;      // 预览用完整 HTML
    int     images      = 0;
    int     chars       = 0;
    int     paragraphs  = 0;
    int     readMinutes = 1;
};

// 主入口
RenderResult renderMarkdown(const QString &source, const RenderOptions &opt);

// 生成「复制到公众号」用的 HTML(含 section 根包裹)
QString toWeChatHtml(const RenderResult &r, const StyleTheme &t, int fontSize = 15);

// 把生成的 HTML 转成可读纯文本(剪贴板兜底格式, 不能再放 Markdown 原文)
QString htmlToPlainText(const QString &html);

// 生成预览用完整 HTML 文档
//   mathScriptUrl     非空时引入 MathJax 并注入公式转 PNG 的辅助函数
//   withScrollBridge  true 时注入滚动位置上报脚本(供编辑器/预览滚动同步)
QString toPreviewDocument(const QString &wechatHtml, const StyleTheme &t, bool darkCanvas,
                          const QString &mathScriptUrl = QString(),
                          bool withScrollBridge = false);

// 预览页里用于把占位符批量转成 PNG 的 JS 函数名
QString mathTypesetFunctionName();

} // namespace md
