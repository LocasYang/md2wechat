#include "MainWindow.h"

#include "AntWidgets.h"
#include "PreviewPage.h"
#include "StyleTheme.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOpenGLWidget>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPlainTextEdit>
#include <QQuickWidget>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSharedPointer>
#include <QSlider>
#include <QSplitter>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QWindow>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

// 把字符串变成安全的 JS 字面量
QString jsQuote(const QString &s)
{
    QJsonArray arr;
    arr.append(s);
    const QString j = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    return j.mid(1, j.size() - 2);
}

QLabel *makeLabel(const QString &text, const char *objectName, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(QString::fromLatin1(objectName));
    return l;
}

// 开发期诊断日志(--shot 模式下写 <png>.log), 正常使用时不产生任何文件
void shotLog(const QString &path, const QString &line)
{
    if (path.isEmpty())
        return;
    QFile f(path + QStringLiteral(".log"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        f.write(line.toUtf8());
        f.write("\n");
    }
}

// ============================================================
//  剪贴板: 自己拼 CF_HTML 直接写 Windows 剪贴板
//
//  不用 QApplication::clipboard()->setMimeData() 的原因:
//  它在 Windows 上生成的 CF_HTML 头(StartHTML/EndHTML 等)是"字节偏移",
//  一旦内容含中文就非常容易算错, 接收方解析失败后只能退回纯文本格式 ——
//  粘到公众号里就变成一堆 Markdown 记号。
//  这里自己拼, 偏移一律按 UTF-8 字节数算, 可控。
// ============================================================

#ifdef Q_OS_WIN

bool writeRichClipboard(const QString &html, const QString &plainText)
{
    const QByteArray frag = html.toUtf8();

    QByteArray prefix =
        "Version:1.0\r\n"
        "StartHTML:0000000000\r\n"
        "EndHTML:0000000000\r\n"
        "StartFragment:0000000000\r\n"
        "EndFragment:0000000000\r\n";
    const QByteArray open  = "<html><body>\r\n<!--StartFragment-->\r\n";
    const QByteArray close = "\r\n<!--EndFragment-->\r\n</body></html>\r\n";

    const int startHtml = prefix.size();
    const int startFrag = startHtml + open.size();
    const int endFrag   = startFrag + frag.size();
    const int endHtml   = endFrag + close.size();

    auto patch = [](QByteArray &b, const char *key, int v) {
        const int i = b.indexOf(key);
        if (i < 0)
            return;
        b.replace(i + int(qstrlen(key)), 10,
                  QByteArray::number(v).rightJustified(10, '0'));
    };
    patch(prefix, "StartHTML:", startHtml);
    patch(prefix, "EndHTML:", endHtml);
    patch(prefix, "StartFragment:", startFrag);
    patch(prefix, "EndFragment:", endFrag);

    const QByteArray cfHtml = prefix + open + frag + close;

    if (!::OpenClipboard(nullptr))
        return false;
    ::EmptyClipboard();

    bool ok = false;

    const UINT fmtHtml = ::RegisterClipboardFormatW(L"HTML Format");
    if (HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE, SIZE_T(cfHtml.size() + 1))) {
        if (void *p = ::GlobalLock(h)) {
            memcpy(p, cfHtml.constData(), SIZE_T(cfHtml.size() + 1));
            ::GlobalUnlock(h);
            if (::SetClipboardData(fmtHtml, h))
                ok = true;
        }
    }

    // 纯文本兜底: 放渲染后的可读文本, 绝不放 Markdown 原文
    const int chars = plainText.size();
    if (HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE, SIZE_T((chars + 1) * sizeof(wchar_t)))) {
        if (void *p = ::GlobalLock(h)) {
            memcpy(p, plainText.utf16(), SIZE_T(chars) * sizeof(wchar_t));
            static_cast<wchar_t *>(p)[chars] = 0;
            ::GlobalUnlock(h);
            ::SetClipboardData(CF_UNICODETEXT, h);
        }
    }

    ::CloseClipboard();
    return ok;
}

#else
bool writeRichClipboard(const QString &html, const QString &plainText)
{
    auto *mime = new QMimeData;
    mime->setHtml(html);
    mime->setText(plainText);
    QApplication::clipboard()->setMimeData(mime);
    return true;
}
#endif

} // namespace

// ============================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("MD2WeChat · 微信公众号排版器"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
    setAcceptDrops(true);
    setMinimumSize(880, 560);
    setWindowIcon(QIcon(QStringLiteral(":/icons/logo.svg")));

    // 初始尺寸按屏幕可用区域推算: 小屏 / 高缩放比下不会超出屏幕
    QRect avail(0, 0, 1440, 900);
    if (QScreen *sc = QGuiApplication::primaryScreen())
        avail = sc->availableGeometry();
    const int w0 = qBound(880, int(avail.width() * 0.90), 1440);
    const int h0 = qBound(560, int(avail.height() * 0.90), 920);
    resize(w0, h0);
    move(avail.x() + (avail.width() - w0) / 2,
         avail.y() + (avail.height() - h0) / 2);

    buildUi();
    applyTheme();

    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(320);
    connect(m_renderTimer, &QTimer::timeout, this, &MainWindow::doRender);

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    m_statusTimer->setInterval(2600);
    connect(m_statusTimer, &QTimer::timeout, this, [this]() {
        if (m_lblFile)
            m_lblFile->setText(m_filePath.isEmpty() ? QStringLiteral("未打开文件")
                                                    : QDir::toNativeSeparators(m_filePath));
    });

    // ---- 公式转换 ----
    m_opt.math = &m_mathCache;

    // 远程图片缓存: 开启「远程图片转 Base64」后由 scanRemoteImages 预下载
    m_net = new QNetworkAccessManager(this);
    m_opt.remoteCache = &m_remoteImages;

    // 内置 MathJax 落到临时目录, 预览页用 file:// 引入
    // (内联进 HTML 会因脚本内的特殊序列破坏文档解析; qrc 在 WebEngine 里也不稳)
    {
        const QString dst = QDir(QDir::tempPath())
                                .filePath(QStringLiteral("md2wx-mathjax-3.2.2.js"));
        const QFileInfo fi(dst);
        if (!fi.exists() || fi.size() < 100000) {
            QFile src(QStringLiteral(":/vendor/mathjax/tex-svg.js"));
            if (src.open(QIODevice::ReadOnly)) {
                QFile out(dst);
                if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    out.write(src.readAll());
                    out.close();
                }
                src.close();
            }
        }
        if (QFileInfo::exists(dst))
            m_mathScriptUrl = QUrl::fromLocalFile(dst).toString();
    }

    m_mathPollTimer = new QTimer(this);
    m_mathPollTimer->setInterval(150);
    connect(m_mathPollTimer, &QTimer::timeout, this, &MainWindow::pollMath);

    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (ok)
            maybeStartMath();
    });

    setSource(sampleMarkdown());
}

MainWindow::~MainWindow() = default;

// ============================================================
//  UI 构建
// ============================================================

