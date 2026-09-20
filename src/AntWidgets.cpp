#include "AntWidgets.h"
#include "AntTheme.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QSvgRenderer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace ant {

// ============================================================
//  图标: 从 SVG 精灵表里取元素, 按主题色重新着色
// ============================================================

QIcon makeIcon(const QString &name, const QColor &color, int size)
{
    static QSvgRenderer renderer(QStringLiteral(":/icons/ui.svg"));
    if (!renderer.isValid())
        return QIcon();

    const int px = qMax(8, size) * 2;          // 2x 超采样, 高 DPI 下不糊
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal pad = px * 0.08;               // 给描边留点余量, 免得被裁
    renderer.render(&p, name, QRectF(pad, pad, px - pad * 2, px - pad * 2));

    // 用 SourceIn 把整块图形染成同一个颜色
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(QRectF(0, 0, px, px), color);
    p.end();

    pm.setDevicePixelRatio(2.0);
    return QIcon(pm);
}

// ============================================================
//  Button
// ============================================================

Button::Button(const QString &text, const QString &type, QWidget *parent)
    : QPushButton(text, parent), m_type(type)
{
    setProperty("antType", m_type);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(30);
    setFocusPolicy(Qt::NoFocus);
}

void Button::setAntType(const QString &t)
{
    if (m_type == t)
        return;
    m_type = t;
    setProperty("antType", m_type);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void Button::setIconText(const QString &glyph)
{
    setText(glyph + QLatin1Char(' ') + text());
}

// ============================================================
//  Switch
// ============================================================

Switch::Switch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);

    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(160);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this, &Switch::onAnimValue);
}

QSize Switch::sizeHint() const
{
    return QSize(40, 22);
}

void Switch::onAnimValue(const QVariant &v)
{
    m_pos = v.toReal();
    update();
}

void Switch::nextCheckState()
{
    const qreal from = m_pos;      // 从当前滑块位置开始动画
    QAbstractButton::nextCheckState();   // 会调用 checkStateSet() 把 m_pos 拉到终点
    m_pos = from;                  // 拉回来, 让动画从原位置播放

    m_anim->stop();
    m_anim->setStartValue(m_pos);
    m_anim->setEndValue(isChecked() ? 1.0 : 0.0);
    m_anim->start();
}

// 程序化 setChecked() 时把滑块位置同步到位, 否则"默认开启"的开关
// 会显示成 OFF 的方向(用户看起来就是方向不对)
void Switch::checkStateSet()
{
    m_pos = isChecked() ? 1.0 : 0.0;
    update();
}

void Switch::paintEvent(QPaintEvent *)
{
    const Tokens &t = tokens(isDark());
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal h = height();
    const qreal r = h / 2.0;

    QColor track = isChecked() ? QColor(t.colorPrimary)
                               : QColor(isDark() ? t.colorFill : t.colorTextQuaternary);
    if (!isEnabled())
        track = QColor(t.colorFillTertiary);
    if (underMouse() && isEnabled())
        track = track.lighter(isDark() ? 118 : 106);

    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(QRectF(0, 0, width(), h), r, r);

    const qreal margin = 2.5;
    const qreal d      = h - margin * 2;
    const qreal x      = margin + m_pos * (width() - d - margin * 2);

    p.setBrush(QColor(QStringLiteral("#ffffff")));
    p.drawEllipse(QRectF(x, margin, d, d));
}

// ============================================================
//  Slider —— 点击轨道直接跳到该位置, 并支持按住拖动
//  QSlider 默认只是按 pageStep 步进, 手感很差
// ============================================================

Slider::Slider(Qt::Orientation o, QWidget *parent)
    : QSlider(o, parent)
{
    setCursor(Qt::PointingHandCursor);
}

void Slider::jumpTo(const QPoint &pos)
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect gr = style()->subControlRect(QStyle::CC_Slider, &opt,
                                             QStyle::SC_SliderGroove, this);
    const QRect sr = style()->subControlRect(QStyle::CC_Slider, &opt,
                                             QStyle::SC_SliderHandle, this);

    qreal p = 0.0;
    if (orientation() == Qt::Horizontal) {
        const int len = gr.width() - sr.width();
        if (len <= 0) { setValue(maximum()); return; }
        p = qreal(pos.x() - gr.x() - sr.width() / 2) / len;
    } else {
        const int len = gr.height() - sr.height();
        if (len <= 0) { setValue(minimum()); return; }
        p = 1.0 - qreal(pos.y() - gr.y() - sr.height() / 2) / len;
    }

    p = qBound(0.0, p, 1.0);
    setValue(minimum() + int(qRound(p * (maximum() - minimum()))));
}

