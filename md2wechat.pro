#-------------------------------------------------
#
#  MD2WeChat —— Markdown 转微信公众号排版工具
#  构建环境: Qt 5.15.2 (MSVC) + Visual Studio 2022 Build Tools
#
#-------------------------------------------------

QT += core gui widgets network svg webenginewidgets quickwidgets pdf

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET   = md2wechat
TEMPLATE = app
CONFIG  += c++17

DEFINES += QT_DEPRECATED_WARNINGS

# MSVC 下源文件按 UTF-8 解析，避免中文字符串乱码
win32-msvc*|msvc {
    QMAKE_CXXFLAGS += /utf-8
}

INCLUDEPATH += $$PWD/src

# 无边框窗口的原生命中测试需要 user32
win32: LIBS += -luser32

SOURCES += \
    src/main.cpp \
    src/MarkdownParser.cpp \
    src/SyntaxHighlighter.cpp \
    src/StyleTheme.cpp \
    src/WeChatRenderer.cpp \
    src/AntTheme.cpp \
    src/AntWidgets.cpp \
    src/PreviewPage.cpp \
    src/MainWindow.cpp

HEADERS += \
    src/MarkdownParser.h \
    src/SyntaxHighlighter.h \
    src/StyleTheme.h \
    src/WeChatRenderer.h \
    src/AntTheme.h \
    src/AntWidgets.h \
    src/PreviewPage.h \
    src/MainWindow.h

RESOURCES += res/app.qrc

# Windows 可执行文件图标: 编译进 exe 的资源段, 资源管理器/任务栏/Alt+Tab 都用它
# (只调 setWindowIcon() 只在运行时生效, 不会改文件本身的图标)
RC_ICONS = $$PWD/res/app.ico

DESTDIR = $$PWD/bin
