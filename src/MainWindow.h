#pragma once

#include <QMainWindow>
#include <QString>

#include "AntTheme.h"
#include "WeChatRenderer.h"

class QLabel;
class QNetworkAccessManager;
class QPlainTextEdit;
class QSplitter;
class QTimer;
class QWebEngineView;
class PreviewPage;

namespace ant {
class Button;
class CaptionBar;
class Switch;
class ThemeCard;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // 自截图模式(供开发期核对界面与预览渲染效果)
    void setShotPath(const QString &path);

    // 命令行直接打开一个 md 文件
    void openFile(const QString &path);

    // 开发期: 执行一次复制后自动退出(用来核对剪贴板内容)
    void setCopyTest();

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void scheduleRender();
    void doRender();
    void onCopyToWeChat();
    void onExportHtml();
    void onOpenMpHome();
    void diagnoseClipboard();
    void onOpenFile();
    void onLoadSample();
    void onClearSource();
    void onThemePicked(const QString &id);
    void onToggleDarkMode();
    void onTogglePreviewBg();
    void onToggleMaximize();
    void captureShot();
    void maybeStartMath();
    void runMathRound();
    void pollMath();
    void onEditorScrolled(int value);
    void onPreviewScrolled(double ratio);
    void onPreviewReady();

private:
    void buildUi();
    void applyTheme();
    void applyIcons();
    void setSource(const QString &text, const QString &path = QString());
    QString sampleMarkdown() const;
    void flashStatus(const QString &msg);
    double editorScrollRatio() const;
    void   scrollPreviewTo(double ratio);
    void   scanRemoteImages(const QString &markdown);

    // ---- UI ----
    ant::CaptionBar  *m_caption   = nullptr;
    QPlainTextEdit   *m_editor    = nullptr;
    QWebEngineView   *m_view      = nullptr;
    PreviewPage      *m_page      = nullptr;
    QSplitter        *m_splitter  = nullptr;
    QLabel           *m_lblStat   = nullptr;
    QLabel           *m_lblFile   = nullptr;
    QLabel           *m_lblFontSize = nullptr;
    QLabel           *m_lblLineH    = nullptr;
    QLabel           *m_toolbarHint = nullptr;
    ant::Button      *m_btnCopy   = nullptr;
    ant::Button      *m_btnBg     = nullptr;
    QList<ant::ThemeCard *> m_themeCards;

    QTimer *m_renderTimer = nullptr;
    QTimer *m_statusTimer = nullptr;

    // ---- 状态 ----
    md::RenderOptions m_opt;
    md::RenderResult  m_result;
    QString           m_filePath;
    bool              m_darkChrome   = true;
    bool              m_darkPreview  = true;
    bool              m_maximized    = false;
    bool              m_needFullReload = true;
    QRect             m_normalGeometry;
    QString           m_shotPath;
    bool              m_shotDone = false;
    int               m_shotTries = 0;
    bool              m_scrollTested = false;

    // ---- 公式转换 ----
    md::MathCache m_mathCache;
    QTimer       *m_mathPollTimer = nullptr;
    int           m_mathPollTries = 0;
    bool          m_mathBusy      = false;
    QStringList   m_mathRoundKeys;
    QHash<QString, int> m_mathAttempts;
    bool          m_copyAfterMath = false;
    bool          m_copyAfterRemote = false;
    QString       m_mathScriptUrl;
    int           m_mathReadyTries = 0;
    qint64        m_lastMathApply  = 0;   // 公式结果回填的时间戳(供自检等待)

    // ---- 滚动同步 ----
    bool    m_syncScroll = true;
    qint64  m_syncGuard  = 0;      // 程序自己触发滚动时的时间锁, 防止两栏互相打架

    // ---- 自适应 ----
    bool    m_compact    = false;  // 窄窗口模式
    bool    m_copyTest   = false;  // 复制后自动退出(开发期核对剪贴板)

    // ---- 远程图片预下载(「远程图片转 Base64」开启时用) ----
    QNetworkAccessManager     *m_net = nullptr;
    QHash<QString, QByteArray> m_remoteImages;
    QSet<QString>              m_remoteFailed;
    QSet<QString>              m_remotePending;
};