void MainWindow::buildUi()
{
    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("antRoot"));
    setCentralWidget(root);

    auto *outer = new QVBoxLayout(root);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ---------------- 标题栏 ----------------
    m_caption = new ant::CaptionBar(QStringLiteral("MD2WeChat"),
                                    QStringLiteral("Markdown → 微信公众号排版"), root);
    outer->addWidget(m_caption);

    connect(m_caption, &ant::CaptionBar::dragStarted, this, [this]() {
        if (windowHandle() && !m_maximized)
            windowHandle()->startSystemMove();
    });
    connect(m_caption, &ant::CaptionBar::toggleMaximize, this, &MainWindow::onToggleMaximize);
    connect(m_caption, &ant::CaptionBar::minimizeRequested, this, &MainWindow::showMinimized);
    connect(m_caption, &ant::CaptionBar::closeRequested, this, &MainWindow::close);

    {
        auto *extra = m_caption->extraArea();
        auto *el    = qobject_cast<QHBoxLayout *>(extra->layout());
        auto *lbl   = makeLabel(QStringLiteral("深色界面"), "antHint", extra);
        auto *sw    = new ant::Switch(extra);
        sw->setChecked(m_darkChrome);
        connect(sw, &ant::Switch::toggled, this, &MainWindow::onToggleDarkMode);
        el->addWidget(lbl);
        el->addWidget(sw);
    }

    // ---------------- 工具栏 ----------------
    {
        auto *bar = new QWidget(root);
        bar->setObjectName(QStringLiteral("antToolbar"));
        bar->setFixedHeight(54);
        auto *bl = new QHBoxLayout(bar);
        bl->setContentsMargins(16, 0, 16, 0);
        bl->setSpacing(8);

        auto *bOpen = new ant::Button(QStringLiteral("打开 Markdown"), QStringLiteral("default"), bar);
        auto *bPaste = new ant::Button(QStringLiteral("从剪贴板粘贴"), QStringLiteral("default"), bar);
        auto *bSample = new ant::Button(QStringLiteral("示例"), QStringLiteral("text"), bar);
        auto *bClear = new ant::Button(QStringLiteral("清空"), QStringLiteral("text"), bar);

        bOpen->setProperty("antIcon", QStringLiteral("folder"));
        bPaste->setProperty("antIcon", QStringLiteral("clipboard"));
        bSample->setProperty("antIcon", QStringLiteral("file-text"));
        bClear->setProperty("antIcon", QStringLiteral("trash"));

        auto *sep = new QWidget(bar);
        sep->setFixedSize(1, 22);
        sep->setStyleSheet(QStringLiteral("background:%1;").arg(ant::tokens(true).colorBorder));
        sep->setObjectName(QStringLiteral("antSep"));

        m_btnCopy = new ant::Button(QStringLiteral("复制到公众号"), QStringLiteral("primary"), bar);
        m_btnCopy->setMinimumHeight(32);
        m_btnCopy->setProperty("antIcon", QStringLiteral("copy"));
        m_btnCopy->setProperty("antIconTone", QStringLiteral("inverse"));
        m_btnCopy->setToolTip(QStringLiteral(
            "把排版好的文章复制到剪贴板\n\n"
            "① 点这个按钮\n"
            "② 打开 mp.weixin.qq.com，新建图文消息\n"
            "③ 在正文区按 Ctrl+V 粘贴"));

        auto *bExport = new ant::Button(QStringLiteral("导出 HTML"), QStringLiteral("default"), bar);
        bExport->setProperty("antIcon", QStringLiteral("download"));

        auto *bMp = new ant::Button(QStringLiteral("公众号后台"), QStringLiteral("text"), bar);
        bMp->setProperty("antIcon", QStringLiteral("external"));

        bl->addWidget(bOpen);
        bl->addWidget(bPaste);
        bl->addWidget(bSample);
        bl->addWidget(bClear);
        bl->addWidget(sep);
        bl->addSpacing(4);
        bl->addWidget(m_btnCopy);
        bl->addWidget(bExport);
        bl->addWidget(bMp);
        bl->addStretch(1);

        auto *tip = makeLabel(QStringLiteral("编辑左侧 Markdown · 右侧实时预览 · 一键复制后粘贴到公众号编辑器"),
                              "antHint", bar);
        tip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        m_toolbarHint = tip;
        bl->addWidget(tip);

        outer->addWidget(bar);

        connect(bOpen, &ant::Button::clicked, this, &MainWindow::onOpenFile);
        connect(bSample, &ant::Button::clicked, this, &MainWindow::onLoadSample);
        connect(bClear, &ant::Button::clicked, this, &MainWindow::onClearSource);
        connect(m_btnCopy, &ant::Button::clicked, this, &MainWindow::onCopyToWeChat);
        connect(bExport, &ant::Button::clicked, this, &MainWindow::onExportHtml);
        connect(bMp, &ant::Button::clicked, this, &MainWindow::onOpenMpHome);
        connect(bPaste, &ant::Button::clicked, this, [this]() {
            const QString txt = QApplication::clipboard()->text();
            if (txt.trimmed().isEmpty()) {
                flashStatus(QStringLiteral("剪贴板里没有文本内容"));
                return;
            }
            m_editor->setPlainText(txt);
            m_filePath.clear();
            flashStatus(QStringLiteral("已粘贴剪贴板内容"));
        });
    }

    // ---------------- 主体三栏 ----------------
    {
        auto *body = new QWidget(root);
        auto *bl   = new QHBoxLayout(body);
        bl->setContentsMargins(12, 12, 12, 10);
        bl->setSpacing(0);

        m_splitter = new QSplitter(Qt::Horizontal, body);
        m_splitter->setChildrenCollapsible(false);
        m_splitter->setHandleWidth(12);
        bl->addWidget(m_splitter);

        outer->addWidget(body, 1);

        // ============ 左: 排版设置 ============
        {
            auto *card = new ant::Card(m_splitter);
            auto *cl   = new QVBoxLayout(card);
            // 右边距留小一点, 让滚动条尽量贴到卡片右侧
            cl->setContentsMargins(14, 12, 4, 14);
            cl->setSpacing(14);

            auto *scroll = new QScrollArea(card);
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

            auto *inner = new QWidget(scroll);
            inner->setObjectName(QStringLiteral("antPane"));
            auto *il = new QVBoxLayout(inner);
            // 右侧留出滚动条 + 间隙的宽度
            il->setContentsMargins(0, 0, 10, 0);
            il->setSpacing(12);

            // --- 排版样式 ---
            il->addWidget(new ant::SectionTitle(QStringLiteral("排版样式"), inner));

            auto *grid = new QGridLayout;
            grid->setContentsMargins(0, 0, 0, 0);
            grid->setHorizontalSpacing(9);
            grid->setVerticalSpacing(9);

            const auto &themes = md::styleThemes();
            for (int i = 0; i < themes.size(); ++i) {
                auto *tc = new ant::ThemeCard(themes.at(i), inner);
                tc->setSelected(themes.at(i).id == m_opt.themeId);
                connect(tc, &ant::ThemeCard::picked, this, &MainWindow::onThemePicked);
                m_themeCards << tc;
                grid->addWidget(tc, i / 2, i % 2);
            }
            grid->setColumnStretch(0, 1);
            grid->setColumnStretch(1, 1);
            il->addLayout(grid);

            // --- 排版参数 ---
            il->addSpacing(4);
            il->addWidget(new ant::SectionTitle(QStringLiteral("排版参数"), inner));

            auto addSlider = [&](const QString &name, int lo, int hi, int val,
                                 QLabel **outLabel, int *outValue) {
                auto *head = new QWidget(inner);
                head->setObjectName(QStringLiteral("antPane"));
                auto *hl = new QHBoxLayout(head);
                hl->setContentsMargins(0, 0, 0, 0);
                hl->setSpacing(6);
                hl->addWidget(makeLabel(name, "antStat", head));
                hl->addStretch(1);
                auto *vl = makeLabel(QString::number(val), "antValue", head);
                hl->addWidget(vl);
                *outLabel = vl;

                auto *sl = new ant::Slider(Qt::Horizontal, inner);
                sl->setRange(lo, hi);
                sl->setValue(val);
                sl->setCursor(Qt::PointingHandCursor);
                connect(sl, &QSlider::valueChanged, this, [this, outValue, vl](int v) {
                    *outValue = v;
                    if (vl)
                        vl->setText(QString::number(v));
                    scheduleRender();
                });
                il->addWidget(head);
                il->addWidget(sl);
            };

            addSlider(QStringLiteral("正文字号"), 13, 20, m_opt.fontSize, &m_lblFontSize, &m_opt.fontSize);

            {
                auto *head = new QWidget(inner);
                head->setObjectName(QStringLiteral("antPane"));
                auto *hl = new QHBoxLayout(head);
                hl->setContentsMargins(0, 0, 0, 0);
                hl->addWidget(makeLabel(QStringLiteral("行间距"), "antStat", head));
                hl->addStretch(1);
                m_lblLineH = makeLabel(QString::number(m_opt.lineHeight, 'f', 2), "antValue", head);
                hl->addWidget(m_lblLineH);
                auto *sl = new ant::Slider(Qt::Horizontal, inner);
                sl->setRange(140, 240);
                sl->setValue(int(m_opt.lineHeight * 100));
                sl->setCursor(Qt::PointingHandCursor);
                connect(sl, &QSlider::valueChanged, this, [this](int v) {
                    m_opt.lineHeight = v / 100.0;
                    if (m_lblLineH)
                        m_lblLineH->setText(QString::number(m_opt.lineHeight, 'f', 2));
                    scheduleRender();
                });
                il->addWidget(head);
                il->addWidget(sl);
            }

            // --- 选项开关 ---
            il->addSpacing(4);
            il->addWidget(new ant::SectionTitle(QStringLiteral("输出选项"), inner));

            auto addSwitch = [&](const QString &name, bool init, bool *target) {
                auto *row = new QWidget(inner);
                row->setObjectName(QStringLiteral("antPane"));
                auto *hl = new QHBoxLayout(row);
                hl->setContentsMargins(0, 0, 0, 0);
                hl->addWidget(makeLabel(name, "antStat", row));
                hl->addStretch(1);
                auto *sw = new ant::Switch(row);
                sw->setChecked(init);
                connect(sw, &ant::Switch::toggled, this, [this, target](bool on) {
                    *target = on;
                    scheduleRender();
                });
                hl->addWidget(sw);
                il->addWidget(row);
            };

            addSwitch(QStringLiteral("代码块语法高亮"), m_opt.syntaxHighlight, &m_opt.syntaxHighlight);
            addSwitch(QStringLiteral("公式转图片"), m_opt.mathToImage, &m_opt.mathToImage);
            addSwitch(QStringLiteral("代码块自动换行"), m_opt.codeWrap, &m_opt.codeWrap);
            addSwitch(QStringLiteral("预览跟随滚动"), m_syncScroll, &m_syncScroll);
            addSwitch(QStringLiteral("裸链接自动识别"), m_opt.autoLink, &m_opt.autoLink);
            addSwitch(QStringLiteral("本地图片转 Base64"), m_opt.inlineLocalImages, &m_opt.inlineLocalImages);
            addSwitch(QStringLiteral("远程图片转 Base64"), m_opt.inlineRemoteImages, &m_opt.inlineRemoteImages);
            addSwitch(QStringLiteral("图片 alt 作图注"), m_opt.showImageAlt, &m_opt.showImageAlt);
            addSwitch(QStringLiteral("保留原文 HTML 片段"), m_opt.allowRawHtml, &m_opt.allowRawHtml);

            il->addSpacing(6);
            auto *hint = makeLabel(
                QStringLiteral("发布三步：\n"
                               "① 点上方「复制到公众号」\n"
                               "② 进公众号后台，新建图文消息\n"
                               "③ 在正文区 Ctrl+V 粘贴\n"
                               "\n"
                               "样式与图片会自动带过去。图片建议用图床链接，"
                               "粘贴时微信会自动转存到自己服务器。"),
                "antHint", inner);
            hint->setWordWrap(true);
            il->addWidget(hint);
            il->addStretch(1);

            scroll->setWidget(inner);
            cl->addWidget(scroll, 1);
            m_splitter->addWidget(card);
        }

        // ============ 中: Markdown 编辑器 ============
        {
            auto *card = new ant::Card(m_splitter);
            auto *cl   = new QVBoxLayout(card);
            cl->setContentsMargins(14, 12, 14, 14);
            cl->setSpacing(10);

            auto *head = new QWidget(card);
            head->setObjectName(QStringLiteral("antPane"));
            auto *hl = new QHBoxLayout(head);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(8);
            hl->addWidget(makeLabel(QStringLiteral("Markdown 源文"), "antSectionTitle", head));
            hl->addStretch(1);
            hl->addWidget(makeLabel(QStringLiteral("可直接拖拽 .md 文件到窗口"), "antHint", head));
            cl->addWidget(head);

            m_editor = new QPlainTextEdit(card);
            m_editor->setPlaceholderText(
                QStringLiteral("# 在这里粘贴你的 Markdown\n\n"
                               "支持 标题 / 段落 / **加粗** / *斜体* / 列表 / 表格 / 代码块 / 引用 / 图片"));
            m_editor->setTabStopDistance(4 * m_editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')));
            m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
            connect(m_editor, &QPlainTextEdit::textChanged, this, &MainWindow::scheduleRender);
            connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
                    this, &MainWindow::onEditorScrolled);

            cl->addWidget(m_editor, 1);
            m_splitter->addWidget(card);
        }

        // ============ 右: 预览 ============
        {
            auto *card = new ant::Card(m_splitter);
            auto *cl   = new QVBoxLayout(card);
            cl->setContentsMargins(14, 12, 14, 14);
            cl->setSpacing(10);

            auto *head = new QWidget(card);
            head->setObjectName(QStringLiteral("antPane"));
            auto *hl = new QHBoxLayout(head);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(8);
            hl->addWidget(makeLabel(QStringLiteral("预览 · 公众号效果"), "antSectionTitle", head));
            hl->addStretch(1);

            m_btnBg = new ant::Button(QStringLiteral("切换底色"), QStringLiteral("text"), head);
            m_btnBg->setMinimumHeight(26);
            m_btnBg->setProperty("antIcon", QStringLiteral("contrast"));
            auto *bCopy2 = new ant::Button(QStringLiteral("复制"), QStringLiteral("ghost"), head);
            bCopy2->setMinimumHeight(26);
            bCopy2->setProperty("antIcon", QStringLiteral("copy"));

            hl->addWidget(m_btnBg);
            hl->addWidget(bCopy2);
            cl->addWidget(head);

            m_view = new QWebEngineView(card);
            // 用自定义 page, 才能收到网页的滚动位置(用于两栏滚动同步)
            m_page = new PreviewPage(m_view);
            m_view->setPage(m_page);
            m_page->setBackgroundColor(QColor(QStringLiteral("#1a1a1c")));
            connect(m_page, &PreviewPage::scrollRatio,   this, &MainWindow::onPreviewScrolled);
            connect(m_page, &PreviewPage::documentReady, this, &MainWindow::onPreviewReady);
            cl->addWidget(m_view, 1);

            connect(m_btnBg, &ant::Button::clicked, this, &MainWindow::onTogglePreviewBg);
            connect(bCopy2, &ant::Button::clicked, this, &MainWindow::onCopyToWeChat);

            m_splitter->addWidget(card);
        }

        // 三栏各自的最小宽度, 保证窗口缩到很小时布局依然可用
        if (m_splitter->count() == 3) {
            m_splitter->widget(0)->setMinimumWidth(224);
            m_splitter->widget(1)->setMinimumWidth(288);
            m_splitter->widget(2)->setMinimumWidth(320);
        }

        // 按比例分配, 缩放窗口 / 最大化时三栏等比变化
        m_splitter->setStretchFactor(0, 20);
        m_splitter->setStretchFactor(1, 35);
        m_splitter->setStretchFactor(2, 45);
        const int bodyW = qMax(880, width() - 24);
        m_splitter->setSizes(QList<int>{int(bodyW * 0.20), int(bodyW * 0.35), int(bodyW * 0.45)});
    }

    // ---------------- 状态栏 ----------------
    {
        auto *sb = new QWidget(root);
        sb->setObjectName(QStringLiteral("antStatusBar"));
        sb->setFixedHeight(32);
        auto *sl = new QHBoxLayout(sb);
        sl->setContentsMargins(16, 0, 16, 0);
        sl->setSpacing(10);

        m_lblFile = makeLabel(QStringLiteral("未打开文件"), "antStat", sb);
        m_lblStat = makeLabel(QString(), "antStat", sb);
        sl->addWidget(m_lblFile);
        sl->addStretch(1);
        sl->addWidget(m_lblStat);

        outer->addWidget(sb);
    }
}

