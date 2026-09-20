#include "StyleTheme.h"

namespace md {

static const QString kFontFamily =
    QStringLiteral("'PingFang SC','Microsoft YaHei UI','Microsoft YaHei',"
                   "-apple-system,BlinkMacSystemFont,'Helvetica Neue',Arial,sans-serif");
static const QString kMonoFamily =
    QStringLiteral("Menlo,Consolas,'Courier New',Monaco,monospace");

static StyleTheme makeBase(const QString &id, const QString &name, const QString &desc,
                           const QString &swatch, const QString &accent,
                           const QString &accentSoft, const QString &accentDark)
{
    StyleTheme t;
    t.id         = id;
    t.name       = name;
    t.desc       = desc;
    t.swatch     = swatch;
    t.fontFamily = kFontFamily;

    t.pageBg     = QStringLiteral("#ffffff");
    t.canvasBg   = QStringLiteral("#f2f3f5");
    t.text       = QStringLiteral("#3f3f3f");
    t.textStrong = QStringLiteral("#1a1a1a");
    t.textLight  = QStringLiteral("#8c8c8c");
    t.border     = QStringLiteral("#e8e8e8");
    t.bgSoft     = QStringLiteral("#f7f8fa");

    t.accent     = accent;
    t.accentSoft = accentSoft;
    t.accentDark = accentDark;
    t.link       = accent;

    t.h2Style = 0;
    t.h3Style = 0;

    t.quoteBg     = QStringLiteral("#f7f8fa");
    t.quoteBorder = QStringLiteral("#d9d9d9");
    t.quoteText   = QStringLiteral("#6b6b6b");

    t.code       = codeColorsDark();
    t.codeDark   = true;
    t.codeHeaderBg = QStringLiteral("#21252b");

    t.thBg     = QStringLiteral("#fafafa");
    t.thText   = QStringLiteral("#1a1a1a");
    t.tdBorder = QStringLiteral("#e8e8e8");

    t.imgRadius        = QStringLiteral("6px");
    t.imgCaptionColor  = QStringLiteral("#999999");
    return t;
}

const QVector<StyleTheme> &styleThemes()
{
    static QVector<StyleTheme> list;

    if (list.isEmpty()) {
        // ---------- 1. 经典蓝 ----------
        {
            StyleTheme t = makeBase(QStringLiteral("classic"), QStringLiteral("经典蓝"),
                                    QStringLiteral("Ant Design 蓝 · 左侧色条标题"),
                                    QStringLiteral("#1677ff"), QStringLiteral("#1677ff"),
                                    QStringLiteral("#e6f4ff"), QStringLiteral("#0958d9"));
            t.h2Style = 0;
            t.h3Style = 0;
            t.quoteBg     = QStringLiteral("#f7f8fa");
            t.quoteBorder = QStringLiteral("#c7d6e8");
            t.code = codeColorsDark();
            t.codeDark = true;
            t.codeHeaderBg = QStringLiteral("#21252b");
            list << t;
        }
        // ---------- 2. 微信绿 ----------
        {
            StyleTheme t = makeBase(QStringLiteral("wechat"), QStringLiteral("微信绿"),
                                    QStringLiteral("公众号原生绿 · 底色块标题"),
                                    QStringLiteral("#07c160"), QStringLiteral("#07c160"),
                                    QStringLiteral("#e8f9ef"), QStringLiteral("#04964a"));
            t.h2Style = 1;
            t.h3Style = 3;
            t.link        = QStringLiteral("#07c160");
            t.quoteBg     = QStringLiteral("#f2fbf6");
            t.quoteBorder = QStringLiteral("#07c160");
            t.thBg        = QStringLiteral("#f2fbf6");
            t.code = codeColorsDark();
            t.codeDark = true;
            t.codeHeaderBg = QStringLiteral("#1f2a24");
            list << t;
        }
        // ---------- 3. 极简灰 ----------
        {
            StyleTheme t = makeBase(QStringLiteral("minimal"), QStringLiteral("极简黑白"),
                                    QStringLiteral("居中下划线标题 · 极少装饰"),
                                    QStringLiteral("#262626"), QStringLiteral("#262626"),
                                    QStringLiteral("#f2f2f2"), QStringLiteral("#000000"));
            t.h2Style = 2;
            t.h3Style = 2;
            t.text       = QStringLiteral("#404040");
            t.textStrong = QStringLiteral("#111111");
            t.link       = QStringLiteral("#111111");
            t.accent     = QStringLiteral("#262626");
            t.border     = QStringLiteral("#eaeaea");
            t.quoteBg    = QStringLiteral("#fafafa");
            t.quoteBorder = QStringLiteral("#cfcfcf");
            t.tdBorder   = QStringLiteral("#ececec");
            t.code = codeColorsLight();
            t.codeDark = false;
            t.codeHeaderBg = QStringLiteral("#eceff1");
            list << t;
        }
        // ---------- 4. 暖橙 ----------
        {
            StyleTheme t = makeBase(QStringLiteral("warm"), QStringLiteral("暖橙"),
                                    QStringLiteral("暖调橙色 · 左条 + 浅底标题"),
                                    QStringLiteral("#ff7a45"), QStringLiteral("#ff7a45"),
                                    QStringLiteral("#fff2e8"), QStringLiteral("#d4380d"));
            t.h2Style = 3;
            t.h3Style = 0;
            t.link        = QStringLiteral("#d4380d");
            t.text        = QStringLiteral("#4a3b34");
            t.textStrong  = QStringLiteral("#2b1d16");
            t.quoteBg     = QStringLiteral("#fff8f4");
            t.quoteBorder = QStringLiteral("#ff7a45");
            t.quoteText   = QStringLiteral("#8c6a58");
            t.thBg        = QStringLiteral("#fff8f4");
            t.tdBorder    = QStringLiteral("#f2e3d9");
            t.code = codeColorsDark();
            t.codeDark = true;
            t.codeHeaderBg = QStringLiteral("#2b211c");
            list << t;
        }
        // ---------- 5. 夜读 ----------
        {
            StyleTheme t = makeBase(QStringLiteral("night"), QStringLiteral("夜读"),
                                    QStringLiteral("深色正文 · 适合夜间阅读"),
                                    QStringLiteral("#4d9fff"), QStringLiteral("#4d9fff"),
                                    QStringLiteral("#10233d"), QStringLiteral("#91caff"));
            t.pageBg      = QStringLiteral("#1c1c1e");
            t.canvasBg    = QStringLiteral("#131314");
            t.text        = QStringLiteral("#c8c8cc");
            t.textStrong  = QStringLiteral("#f0f0f2");
            t.textLight   = QStringLiteral("#8a8a90");
            t.border      = QStringLiteral("#3a3a3d");
            t.bgSoft      = QStringLiteral("#26262a");
            t.h2Style     = 0;
            t.h3Style     = 0;
            t.quoteBg     = QStringLiteral("#26262a");
            t.quoteBorder = QStringLiteral("#4d9fff");
            t.quoteText   = QStringLiteral("#a8a8b0");
            t.thBg        = QStringLiteral("#26262a");
            t.thText      = QStringLiteral("#f0f0f2");
            t.tdBorder    = QStringLiteral("#3a3a3d");
            t.code        = codeColorsDark();
            t.codeDark    = true;
            t.codeHeaderBg = QStringLiteral("#141417");
            t.imgCaptionColor = QStringLiteral("#7a7a82");
            list << t;
        }
    }
    return list;
}

const StyleTheme &styleThemeById(const QString &id)
{
    const auto &list = styleThemes();
    for (const auto &t : list) {
        if (t.id == id)
            return t;
    }
    return list.first();
}

} // namespace md
