#include <QApplication>
#include <QFont>

#include "AntTheme.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    // QtWebEngine 要求: 必须在 QApplication 之前设置共享 OpenGL 上下文
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("MD2WeChat"));
    app.setOrganizationName(QStringLiteral("MD2WeChat"));
    app.setApplicationDisplayName(QStringLiteral("MD2WeChat"));

    QFont f = app.font();
    f.setFamily(QStringLiteral("Microsoft YaHei UI"));
    f.setPixelSize(14);
    app.setFont(f);

    ant::setDarkMode(true);
    app.setStyleSheet(ant::buildStyleSheet(ant::tokens(true)));

    MainWindow w;

    // 开发期自截图: --shot <png> [--size WxH]
    const QStringList args = app.arguments();

    const int si = args.indexOf(QStringLiteral("--size"));
    if (si >= 0 && si + 1 < args.size()) {
        const QStringList wh = args.at(si + 1).split(QLatin1Char('x'));
        if (wh.size() == 2 && wh.at(0).toInt() > 0 && wh.at(1).toInt() > 0)
            w.resize(wh.at(0).toInt(), wh.at(1).toInt());
    }

    w.show();

    // 命令行直接打开: --open <md 文件>
    const int oi = args.indexOf(QStringLiteral("--open"));
    if (oi >= 0 && oi + 1 < args.size())
        w.openFile(args.at(oi + 1));

    const int idx = args.indexOf(QStringLiteral("--shot"));
    if (idx >= 0 && idx + 1 < args.size())
        w.setShotPath(args.at(idx + 1));

    // 开发期: 复制一次后退出, 便于核对剪贴板内容
    if (args.contains(QStringLiteral("--copy")))
        w.setCopyTest();

    return app.exec();
}