// ============================================================
//  主题
// ============================================================

void MainWindow::applyTheme()
{
    ant::setDarkMode(m_darkChrome);
    qApp->setStyleSheet(ant::buildStyleSheet(ant::tokens(m_darkChrome)));

    for (auto *w : findChildren<ant::Switch *>())
        w->update();
    for (auto *w : findChildren<ant::ThemeCard *>())
        w->update();
    for (auto *w : findChildren<ant::SectionTitle *>())
        w->update();

    if (auto *sep = findChild<QWidget *>(QStringLiteral("antSep")))
        sep->setStyleSheet(QStringLiteral("background:%1;").arg(ant::tokens(m_darkChrome).colorBorder));

    applyIcons();
    update();
}

// 按主题给带 antIcon 属性的按钮统一着色(图标是矢量, 随主题重新渲染)
void MainWindow::applyIcons()
{
    const ant::Tokens &t = ant::tokens(m_darkChrome);
    for (auto *b : findChildren<ant::Button *>()) {
        const QString name = b->property("antIcon").toString();
        if (name.isEmpty())
            continue;

        QColor c = (b->property("antIconTone").toString() == QLatin1String("inverse"))
                       ? QColor(QStringLiteral("#ffffff"))
                       : QColor(t.colorTextSecondary);
        if (!b->isEnabled())
            c = QColor(t.colorTextQuaternary);

        b->setIcon(ant::makeIcon(name, c, 15));
        b->setIconSize(QSize(15, 15));
    }
}

