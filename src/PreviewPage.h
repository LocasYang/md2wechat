#pragma once

// ============================================================
//  PreviewPage —— 预览页, 额外提供「滚动位置回调」
//
//  预览区跑在 QtWebEngine 里, 网页滚动事件没法直接连到 C++。
//  这里用最省事也最稳的一条通道: 预览页脚本把滚动比例 console.log 出来,
//  重写 javaScriptConsoleMessage 把它接住。不依赖 QWebChannel / 额外脚本文件。
// ============================================================

#include <QWebEnginePage>

class PreviewPage : public QWebEnginePage
{
    Q_OBJECT

public:
    explicit PreviewPage(QObject *parent = nullptr);

signals:
    void scrollRatio(double ratio);   // 0.0 ~ 1.0
    void documentReady();             // 文档加载完并挂上监听

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override;
};
