#pragma once

// ============================================================
//  AntWidgets —— 对齐 HuskarUI 观感的自绘控件
// ============================================================

#include <QAbstractButton>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

#include "StyleTheme.h"

class QVariantAnimation;

namespace ant {

// 从 :/icons/ui.svg 里取一个命名图标并按指定颜色着色(自带 2x 高清)
QIcon makeIcon(const QString &name, const QColor &color, int size = 15);

// ---------------- 按钮 ----------------
// antType: primary / default / text / ghost / danger
class Button : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(QString antType READ antType WRITE setAntType)

public:
    explicit Button(const QString &text = QString(),
                    const QString &type = QStringLiteral("default"),
                    QWidget *parent = nullptr);

    QString antType() const { return m_type; }
    void    setAntType(const QString &t);
    void    setIconText(const QString &glyph);   // 用字符当作图标前缀

private:
    QString m_type;
};

// ---------------- 开关 ----------------
class Switch : public QAbstractButton
{
    Q_OBJECT

public:
    explicit Switch(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *e) override;
    void nextCheckState() override;
    void checkStateSet() override;

private slots:
    void onAnimValue(const QVariant &v);

private:
    qreal             m_pos = 0.0;
    QVariantAnimation *m_anim = nullptr;
};

// ---------------- 滑块(点击直接跳到该位置, 也支持拖动) ----------------
class Slider : public QSlider
{
    Q_OBJECT

public:
    explicit Slider(Qt::Orientation o, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    void jumpTo(const QPoint &pos);
};

// ---------------- 卡片 ----------------
class Card : public QFrame
{
    Q_OBJECT
public:
    explicit Card(QWidget *parent = nullptr);
};

// ---------------- 区块标题(带强调竖线) ----------------
class SectionTitle : public QWidget
{
    Q_OBJECT
public:
    explicit SectionTitle(const QString &text, QWidget *parent = nullptr);
    void setText(const QString &text);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QLabel *m_label = nullptr;
};

// ---------------- 排版样式选择卡 ----------------
class ThemeCard : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(bool selected READ isSelected WRITE setSelected)

public:
    ThemeCard(const md::StyleTheme &theme, QWidget *parent = nullptr);

    QString id() const { return m_id; }
    bool    isSelected() const { return m_selected; }
    void    setSelected(bool s);

signals:
    void picked(const QString &id);

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    QString m_id, m_name, m_desc, m_swatch;
    bool    m_selected = false;
    bool    m_hover    = false;
    // 迷你预览用的色值(与公众号主题保持一致)
    QString m_pageBg, m_accent, m_accentSoft, m_accentDark, m_text, m_textLight, m_quoteBg, m_quoteBorder;
    int     m_h2Style = 0;
};

// ---------------- 标题栏按钮(手绘图标, 保证大小/基线一致) ----------------
// 不用 ─ □ ✕ 这类 Unicode 字符: 不同字体里字形大小和基线都不同, 排出来会参差不齐
class CaptionButton : public QAbstractButton
{
    Q_OBJECT

public:
    enum Kind { Minimize, Maximize, Restore, Close };

    explicit CaptionButton(Kind kind, QWidget *parent = nullptr);

    void setKind(Kind k);

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    Kind m_kind;
    bool m_hover = false;
};

// ---------------- 自绘标题栏 ----------------
class CaptionBar : public QWidget
{
    Q_OBJECT
public:
    explicit CaptionBar(const QString &title, const QString &subtitle, QWidget *parent = nullptr);

    QWidget *extraArea() const { return m_extra; }
    void setMaximized(bool m);

signals:
    void dragStarted();
    void toggleMaximize();
    void minimizeRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    QWidget        *m_extra    = nullptr;
    CaptionButton  *m_btnMin   = nullptr;
    CaptionButton  *m_btnMax   = nullptr;
    CaptionButton  *m_btnClose = nullptr;
    QString         m_title, m_subtitle;
};

} // namespace ant