void MainWindow::onToggleDarkMode()
{
    m_darkChrome = !m_darkChrome;
    applyTheme();
    doRender();
}

void MainWindow::onTogglePreviewBg()
{
    m_darkPreview = !m_darkPreview;
    m_needFullReload = true;
    doRender();
}

// ============================================================
//  渲染
// ============================================================

void MainWindow::scheduleRender()
{
    if (m_renderTimer)
        m_renderTimer->start();
}

void MainWindow::doRender()
{
    if (!m_editor || !m_view)
        return;

    const QString src = m_editor->toPlainText();
    m_opt.basePath   = m_filePath.isEmpty() ? QDir::currentPath()
                                            : QFileInfo(m_filePath).absolutePath();

    m_mathCache.pending.clear();
    scanRemoteImages(src);
    m_result = md::renderMarkdown(src, m_opt);

    const md::StyleTheme &t = md::styleThemeById(m_opt.themeId);
    const QString wx        = md::toWeChatHtml(m_result, t, m_opt.fontSize);

    m_view->page()->setBackgroundColor(QColor(m_darkPreview ? QStringLiteral("#1a1a1c") : t.canvasBg));

    const bool fullReload = m_needFullReload;
    m_needFullReload = false;

    // 正文里有还没转成图片的公式时, 预览页需要带上本地 MathJax
    const bool needMath = !m_mathCache.pending.isEmpty();

    if (fullReload) {
        const QString doc = md::toPreviewDocument(wx, t, m_darkPreview,
                                                  needMath ? m_mathScriptUrl : QString(),
                                                  true);
        shotLog(m_shotPath, QStringLiteral("render: needMath=%1 scriptUrl=%2 docLen=%3 hasFn=%4 hasPh=%5 pending=%6")
                                .arg(needMath ? 1 : 0)
                                .arg(m_mathScriptUrl.isEmpty() ? 0 : 1)
                                .arg(doc.size())
                                .arg(doc.contains(QStringLiteral("md2wxTypesetMath")) ? 1 : 0)
                                .arg(doc.count(QStringLiteral("data-md2wx-math")))
                                .arg(m_mathCache.pending.size()));
        if (!m_shotPath.isEmpty()) {
            QFile df(m_shotPath + QStringLiteral(".preview.html"));
            if (df.open(QIODevice::WriteOnly | QIODevice::Truncate))
                df.write(doc.toUtf8());
        }
        m_view->setHtml(doc, QUrl::fromLocalFile(m_opt.basePath + QLatin1Char('/')));
    } else {
        // 只替换文章内容, 保留滚动位置
        const QString js =
            QStringLiteral("(function(){var p=document.getElementById('paper');"
                           "if(p){p.innerHTML=%1;}})();").arg(jsQuote(wx));
        m_view->page()->runJavaScript(js);
    }

    if (needMath)
        QTimer::singleShot(fullReload ? 400 : 60, this, &MainWindow::maybeStartMath);

    // 更新统计
    if (m_lblStat) {
        m_lblStat->setText(QStringLiteral("字符 %1 · 段落 %2 · 图片 %3 · 约 %4 分钟")
                               .arg(m_result.chars)
                               .arg(m_result.paragraphs)
                               .arg(m_result.images)
                               .arg(m_result.readMinutes));
    }
}