void Slider::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && isEnabled()) {
        jumpTo(e->pos());
        setSliderDown(true);           // 进入拖动状态
        e->accept();
        return;
    }
    QSlider::mousePressEvent(e);
}

void Slider::mouseMoveEvent(QMouseEvent *e)
{
    if ((e->buttons() & Qt::LeftButton) && isSliderDown()) {
        jumpTo(e->pos());
        e->accept();
        return;
    }
    QSlider::mouseMoveEvent(e);
}

void Slider::mouseReleaseEvent(QMouseEvent *e)
{
    if (isSliderDown()) {
        setSliderDown(false);
        e->accept();
        return;
    }
    QSlider::mouseReleaseEvent(e);
}

// ============================================================
//  Card
// ============================================================

Card::Card(QWidget *parent)
    : QFrame(parent)
{
    setProperty("antCard", QStringLiteral("true"));
    setFrameShape(QFrame::NoFrame);
}

// ============================================================
//  SectionTitle
// ============================================================

SectionTitle::SectionTitle(const QString &text, QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(22);
    auto *lay = new QHBoxLayout(this);
    // 左侧 3px 是绘制的强调色条, 这里留出间距, 否则文字会压在色条上
    lay->setContentsMargins(12, 0, 0, 0);
    lay->setSpacing(0);
    m_label = new QLabel(text, this);
    m_label->setObjectName(QStringLiteral("antSectionTitle"));
    lay->addWidget(m_label, 0, Qt::AlignVCenter);
    lay->addStretch(1);
}

void SectionTitle::setText(const QString &text)
{
    m_label->setText(text);
}

void SectionTitle::paintEvent(QPaintEvent *)
{
    const Tokens &t = tokens(isDark());
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 5, 3, 12), 1.5, 1.5);
    p.fillPath(path, QColor(t.colorPrimary));
}

// ============================================================
//  ThemeCard
// ============================================================

ThemeCard::ThemeCard(const md::StyleTheme &th, QWidget *parent)
    : QFrame(parent)
{
    m_id         = th.id;
    m_name       = th.name;
    m_desc       = th.desc;
    m_swatch     = th.swatch;
    m_pageBg     = th.pageBg;
    m_accent     = th.accent;
    m_accentSoft = th.accentSoft;
    m_accentDark = th.accentDark;
    m_text       = th.text;
    m_textLight  = th.textLight;
    m_quoteBg    = th.quoteBg;
    m_quoteBorder = th.quoteBorder;
    m_h2Style    = th.h2Style;

    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setMinimumHeight(104);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setToolTip(th.name + QStringLiteral(" · ") + th.desc);
}

void ThemeCard::setSelected(bool s)
{
    if (m_selected == s)
        return;
    m_selected = s;
    update();
}

void ThemeCard::enterEvent(QEvent *)
{
    m_hover = true;
    update();
}

void ThemeCard::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void ThemeCard::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        emit picked(m_id);
    QFrame::mousePressEvent(e);
}

