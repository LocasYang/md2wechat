#include "AntTheme.h"

namespace ant {

static const QString kFontFamily =
    QStringLiteral("'Microsoft YaHei UI','Microsoft YaHei','Segoe UI',Roboto,"
                   "'Helvetica Neue',Arial,'Noto Sans',sans-serif");
static const QString kMonoFamily =
    QStringLiteral("Consolas,'Cascadia Mono','Courier New',Menlo,monospace");

const Tokens &tokens(bool dark)
{
    static Tokens light;
    static Tokens darkT;
    static bool inited = false;

    if (!inited) {
        inited = true;

        // ---------------- 浅色 ----------------
        light.dark                = false;
        light.colorPrimary        = QStringLiteral("#1677ff");
        light.colorPrimaryHover   = QStringLiteral("#4096ff");
        light.colorPrimaryActive  = QStringLiteral("#0958d9");
        light.colorPrimaryBg      = QStringLiteral("#e6f4ff");
        light.colorPrimaryBorder  = QStringLiteral("#91caff");
        light.colorBgLayout       = QStringLiteral("#f5f5f5");
        light.colorBgContainer    = QStringLiteral("#ffffff");
        light.colorBgElevated     = QStringLiteral("#ffffff");
        light.colorBgSpotlight    = QStringLiteral("#262626");
        light.colorBgMask         = QStringLiteral("rgba(0,0,0,0.45)");
        light.colorText           = QStringLiteral("#1f1f1f");
        light.colorTextSecondary  = QStringLiteral("#595959");
        light.colorTextTertiary   = QStringLiteral("#8c8c8c");
        light.colorTextQuaternary = QStringLiteral("#bfbfbf");
        light.colorFill           = QStringLiteral("#d9d9d9");
        light.colorFillSecondary  = QStringLiteral("#f0f0f0");
        light.colorFillTertiary   = QStringLiteral("#f5f5f5");
        light.colorBorder         = QStringLiteral("#d9d9d9");
        light.colorBorderSecondary = QStringLiteral("#f0f0f0");
        light.colorSuccess        = QStringLiteral("#52c41a");
        light.colorWarning        = QStringLiteral("#faad14");
        light.colorError          = QStringLiteral("#ff4d4f");
        light.colorInfo           = QStringLiteral("#1677ff");
        light.fontFamily          = kFontFamily;
        light.monoFamily          = kMonoFamily;

        // ---------------- 深色 ----------------
        darkT                      = light;
        darkT.dark                 = true;
        darkT.colorPrimary         = QStringLiteral("#1677ff");
        darkT.colorPrimaryHover    = QStringLiteral("#4096ff");
        darkT.colorPrimaryActive   = QStringLiteral("#0958d9");
        darkT.colorPrimaryBg       = QStringLiteral("#111a2c");
        darkT.colorPrimaryBorder   = QStringLiteral("#15325b");
        darkT.colorBgLayout        = QStringLiteral("#141414");
        darkT.colorBgContainer     = QStringLiteral("#181818");
        darkT.colorBgElevated      = QStringLiteral("#1f1f1f");
        darkT.colorBgSpotlight     = QStringLiteral("#2b2b2f");
        darkT.colorBgMask          = QStringLiteral("rgba(0,0,0,0.65)");
        darkT.colorText            = QStringLiteral("#f3f3f3");
        darkT.colorTextSecondary   = QStringLiteral("#a6a6a6");
        darkT.colorTextTertiary    = QStringLiteral("#737373");
        darkT.colorTextQuaternary  = QStringLiteral("#5c5c5c");
        darkT.colorFill            = QStringLiteral("#3a3a3a");
        darkT.colorFillSecondary   = QStringLiteral("#2c2c2c");
        darkT.colorFillTertiary    = QStringLiteral("#242424");
        darkT.colorBorder          = QStringLiteral("#303030");
        darkT.colorBorderSecondary = QStringLiteral("#262626");
        darkT.colorSuccess         = QStringLiteral("#49aa19");
        darkT.colorWarning         = QStringLiteral("#d89614");
        darkT.colorError           = QStringLiteral("#dc4446");
        darkT.colorInfo            = QStringLiteral("#1677ff");
    }

    return dark ? darkT : light;
}

namespace {
bool g_dark = true;
}

void setDarkMode(bool dark)
{
    g_dark = dark;
}

bool isDark()
{
    return g_dark;
}

QString buildStyleSheet(const Tokens &t)
{
    const QString R  = QString::number(t.radius);
    const QString RL = QString::number(t.radiusLG);
    const QString RS = QString::number(t.radiusSM);

    QString qss;

    // ---------- 全局 ----------
    qss += QStringLiteral(
        "* { outline: none; }\n"
        "QWidget { font-family: %1; font-size: %2px; color: %3; }\n"
        "QMainWindow, QDialog { background: %4; }\n"
        "QWidget#antRoot { background: %4; }\n"
        "#antPane { background: transparent; }\n"
        "QLabel { background: transparent; }\n"
        "QLabel#antTitle { font-size: 15px; font-weight: 600; color: %3; letter-spacing: .3px; }\n"
        "QLabel#antSubTitle { font-size: 12px; color: %5; }\n"
        "QLabel#antSectionTitle { font-size: 13px; font-weight: 600; color: %6; letter-spacing: .5px; }\n"
        "QLabel#antHint { font-size: 12px; color: %5; }\n"
        "QLabel#antStat { font-size: 12px; color: %5; }\n"
        "QLabel#antValue { font-size: 12px; color: %7; font-family: %8; }\n"
        "QLabel#antBadge { font-size: 11px; color: %9; background: %10; border: 1px solid %11;"
        "  border-radius: 10px; padding: 1px 8px; }\n"
    ).arg(t.fontFamily)
     .arg(t.fontSize)
     .arg(t.colorText)
     .arg(t.colorBgLayout)
     .arg(t.colorTextTertiary)
     .arg(t.colorTextSecondary)
     .arg(t.colorTextSecondary)
     .arg(t.monoFamily)
     .arg(t.colorPrimary)
     .arg(t.colorPrimaryBg)
     .arg(t.colorPrimaryBorder);

    // ---------- 卡片 ----------
    qss += QStringLiteral(
        "QFrame[antCard=\"true\"] { background: %1; border: 1px solid %2; border-radius: %3px; }\n"
        "QFrame[antCard=\"false\"] { background: transparent; border: none; }\n"
    ).arg(t.colorBgContainer, t.colorBorderSecondary, RL);

    // ---------- 自定义标题栏 ----------
    // 注: 最小化/最大化/关闭按钮是自绘的(CaptionButton), 不走 QSS
    qss += QStringLiteral(
        "QWidget#antCaptionBar { background: %1; border-bottom: 1px solid %2; }\n"
        "QWidget#antToolbar { background: %1; border-bottom: 1px solid %2; }\n"
    ).arg(t.colorBgContainer, t.colorBorderSecondary);

    // ---------- 按钮 ----------
    qss += QStringLiteral(
        "QPushButton { border-radius: %1px; padding: 5px 15px; font-size: %2px; }\n"
        "QPushButton[antType=\"primary\"] { background: %3; color: #ffffff; border: 1px solid %3; font-weight: 500; }\n"
        "QPushButton[antType=\"primary\"]:hover { background: %4; border-color: %4; }\n"
        "QPushButton[antType=\"primary\"]:pressed { background: %5; border-color: %5; }\n"
        "QPushButton[antType=\"default\"] { background: %6; color: %7; border: 1px solid %8; }\n"
        "QPushButton[antType=\"default\"]:hover { color: %4; border-color: %4; }\n"
        "QPushButton[antType=\"default\"]:pressed { color: %5; border-color: %5; }\n"
        "QPushButton[antType=\"text\"] { background: transparent; color: %9; border: 1px solid transparent; }\n"
        "QPushButton[antType=\"text\"]:hover { background: %10; color: %7; }\n"
        "QPushButton[antType=\"text\"]:pressed { background: %11; color: %7; }\n"
        "QPushButton[antType=\"ghost\"] { background: %12; color: %3; border: 1px solid %13; }\n"
        "QPushButton[antType=\"ghost\"]:hover { color: %4; border-color: %4; }\n"
        "QPushButton[antType=\"danger\"] { background: transparent; color: %14; border: 1px solid %14; }\n"
        "QPushButton[antType=\"danger\"]:hover { color: #ffffff; background: %14; }\n"
        "QPushButton:disabled { background: %15; color: %16; border-color: %15; }\n"
    ).arg(RS).arg(t.fontSize)
     .arg(t.colorPrimary, t.colorPrimaryHover, t.colorPrimaryActive)
     .arg(t.colorBgContainer, t.colorText, t.colorBorder)
     .arg(t.colorTextSecondary, t.colorFillTertiary, t.colorFillSecondary)
     .arg(t.colorPrimaryBg, t.colorPrimaryBorder, t.colorError)
     .arg(t.colorFillTertiary, t.colorTextQuaternary);

    // ---------- 编辑器 ----------
    qss += QStringLiteral(
        "QPlainTextEdit, QTextEdit { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: %4px; padding: 14px 16px; selection-background-color: %5;"
        "  selection-color: #ffffff; font-family: %6; font-size: 13px; }\n"
        "QPlainTextEdit:focus, QTextEdit:focus { border-color: %7; }\n"
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: %4px;"
        "  padding: 5px 11px; selection-background-color: %5; selection-color: #ffffff; }\n"
        "QLineEdit:focus { border-color: %7; }\n"
        "QLineEdit::placeholder { color: %8; }\n"
    ).arg(t.colorBgContainer, t.colorText, t.colorBorder, R)
     .arg(t.colorPrimary, t.monoFamily, t.colorPrimaryHover, t.colorTextQuaternary);

    // ---------- 滚动条(细长条, 悬停加粗反馈) ----------
    qss += QStringLiteral(
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 2px 1px 2px 1px; }\n"
        "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 36px; }\n"
        "QScrollBar::handle:vertical:hover { background: %2; }\n"
        "QScrollBar:horizontal { background: transparent; height: 6px; margin: 1px 2px 1px 2px; }\n"
        "QScrollBar::handle:horizontal { background: %1; border-radius: 3px; min-width: 36px; }\n"
        "QScrollBar::handle:horizontal:hover { background: %2; }\n"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; background: none; border: none; }\n"
        "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }\n"
        "QScrollArea { background: transparent; border: none; }\n"
    ).arg(t.colorFill, t.colorTextQuaternary);

    // ---------- 分割器 ----------
    qss += QStringLiteral(
        "QSplitter { background: transparent; }\n"
        "QSplitter::handle { background: transparent; }\n"
        "QSplitter::handle:horizontal { width: 14px; }\n"
        "QSplitter::handle:vertical { height: 14px; }\n"
        "QSplitter::handle:hover { background: %1; }\n"
    ).arg(t.colorFillTertiary);

    // ---------- 滑块 ----------
    qss += QStringLiteral(
        "QSlider { min-height: 20px; background: transparent; }\n"
        "QSlider::groove:horizontal { height: 4px; background: %1; border-radius: 2px; }\n"
        "QSlider::sub-page:horizontal { background: %2; border-radius: 2px; }\n"
        "QSlider::handle:horizontal { background: %3; width: 14px; height: 14px;"
        "  margin: -6px 0; border-radius: 8px; border: 2px solid %2; }\n"
        "QSlider::handle:horizontal:hover { border-color: %4; }\n"
    ).arg(t.colorFillSecondary, t.colorPrimary, t.colorBgContainer, t.colorPrimaryHover);

    // ---------- 复选框 ----------
    qss += QStringLiteral(
        "QCheckBox { color: %1; spacing: 8px; font-size: 13px; background: transparent; }\n"
        "QCheckBox::indicator { width: 16px; height: 16px; border-radius: %5px;"
        "  border: 1px solid %2; background: %3; }\n"
        "QCheckBox::indicator:hover { border-color: %4; }\n"
        "QCheckBox::indicator:checked { background: %4; border-color: %4;"
        "  image: url(:/icons/check.svg); }\n"
    ).arg(t.colorTextSecondary, t.colorBorder, t.colorBgContainer, t.colorPrimary)
     .arg(QString::number(t.radiusXS));

    // ---------- 下拉框 ----------
    qss += QStringLiteral(
        "QComboBox { background: %1; color: %2; border: 1px solid %3; border-radius: %4px;"
        "  padding: 4px 10px; min-height: 22px; }\n"
        "QComboBox:hover { border-color: %5; }\n"
        "QComboBox::drop-down { border: none; width: 22px; }\n"
        "QComboBox::down-arrow { image: none; width: 0; height: 0; }\n"
        "QComboBox QAbstractItemView { background: %6; color: %2; border: 1px solid %3;"
        "  border-radius: %4px; padding: 4px; outline: none;"
        "  selection-background-color: %7; selection-color: %2; }\n"
    ).arg(t.colorBgContainer, t.colorText, t.colorBorder, R, t.colorPrimaryHover)
     .arg(t.colorBgElevated, t.colorFillTertiary);

    // ---------- 提示气泡 ----------
    qss += QStringLiteral(
        "QToolTip { background: %1; color: %2; border: 1px solid %3; border-radius: %4px;"
        "  padding: 5px 9px; font-size: 12px; }\n"
    ).arg(t.colorBgElevated, t.colorText, t.colorBorder, RS);

    // ---------- 状态栏 ----------
    qss += QStringLiteral(
        "QWidget#antStatusBar { background: %1; border-top: 1px solid %2; }\n"
    ).arg(t.colorBgContainer, t.colorBorderSecondary);

    // ---------- 菜单 ----------
    qss += QStringLiteral(
        "QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: %4px; padding: 5px; }\n"
        "QMenu::item { padding: 6px 22px 6px 14px; border-radius: %5px; }\n"
        "QMenu::item:selected { background: %6; }\n"
        "QMenu::separator { height: 1px; background: %3; margin: 4px 8px; }\n"
    ).arg(t.colorBgElevated, t.colorText, t.colorBorder, R, RS, t.colorFillTertiary);

    return qss;
}

} // namespace ant