// ============================================================
//  远程图片预下载
//  开启后把正文里的外链图片抓下来内联成 base64, 微信就不用去抓外链了。
//  适用于对方服务器 Content-Type 不是 image/* 、或微信抓不到的情况。
// ============================================================

void MainWindow::scanRemoteImages(const QString &markdown)
{
    if (!m_opt.inlineRemoteImages || !m_net)
        return;

    static const QRegularExpression reMd(
        QStringLiteral("!\\[[^\\]]*\\]\\((https?://[^)\\s]+)"));
    static const QRegularExpression reHtml(
        QStringLiteral("<img[^>]+src=[\"'](https?://[^\"']+)[\"']"));

    QStringList urls;
    for (const QRegularExpression &re : {reMd, reHtml}) {
        auto it = re.globalMatch(markdown);
        while (it.hasNext())
            urls << it.next().captured(1);
    }

    for (const QString &u : urls) {
        if (m_remoteImages.contains(u) || m_remoteFailed.contains(u)
            || m_remotePending.contains(u))
            continue;

        m_remotePending.insert(u);

        QNetworkRequest req{QUrl(u)};
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0"));

        QNetworkReply *reply = m_net->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, u]() {
            reply->deleteLater();
            m_remotePending.remove(u);

            if (reply->error() == QNetworkReply::NoError) {
                const QByteArray d = reply->readAll();
                if (!d.isEmpty() && d.size() <= 4 * 1024 * 1024)
                    m_remoteImages.insert(u, d);
                else
                    m_remoteFailed.insert(u);
            } else {
                m_remoteFailed.insert(u);
            }

            if (m_remotePending.isEmpty()) {
                flashStatus(QStringLiteral("已内联 %1 张远程图片").arg(m_remoteImages.size()));
                scheduleRender();          // 重新渲染, 地址换成 data URI

                if (m_copyAfterRemote) {   // 用户刚才点了复制, 在等图片下载
                    m_copyAfterRemote = false;
                    QTimer::singleShot(300, this, &MainWindow::onCopyToWeChat);
                }
            }
        });
    }
}

// ============================================================
//  公式: MathJax -> SVG -> PNG -> dataURI
// ============================================================

void MainWindow::maybeStartMath()
{
    if (!m_opt.mathToImage || m_mathBusy || !m_view || !m_view->page())
        return;
    if (m_mathCache.pending.isEmpty())
        return;

    // loadFinished 有可能先为 about:blank 触发一次, 所以这里再确认页面真的就绪
    // (占位符已经进 DOM, 且转换函数已经定义)
    m_view->page()->runJavaScript(
        QStringLiteral("JSON.stringify({ph:document.querySelectorAll('[data-md2wx-math]').length,"
                       "fn:typeof window.md2wxTypesetMath})"),
        [this](const QVariant &v) {
            const QString s = v.toString();
            if (s.isEmpty())
                return;
            const QJsonObject o = QJsonDocument::fromJson(s.toUtf8()).object();
            const bool ready =
                o.value(QStringLiteral("ph")).toInt() > 0
                && o.value(QStringLiteral("fn")).toString() == QLatin1String("function");
            if (!ready) {
                if (++m_mathReadyTries < 40)
                    QTimer::singleShot(200, this, &MainWindow::maybeStartMath);
                return;
            }
            m_mathReadyTries = 0;
            runMathRound();
        });
}

void MainWindow::runMathRound()
{
    if (m_mathBusy || m_mathCache.pending.isEmpty())
        return;

    // 反复失败的直接标记为失败(退回显示 TeX 原文), 避免无限重试
    QStringList todo;
    for (const QString &key : m_mathCache.pending) {
        const int n = m_mathAttempts.value(key, 0) + 1;
        m_mathAttempts.insert(key, n);
        if (n > 3)
            m_mathCache.failed.insert(key);
        else
            todo << key;
    }
    if (todo.isEmpty()) {
        doRender();
        return;
    }

    m_mathRoundKeys = todo;
    m_mathBusy      = true;
    m_mathPollTries = 0;

    shotLog(m_shotPath, QStringLiteral("math round: %1 keys").arg(todo.size()));

    m_view->page()->runJavaScript(
        QStringLiteral("JSON.stringify({url:(location.href||'').substr(0,50),ready:document.readyState,"
                       "mj:!!window.MathJax,t2s:typeof (window.MathJax&&MathJax.tex2svg),"
                       "fn:typeof window.md2wxTypesetMath,"
                       "ph:document.querySelectorAll('[data-md2wx-math]').length,"
                       "spans:document.querySelectorAll('span').length,"
                       "scripts:document.querySelectorAll('script').length,"
                       "bodyLen:document.body?document.body.innerHTML.length:-1})"),
        [this](const QVariant &v) {
            shotLog(m_shotPath, QStringLiteral("math probe: ") + v.toString());
        });

    m_view->page()->runJavaScript(
        QStringLiteral("window.__md2wxMathResult=null;"
                       "if(window.%1){window.%1();}").arg(md::mathTypesetFunctionName()));

    if (m_mathPollTimer)
        m_mathPollTimer->start();
}