void ThemeCard::paintEvent(QPaintEvent *)
{
    const Tokens &t = tokens(isDark());
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 外框
    QColor border = m_selected ? QColor(m_accent)
                               : (m_hover ? QColor(t.colorTextQuaternary) : QColor(t.colorBorder));
    if (!m_selected && !m_hover)
        border = QColor(t.colorBorderSecondary);

    QPainterPath outer;
    const QRectF box(0.5, 0.5, width() - 1.0, height() - 1.0);
    outer.addRoundedRect(box, t.radiusLG, t.radiusLG);
    p.fillPath(outer, QColor(t.colorBgContainer));
    p.setPen(QPen(border, m_selected ? 1.6 : 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(outer);

    // 内部迷你文章预览
    const QRectF paper(8, 8, width() - 16, height() - 30);
    p.save();
    QPainterPath clip;
    clip.addRoundedRect(paper, t.radiusSM, t.radiusSM);
    p.setClipPath(clip);
    p.fillRect(paper, QColor(m_pageBg));

    const qreal x  = paper.left() + 9;
    const qreal w  = paper.width() - 18;

    // 标题(按 h2Style 区分)
    qreal y = paper.top() + 9;
    if (m_h2Style == 1) {
        p.fillRect(QRectF(x - 4, y - 2, w + 8, 11), QColor(m_accentSoft));
        p.fillRect(QRectF(x + 3, y + 2, w * 0.42, 4), QColor(m_accentDark));
        y += 15;
    } else if (m_h2Style == 2) {
        p.fillRect(QRectF(x + w * 0.28, y, w * 0.44, 4), QColor(m_accent));
        p.fillRect(QRectF(x - 4, y + 8, w + 8, 1), QColor(m_swatch));
        y += 15;
    } else if (m_h2Style == 3) {
        p.fillRect(QRectF(x - 4, y - 2, w + 8, 11), QColor(m_accentSoft));
        p.fillRect(QRectF(x - 4, y - 2, 3, 11), QColor(m_accent));
        p.fillRect(QRectF(x + 4, y + 2, w * 0.36, 4), QColor(m_accentDark));
        y += 15;
    } else {
        p.fillRect(QRectF(x - 5, y - 2, 3, 11), QColor(m_accent));
        p.fillRect(QRectF(x + 2, y + 2, w * 0.40, 4), QColor(m_accentDark));
        y += 15;
    }

    // 正文两行
    for (int i = 0; i < 2 && y < paper.bottom() - 30; ++i) {
        p.fillRect(QRectF(x, y, w * (i == 0 ? 1.0 : 0.72), 3), QColor(m_textLight));
        y += 7;
    }

    // 引用块
    if (y < paper.bottom() - 20) {
        const QRectF q(x, y, w, 13);
        p.fillRect(q, QColor(m_quoteBg));
        p.fillRect(QRectF(q.left(), q.top(), 2.5, q.height()), QColor(m_quoteBorder));
        p.fillRect(QRectF(q.left() + 7, q.top() + 4.5, w * 0.6, 3), QColor(m_textLight));
        y += 19;
    }

    // 再一行正文
    if (y < paper.bottom() - 8)
        p.fillRect(QRectF(x, y, w * 0.55, 3), QColor(m_textLight));

    p.restore();

    // 选中徽标
    if (m_selected) {
        const qreal d = 15;
        const QRectF badge(width() - d - 6, 6, d, d);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(m_accent));
        p.drawEllipse(badge);
        p.setPen(QPen(QColor(QStringLiteral("#ffffff")), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF c = badge.center();
        p.drawLine(QPointF(c.x() - 3.5, c.y()),
                   QPointF(c.x() - 1.0, c.y() + 2.6));
        p.drawLine(QPointF(c.x() - 1.0, c.y() + 2.6),
                   QPointF(c.x() + 3.6, c.y() - 2.4));
    }

    // 主题名
    QFont f = font();
    f.setPixelSize(11);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(QColor(m_selected ? m_accent : t.colorTextSecondary));
    QFontMetrics fm(f);
    const QString nm = fm.elidedText(m_name, Qt::ElideRight, int(width() - 46));
    p.drawText(QRectF(10, height() - 20, width() - 20, 16), Qt::AlignLeft | Qt::AlignVCenter, nm);
}

// ============================================================
//  CaptionButton —— 标题栏上的最小化/最大化/关闭
//  全部手绘: 同一套尺寸与线宽, 三个图标严格对齐
// ============================================================

CaptionButton::CaptionButton(Kind kind, QWidget *parent)
    : QAbstractButton(parent), m_kind(kind)
{
    setFixedSize(46, 46);
    setCursor(Qt::ArrowCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);
}

void CaptionButton::setKind(Kind k)
{
    if (m_kind == k)
        return;
    m_kind = k;
    update();
}

void CaptionButton::enterEvent(QEvent *)
{
    m_hover = true;
    update();
}

void CaptionButton::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void CaptionButton::paintEvent(QPaintEvent *)
{
    const Tokens &t = tokens(isDark());
    const bool isClose = (m_kind == Close);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_hover)
        p.fillRect(rect(), isClose ? QColor(QStringLiteral("#e81123"))
                                   : QColor(t.colorFillTertiary));

    const QColor stroke = (m_hover && isClose) ? QColor(QStringLiteral("#ffffff"))
                                               : QColor(t.colorTextSecondary);
    p.setPen(QPen(stroke, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    const QPointF c = QRectF(rect()).center();
    const qreal   s = 4.2;      // 半边长, 三个图标统一
    const qreal   h = 0.5;      // 对齐到半像素, 描边更锐利

    switch (m_kind) {
    case Minimize:
        p.drawLine(QPointF(c.x() - s, c.y() + h), QPointF(c.x() + s, c.y() + h));
        break;
    case Maximize:
        p.drawRect(QRectF(c.x() - s, c.y() - s, s * 2, s * 2));
        break;
    case Restore: {
        const qreal o = 1.8;
        p.drawRect(QRectF(c.x() - s + o, c.y() - s, s * 2 - o, s * 2 - o));
        p.drawRect(QRectF(c.x() - s, c.y() - s + o, s * 2 - o, s * 2 - o));
        break;
    }
    case Close:
    default:
        p.drawLine(QPointF(c.x() - s, c.y() - s), QPointF(c.x() + s, c.y() + s));
        p.drawLine(QPointF(c.x() + s, c.y() - s), QPointF(c.x() - s, c.y() + s));
        break;
    }
}

// ============================================================
//  CaptionBar
// ============================================================

CaptionBar::CaptionBar(const QString &title, const QString &subtitle, QWidget *parent)
    : QWidget(parent), m_title(title), m_subtitle(subtitle)
{
    setObjectName(QStringLiteral("antCaptionBar"));
    setFixedHeight(46);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(16, 0, 0, 0);
    lay->setSpacing(10);

    auto *logo = new QLabel(this);
    logo->setFixedSize(20, 20);
    logo->setPixmap(QPixmap(QStringLiteral(":/icons/logo.svg")).scaled(
        20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    lay->addWidget(logo, 0, Qt::AlignVCenter);

    auto *cap = new QLabel(m_title, this);
    cap->setObjectName(QStringLiteral("antTitle"));
    lay->addWidget(cap, 0, Qt::AlignVCenter);

    auto *sub = new QLabel(m_subtitle, this);
    sub->setObjectName(QStringLiteral("antSubTitle"));
    lay->addWidget(sub, 0, Qt::AlignVCenter);

    lay->addStretch(1);

    m_extra = new QWidget(this);
    m_extra->setObjectName(QStringLiteral("antPane"));
    auto *extraLay = new QHBoxLayout(m_extra);
    extraLay->setContentsMargins(0, 0, 4, 0);
    extraLay->setSpacing(8);
    extraLay->setAlignment(Qt::AlignVCenter);
    lay->addWidget(m_extra, 0, Qt::AlignVCenter);

    m_btnMin   = new CaptionButton(CaptionButton::Minimize, this);
    m_btnMax   = new CaptionButton(CaptionButton::Maximize, this);
    m_btnClose = new CaptionButton(CaptionButton::Close, this);

    lay->addWidget(m_btnMin, 0, Qt::AlignVCenter);
    lay->addWidget(m_btnMax, 0, Qt::AlignVCenter);
    lay->addWidget(m_btnClose, 0, Qt::AlignVCenter);

    connect(m_btnMin, &QAbstractButton::clicked, this, &CaptionBar::minimizeRequested);
    connect(m_btnMax, &QAbstractButton::clicked, this, &CaptionBar::toggleMaximize);
    connect(m_btnClose, &QAbstractButton::clicked, this, &CaptionBar::closeRequested);
}

void CaptionBar::setMaximized(bool m)
{
    if (m_btnMax)
        m_btnMax->setKind(m ? CaptionButton::Restore : CaptionButton::Maximize);
}

void CaptionBar::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        emit dragStarted();
    QWidget::mousePressEvent(e);
}

void CaptionBar::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        emit toggleMaximize();
    QWidget::mouseDoubleClickEvent(e);
}

} // namespace ant
