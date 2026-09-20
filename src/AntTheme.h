#pragma once

// ============================================================
//  AntTheme —— 对齐 HuskarUI (Ant Design) 的设计变量与全局样式
//  变量取值来源: HuskarUI_Qt5/src/theme/Index.json
//    colorTextBase #000000 / #f3f3f3
//    colorBgBase   #f5f5f5 / #181818
//    colorPrimary  #1677ff (Preset_Blue, 取自 HusColorGenerator)
//    radiusBase    6  (LG 8 / SM 4 / XS 2)
//    fontSizeBase  16 (正文 14, H5 16, H4 20, H3 24, H2 30, H1 38)
// ============================================================

#include <QString>

namespace ant {

struct Tokens
{
    bool dark = true;

    // 主色
    QString colorPrimary;
    QString colorPrimaryHover;
    QString colorPrimaryActive;
    QString colorPrimaryBg;
    QString colorPrimaryBorder;

    // 背景
    QString colorBgLayout;
    QString colorBgContainer;
    QString colorBgElevated;
    QString colorBgSpotlight;
    QString colorBgMask;

    // 文字
    QString colorText;
    QString colorTextSecondary;
    QString colorTextTertiary;
    QString colorTextQuaternary;

    // 填充
    QString colorFill;
    QString colorFillSecondary;
    QString colorFillTertiary;

    // 边框
    QString colorBorder;
    QString colorBorderSecondary;

    // 功能色
    QString colorSuccess;
    QString colorWarning;
    QString colorError;
    QString colorInfo;

    // 圆角 / 字号 / 动效
    int radius     = 6;
    int radiusLG   = 8;
    int radiusSM   = 4;
    int radiusXS   = 2;
    int fontSize   = 14;
    int durationFast = 100;
    int durationMid  = 200;

    QString fontFamily;
    QString monoFamily;
};

const Tokens &tokens(bool dark);
QString buildStyleSheet(const Tokens &t);

// 全局模式标记, 供自绘控件取色
void setDarkMode(bool dark);
bool isDark();

} // namespace ant