void MainWindow::pollMath()
{
    if (!m_view || !m_view->page()) {
        m_mathPollTimer->stop();
        m_mathBusy = false;
        return;
    }
    if (++m_mathPollTries > 80) {                 // 约 12s 超时
        m_mathPollTimer->stop();
        m_mathBusy = false;
        for (const QString &k : m_mathRoundKeys)
            m_mathCache.failed.insert(k);
        shotLog(m_shotPath, QStringLiteral("math round TIMEOUT, %1 keys marked failed")
                                .arg(m_mathRoundKeys.size()));
        m_mathRoundKeys.clear();
        doRender();
        return;
    }

    m_view->page()->runJavaScript(QStringLiteral("window.__md2wxMathResult || ''"),
                                  [this](const QVariant &v) {
        const QString s = v.toString();
        if (s.isEmpty())
            return;                               // 还没算完, 继续等
        if (m_mathPollTimer)
            m_mathPollTimer->stop();
        m_mathBusy = false;

        const QJsonDocument jd = QJsonDocument::fromJson(s.toUtf8());
        int okCount = 0;
        if (jd.isArray()) {
            const QJsonArray arr = jd.array();
            for (const QJsonValue &v2 : arr) {
                const QJsonObject o = v2.toObject();
                const QString tex     = o.value(QStringLiteral("tex")).toString();
                const bool    display = o.value(QStringLiteral("display")).toBool();
                const QString color   = o.value(QStringLiteral("color")).toString();
                const QString key     = md::mathKey(tex, display, color);

                if (o.contains(QStringLiteral("err"))) {
                    m_mathCache.failed.insert(key);
                    continue;
                }
                md::MathImage mi;
                mi.dataUri = o.value(QStringLiteral("png")).toString();
                mi.w       = o.value(QStringLiteral("w")).toDouble();
                mi.h       = o.value(QStringLiteral("h")).toDouble();
                mi.depth   = o.value(QStringLiteral("depth")).toDouble();
                if (mi.dataUri.startsWith(QStringLiteral("data:image")) && mi.w > 0 && mi.h > 0) {
                    m_mathCache.images.insert(key, mi);
                    ++okCount;
                } else {
                    m_mathCache.failed.insert(key);
                }
            }
        }

        // 本轮请求了但没拿到结果的, 直接标记失败
        for (const QString &k : m_mathRoundKeys) {
            if (!m_mathCache.images.contains(k))
                m_mathCache.failed.insert(k);
        }
        m_mathRoundKeys.clear();

        shotLog(m_shotPath, QStringLiteral("math round done: ok=%1 failed=%2")
                                .arg(okCount).arg(m_mathCache.failed.size()));

        if (okCount > 0)
            flashStatus(QStringLiteral("已渲染 %1 个公式为图片").arg(okCount));

        doRender();                               // 这次直接命中缓存
        m_lastMathApply = QDateTime::currentMSecsSinceEpoch();

        if (m_copyAfterMath) {
            m_copyAfterMath = false;
            QTimer::singleShot(120, this, &MainWindow::onCopyToWeChat);
        }
    });
}

// ============================================================
//  滚动同步: 编辑器 <-> 预览
// ============================================================

double MainWindow::editorScrollRatio() const
{
    if (!m_editor)
        return 0.0;
    QScrollBar *sb = m_editor->verticalScrollBar();
    if (!sb || sb->maximum() <= 0)
        return 0.0;
    return double(sb->value()) / double(sb->maximum());
}

void MainWindow::scrollPreviewTo(double ratio)
{
    if (!m_page)
        return;
    const QString js =
        QStringLiteral("window.md2wxScrollTo && window.md2wxScrollTo(%1);")
            .arg(QString::number(ratio, 'f', 6));
    m_page->runJavaScript(js);
}

void MainWindow::onEditorScrolled(int)
{
    if (!m_syncScroll || !m_page)
        return;
    m_syncGuard = QDateTime::currentMSecsSinceEpoch() + 220;
    scrollPreviewTo(editorScrollRatio());
}

void MainWindow::onPreviewScrolled(double ratio)
{
    if (!m_syncScroll || !m_editor)
        return;
    if (QDateTime::currentMSecsSinceEpoch() < m_syncGuard)
        return;                                 // 这次是程序自己滚的, 别回灌

    QScrollBar *sb = m_editor->verticalScrollBar();
    if (!sb || sb->maximum() <= 0)
        return;

    m_syncGuard = QDateTime::currentMSecsSinceEpoch() + 220;
    sb->setValue(int(sb->maximum() * ratio + 0.5));
}

void MainWindow::onPreviewReady()
{
    // 每次整页重载后, 把预览拉回编辑器所在位置
    if (!m_syncScroll || !m_page)
        return;
    const double r = editorScrollRatio();
    if (r <= 0.0)
        return;
    m_syncGuard = QDateTime::currentMSecsSinceEpoch() + 300;
    QTimer::singleShot(90, this, [this, r]() { scrollPreviewTo(r); });
}

// ============================================================
//  操作
// ============================================================

void MainWindow::onCopyToWeChat()
{
    if (m_renderTimer && m_renderTimer->isActive()) {
        m_renderTimer->stop();
        doRender();
    }

    // 还有公式没转成图片: 先转, 转完自动继续复制
    if (m_opt.mathToImage && (!m_mathCache.pending.isEmpty() || m_mathBusy)) {
        m_copyAfterMath = true;
        if (!m_mathBusy)
            maybeStartMath();
        flashStatus(QStringLiteral("正在渲染公式图片，稍候自动复制…"));
        return;
    }

    // 还有远程图片没下载完: 先下载, 下完自动继续复制
    if (m_opt.inlineRemoteImages && !m_remotePending.isEmpty()) {
        m_copyAfterRemote = true;
        flashStatus(QStringLiteral("正在下载图片（还剩 %1 张）…").arg(m_remotePending.size()));
        return;
    }
    m_copyAfterRemote = false;

    if (!m_mathCache.failed.isEmpty())
        flashStatus(QStringLiteral("有 %1 个公式未能渲染，已按 TeX 原文保留").arg(m_mathCache.failed.size()));

    const md::StyleTheme &t = md::styleThemeById(m_opt.themeId);
    const QString html      = md::toWeChatHtml(m_result, t, m_opt.fontSize);
    const QString plain     = md::htmlToPlainText(html);

    const bool ok = writeRichClipboard(html, plain);

    if (m_btnCopy) {
        m_btnCopy->setText(ok ? QStringLiteral("已复制 ✓") : QStringLiteral("复制失败"));
        QTimer::singleShot(1600, this, [this]() {
            if (m_btnCopy)
                m_btnCopy->setText(QStringLiteral("复制到公众号"));
        });
    }
    flashStatus(ok ? QStringLiteral("已复制（含排版）· 去公众号后台正文区 Ctrl+V")
                   : QStringLiteral("写入剪贴板失败，请重试"));

    if (m_copyTest)
        QTimer::singleShot(500, this, &MainWindow::diagnoseClipboard);
}

