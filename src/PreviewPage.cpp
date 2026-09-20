#include "PreviewPage.h"

namespace {
const char *kPrefix     = "md2wx:";
const char *kScrollTag  = "md2wx:scroll:";
const char *kReadyTag   = "md2wx:ready";
}

PreviewPage::PreviewPage(QObject *parent)
    : QWebEnginePage(parent)
{
}

void PreviewPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                           const QString &message,
                                           int lineNumber,
                                           const QString &sourceID)
{
    Q_UNUSED(level)

    if (message.startsWith(QLatin1String(kReadyTag))) {
        emit documentReady();
        return;
    }
    if (message.startsWith(QLatin1String(kScrollTag))) {
        bool ok = false;
        const double r = message.mid(int(qstrlen(kScrollTag))).toDouble(&ok);
        if (ok)
            emit scrollRatio(qBound(0.0, r, 1.0));
        return;
    }
    if (message.startsWith(QLatin1String(kPrefix)))
        return;   // 我们自己的消息, 不往外传

    // 其它消息(含 MathJax 的告警)丢掉即可, 避免刷屏
    Q_UNUSED(lineNumber)
    Q_UNUSED(sourceID)
}