// 开发期诊断: 回读剪贴板, 确认真的写进去了
void MainWindow::diagnoseClipboard()
{
    QString diag;
#ifdef Q_OS_WIN
    if (::OpenClipboard(nullptr)) {
        const UINT f = ::RegisterClipboardFormatW(L"HTML Format");
        HANDLE h = ::GetClipboardData(f);
        if (!h) {
            diag = QStringLiteral("HTML Format 句柄为空");
        } else if (void *p = ::GlobalLock(h)) {
            const SIZE_T n = ::GlobalSize(h);
            diag = QStringLiteral("fmt=%1 size=%2 first=[%3]")
                       .arg(f)
                       .arg(qulonglong(n))
                       .arg(QString::fromUtf8(static_cast<const char *>(p),
                                              int(qMin<SIZE_T>(n, 48))));
            ::GlobalUnlock(h);
        } else {
            diag = QStringLiteral("GlobalLock 失败");
        }

        HANDLE ht = ::GetClipboardData(CF_UNICODETEXT);
        diag += QStringLiteral(" | textSize=%1")
                    .arg(ht ? qulonglong(::GlobalSize(ht)) : 0);
        ::CloseClipboard();
    } else {
        diag = QStringLiteral("OpenClipboard 失败");
    }
#endif
    QFile f(QDir::tempPath() + QStringLiteral("/md2wx-copytest.log"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(diag.toUtf8() + "\n");
}

void MainWindow::onOpenMpHome()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://mp.weixin.qq.com/")));
    flashStatus(QStringLiteral("已在浏览器打开公众号后台：新建图文消息 → 正文区 Ctrl+V 粘贴"));
}

void MainWindow::onExportHtml()
{
    if (m_opt.mathToImage && (!m_mathCache.pending.isEmpty() || m_mathBusy)) {
        if (!m_mathBusy)
            maybeStartMath();
        flashStatus(QStringLiteral("公式还在渲染，稍候再导出"));
        return;
    }

    const md::StyleTheme &t = md::styleThemeById(m_opt.themeId);
    const QString wx        = md::toWeChatHtml(m_result, t);
    const QString doc       = md::toPreviewDocument(wx, t, false);

    QString base = m_filePath.isEmpty() ? QStringLiteral("untitled")
                                        : QFileInfo(m_filePath).completeBaseName();
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 HTML"),
                                                base + QStringLiteral(".html"),
                                                QStringLiteral("HTML 文件 (*.html)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        flashStatus(QStringLiteral("导出失败：无法写入文件"));
        return;
    }
    f.write(doc.toUtf8());
    f.close();
    flashStatus(QStringLiteral("已导出：") + QDir::toNativeSeparators(path));
}

void MainWindow::setCopyTest()
{
    m_copyTest = true;
    QTimer::singleShot(4000, this, &MainWindow::onCopyToWeChat);
    QTimer::singleShot(20000, qApp, &QApplication::quit);   // 兜底别卡住
}

void MainWindow::openFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        flashStatus(QStringLiteral("打开失败：") + QDir::toNativeSeparators(path));
        return;
    }
    const QString text = QString::fromUtf8(f.readAll());
    f.close();
    setSource(text, path);
}

void MainWindow::onOpenFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开 Markdown"), QString(),
        QStringLiteral("Markdown (*.md *.markdown *.txt);;所有文件 (*.*)"));
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::onLoadSample()
{
    m_filePath.clear();
    setSource(sampleMarkdown());
    flashStatus(QStringLiteral("已载入示例文档"));
}

void MainWindow::onClearSource()
{
    m_filePath.clear();
    m_editor->clear();
    flashStatus(QStringLiteral("已清空"));
}

void MainWindow::onThemePicked(const QString &id)
{
    if (m_opt.themeId == id)
        return;
    m_opt.themeId = id;
    for (auto *tc : m_themeCards)
        tc->setSelected(tc->id() == id);
    m_needFullReload = true;   // 换样式需要整页重载
    doRender();
}

void MainWindow::setSource(const QString &text, const QString &path)
{
    m_filePath = path;
    if (m_editor)
        m_editor->setPlainText(text);
    if (m_lblFile)
        m_lblFile->setText(path.isEmpty() ? QStringLiteral("未打开文件")
                                          : QDir::toNativeSeparators(path));
    m_needFullReload = true;
    doRender();
}

void MainWindow::flashStatus(const QString &msg)
{
    if (!m_lblFile)
        return;
    m_lblFile->setText(msg);
    m_statusTimer->start();
}

// ============================================================
//  示例文档
// ============================================================

QString MainWindow::sampleMarkdown() const
{
    QFile f(QStringLiteral(":/examples/demo.md"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(f.readAll());
    return QStringLiteral("# 欢迎使用 MD2WeChat\n\n左侧编辑 Markdown，右侧实时预览。");
}

void MainWindow::setShotPath(const QString &path)
{
    m_shotPath = path;
    if (m_shotPath.isEmpty())
        return;
    QTimer::singleShot(4500, this, &MainWindow::captureShot);
}

void MainWindow::captureShot()
{
    if (m_shotPath.isEmpty() || m_shotDone)
        return;

    // 等公式图片转换完成再截图
    if ((m_mathBusy || !m_mathCache.pending.isEmpty()) && ++m_shotTries < 60) {
        QTimer::singleShot(400, this, &MainWindow::captureShot);
        return;
    }

    // 滚动同步自检: 把编辑器拉到 40%, 看预览有没有跟过去
    if (!m_scrollTested) {
        // 公式图片回填是异步的, 等 DOM 稳定后再测, 否则测到的是旧高度
        if (m_lastMathApply
            && QDateTime::currentMSecsSinceEpoch() - m_lastMathApply < 900) {
            QTimer::singleShot(300, this, &MainWindow::captureShot);
            return;
        }
        m_scrollTested = true;
        QScrollBar *sb = m_editor ? m_editor->verticalScrollBar() : nullptr;
        if (sb && sb->maximum() > 0) {
            sb->setValue(int(sb->maximum() * 0.40));
            QTimer::singleShot(900, this, [this]() {
                m_page->runJavaScript(
                    QStringLiteral("(function(){var m=document.documentElement.scrollHeight-window.innerHeight;"
                                   "return m>0?(window.scrollY/m).toFixed(3):'n/a';})()"),
                    [this](const QVariant &v) {
                        shotLog(m_shotPath, QStringLiteral("scroll sync check: editor=0.400 preview=")
                                                + v.toString());
                    });
            });
            // 截图前复位到顶部: 预览合成用的是 PDF 第一页(恒从头渲染), 保持一致
            QTimer::singleShot(1150, this, [this]() {
                if (m_editor)
                    m_editor->verticalScrollBar()->setValue(0);
            });
        }
        QTimer::singleShot(1800, this, &MainWindow::captureShot);
        return;
    }

    m_shotDone = true;

    shotLog(m_shotPath, QStringLiteral("captureShot start"));

    // 1) 先抓整个窗口(标题栏 / 工具栏 / 编辑器都由 Qt 绘制, 可直接抓)
    QPixmap chrome = grab();
    shotLog(m_shotPath, QStringLiteral("chrome grabbed %1x%2 dpr=%3 size=%4")
                            .arg(chrome.width()).arg(chrome.height())
                            .arg(chrome.devicePixelRatio()).arg(chrome.size().isEmpty() ? "empty" : "ok"));

    // 2) 预览区是 Chromium 的独立合成表面, 普通抓屏拿不到。
    //    改为让页面自己打印成 PDF, 再用 QtPdf 栅格化贴回预览区。
    const QString pdf = m_shotPath + QStringLiteral(".pdf");

    // 页面尺寸换算: 677 CSS px @96dpi ≈ 507.7pt ≈ 179mm; 高度给足, 让长文一页装下
    const QPageSize pageSize(QSizeF(508.0, 4200.0), QPageSize::Point, QStringLiteral("md2wx"));
    const QPageLayout layout(pageSize, QPageLayout::Portrait,
                             QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
    shotLog(m_shotPath, QStringLiteral("pageSize valid=%1 layout valid=%2")
                            .arg(pageSize.isValid() ? 1 : 0).arg(layout.isValid() ? 1 : 0));

    auto retried = QSharedPointer<bool>::create(false);

    connect(m_view->page(), &QWebEnginePage::pdfPrintingFinished, this,
            [this, chrome, pdf, retried](const QString &file, bool ok) mutable {
        shotLog(m_shotPath, QStringLiteral("pdfPrintingFinished ok=%1").arg(ok ? 1 : 0));

        if (!ok && !*retried) {
            *retried = true;
            shotLog(m_shotPath, QStringLiteral("retry with default page layout"));
            m_view->page()->printToPdf(file);
            return;
        }

        if (ok) {
            QPdfDocument doc;
            const QPdfDocument::DocumentError err = doc.load(file);
            shotLog(m_shotPath, QStringLiteral("pdf load err=%1 pages=%2")
                                    .arg(int(err)).arg(doc.pageCount()));
            if (err == QPdfDocument::NoError && doc.pageCount() > 0) {
                const QSizeF pt = doc.pageSize(0);
                const int w = 1160;   // 正文宽度 ≈ 580 逻辑像素 × dpr 2
                const int h = int(w * pt.height() / pt.width());
                shotLog(m_shotPath, QStringLiteral("render target %1x%2").arg(w).arg(h));
                const QImage img = doc.render(0, QSize(w, h));
                shotLog(m_shotPath, QStringLiteral("render result %1").arg(img.isNull() ? "null" : "ok"));
                if (!img.isNull()) {
                    img.save(m_shotPath + QStringLiteral("-article.png"), "PNG");

                    const qreal dpr = chrome.devicePixelRatio();
                    qreal lw = img.width() / dpr;
                    qreal lh = img.height() / dpr;
                    // 窄窗口下预览区装不下整篇文章宽度时等比缩小, 别被裁掉
                    if (lw > m_view->width()) {
                        const qreal k = qreal(m_view->width()) / lw;
                        lw *= k;
                        lh *= k;
                    }
                    const QPoint pos = m_view->mapTo(this, QPoint(0, 0));
                    const qreal x = pos.x() + (m_view->width() - lw) / 2.0;

                    QPainter p(&chrome);
                    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                    p.setClipRect(QRectF(pos.x(), pos.y(), m_view->width(), m_view->height()));
                    p.drawImage(QRectF(x, pos.y(), lw, lh), img);
                }
            }
        }
        QFile::remove(file);
        shotLog(m_shotPath, QStringLiteral("saved=%1").arg(chrome.save(m_shotPath, "PNG") ? QStringLiteral("ok") : QStringLiteral("fail")));
        QTimer::singleShot(200, qApp, &QApplication::quit);
    });

    // 同时把 WebEngine 里已渲染的 DOM 文本与生成的公众号 HTML 落盘, 便于核对
    m_view->page()->toPlainText([this](const QString &t) {
        shotLog(m_shotPath, QStringLiteral("webengine DOM text len=%1 head=[%2]")
                                .arg(t.size()).arg(t.left(48).simplified()));
    });

    {
        const md::StyleTheme &t = md::styleThemeById(m_opt.themeId);
        const QString wx        = md::toWeChatHtml(m_result, t, m_opt.fontSize);

        QFile f(m_shotPath + QStringLiteral(".html"));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(md::toPreviewDocument(wx, t, false).toUtf8());
            shotLog(m_shotPath, QStringLiteral("html dumped, wechat body len=%1").arg(m_result.body.size()));
        }
        // 「复制到公众号」真正放进剪贴板的那份 HTML, 单独落盘便于核对
        QFile g(m_shotPath + QStringLiteral(".wechat.html"));
        if (g.open(QIODevice::WriteOnly | QIODevice::Text)) {
            g.write(wx.toUtf8());
            shotLog(m_shotPath,
                    QStringLiteral("copy payload: len=%1 styleTag=%2 classAttr=%3 img=%4 imgData=%5")
                        .arg(wx.size())
                        .arg(wx.count(QStringLiteral("<style")))
                        .arg(wx.count(QStringLiteral("class=")))
                        .arg(wx.count(QStringLiteral("<img")))
                        .arg(wx.count(QStringLiteral("src=\"data:"))));
        }
    }

    shotLog(m_shotPath, QStringLiteral("printToPdf issued"));
    m_view->page()->printToPdf(pdf, layout);
}

// ============================================================
//  窗口行为: 拖拽 / 缩放 / 最大化
// ============================================================

void MainWindow::onToggleMaximize()
{
    if (m_maximized) {
        setGeometry(m_normalGeometry);
        m_maximized = false;
    } else {
        m_normalGeometry = geometry();
        if (auto *scr = screen())
            setGeometry(scr->availableGeometry());
        m_maximized = true;
    }
    if (m_caption)
        m_caption->setMaximized(m_maximized);
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    QMainWindow::resizeEvent(e);

    // 窄窗口下收起工具栏的长说明, 把宽度留给按钮
    const bool compact = width() < 1180;
    if (compact != m_compact) {
        m_compact = compact;
        if (m_toolbarHint)
            m_toolbarHint->setVisible(!compact);
    }
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
#ifdef Q_OS_WIN
    if (eventType == QByteArrayLiteral("windows_generic_MSG")) {
        auto *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCHITTEST && !m_maximized) {
            const int x = static_cast<short>(LOWORD(msg->lParam));
            const int y = static_cast<short>(HIWORD(msg->lParam));
            RECT r;
            ::GetWindowRect(reinterpret_cast<HWND>(winId()), &r);
            const int b = 6;
            const bool left   = x >= r.left && x < r.left + b;
            const bool right  = x < r.right && x >= r.right - b;
            const bool top    = y >= r.top && y < r.top + b;
            const bool bottom = y < r.bottom && y >= r.bottom - b;

            if (top && left)            *result = HTTOPLEFT;
            else if (top && right)      *result = HTTOPRIGHT;
            else if (bottom && left)    *result = HTBOTTOMLEFT;
            else if (bottom && right)   *result = HTBOTTOMRIGHT;
            else if (left)              *result = HTLEFT;
            else if (right)             *result = HTRIGHT;
            else if (top)               *result = HTTOP;
            else if (bottom)            *result = HTBOTTOM;
            else                        return false;
            return true;
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls()) {
        for (const QUrl &u : e->mimeData()->urls()) {
            const QString p = u.toLocalFile();
            if (p.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)
                || p.endsWith(QStringLiteral(".markdown"), Qt::CaseInsensitive)
                || p.endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive)) {
                e->acceptProposedAction();
                return;
            }
        }
    }
    e->ignore();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    for (const QUrl &u : e->mimeData()->urls()) {
        const QString p = u.toLocalFile();
        if (p.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)
            || p.endsWith(QStringLiteral(".markdown"), Qt::CaseInsensitive)
            || p.endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive)) {
            QFile f(p);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                setSource(QString::fromUtf8(f.readAll()), p);
                e->acceptProposedAction();
            }
            return;
        }
    }
    e->ignore();
}
