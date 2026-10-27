QT += widgets network svg

CONFIG += c++17

# 崩溃日志的符号解析库（SymFromAddrW / CaptureStackBackTrace）

win32:LIBS += -ldbghelp
# DPAPI 的 CryptProtectData / CryptUnprotectData 在 crypt32.dll 里
#（注意不是 advapi32 —— 链 advapi32 会报 undefined reference to __imp_CryptProtectData）

win32:LIBS += -lcrypt32

RC_ICONS = icon.ico
DESTDIR = ./bin

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    ui/BubbleFrame.cpp \
    ui/SvgIconButton.cpp \
    ui/ChatItemBase.cpp \
    ui/ChatView.cpp \
    ui/FileBubble.cpp \
    ui/MessageTextEdit.cpp \
    ui/PictureBubble.cpp \
    ui/TextBubble.cpp \
    ui/adduseritem.cpp \
    ui/applyfriend.cpp \
    ui/applyfrienditem.cpp \
    ui/applyfriendlist.cpp \
    ui/applyfriendpage.cpp \
    ui/authenfriend.cpp \
    ui/chatdialog.cpp \
    ui/chatpage.cpp \
    ui/chatuserlist.cpp \
    ui/chatuserwid.cpp \
    ui/contactuserlist.cpp \
    ui/conuseritem.cpp \
    ui/findfaildlg.cpp \
    ui/findsuccessdlg.cpp \
    ui/friendinfopage.cpp \
    ui/friendlabel.cpp \
    ui/grouptipitem.cpp \
    ui/groupmemberdlg.cpp \
    ui/selectfrienddlg.cpp \
    ui/invaliditem.cpp \
    ui/lineitem.cpp \
    ui/listitembase.cpp \
    ui/loadingdlg.cpp \
    ui/logindialog.cpp \
    ui/mainwindow.cpp \
    ui/registerdialog.cpp \
    ui/resetdialog.cpp \
    ui/searchlist.cpp \
    ui/userinfopage.cpp \
    widgets/clickedbtn.cpp \
    widgets/clickedlabel.cpp \
    widgets/clickedoncelabel.cpp \
    widgets/customizeedit.cpp \
    widgets/customizetextedit.cpp \
    widgets/emojipanel.cpp \
    widgets/imagecropperlabel.cpp \
    widgets/statelabel.cpp \
    widgets/statewidget.cpp \
    widgets/timerbtn.cpp \
    widgets/toast.cpp \
    network/httpmgr.cpp \
    network/imagemanager.cpp \
    network/tcpmgr.cpp \
    network/uploadmanager.cpp \
    model/userdata.cpp \
    model/usermgr.cpp \
    common/animutil.cpp \
    common/avatarutil.cpp \
    common/global.cpp \
    common/logcategory.cpp

HEADERS += \
    ui/BubbleFrame.h \
    ui/SvgIconButton.h \
    ui/ChatItemBase.h \
    ui/ChatView.h \
    ui/FileBubble.h \
    ui/MessageTextEdit.h \
    ui/PictureBubble.h \
    ui/TextBubble.h \
    ui/adduseritem.h \
    ui/applyfriend.h \
    ui/applyfrienditem.h \
    ui/applyfriendlist.h \
    ui/applyfriendpage.h \
    ui/authenfriend.h \
    ui/chatdialog.h \
    ui/chatpage.h \
    ui/chatuserlist.h \
    ui/chatuserwid.h \
    ui/contactuserlist.h \
    ui/conuseritem.h \
    ui/findfaildlg.h \
    ui/findsuccessdlg.h \
    ui/friendinfopage.h \
    ui/friendlabel.h \
    ui/grouptipitem.h \
    ui/groupmemberdlg.h \
    ui/selectfrienddlg.h \
    ui/invaliditem.h \
    ui/lineitem.h \
    ui/listitembase.h \
    ui/loadingdlg.h \
    ui/logindialog.h \
    ui/mainwindow.h \
    ui/registerdialog.h \
    ui/resetdialog.h \
    ui/searchlist.h \
    ui/userinfopage.h \
    widgets/clickedbtn.h \
    widgets/clickedlabel.h \
    widgets/clickedoncelabel.h \
    widgets/customizeedit.h \
    widgets/customizetextedit.h \
    widgets/emojipanel.h \
    widgets/imagecropperdialog.h \
    widgets/imagecropperlabel.h \
    widgets/statelabel.h \
    widgets/statewidget.h \
    widgets/timerbtn.h \
    widgets/toast.h \
    network/httpmgr.h \
    network/imagemanager.h \
    network/tcpmgr.h \
    network/uploadmanager.h \
    model/userdata.h \
    model/usermgr.h \
    common/animutil.h \
    common/avatarutil.h \
    common/global.h \
    common/logcategory.h \
    common/singleton.h

FORMS += \
    ui/adduseritem.ui \
    ui/applyfriend.ui \
    ui/applyfrienditem.ui \
    ui/applyfriendpage.ui \
    ui/authenfriend.ui \
    ui/chatdialog.ui \
    ui/chatpage.ui \
    ui/chatuserwid.ui \
    ui/conuseritem.ui \
    ui/findfaildlg.ui \
    ui/findsuccessdlg.ui \
    ui/friendinfopage.ui \
    ui/friendlabel.ui \
    ui/grouptipitem.ui \
    ui/lineitem.ui \
    ui/loadingdlg.ui \
    ui/logindialog.ui \
    ui/mainwindow.ui \
    ui/registerdialog.ui \
    ui/resetdialog.ui \
    ui/userinfopage.ui

# 源码按职责分目录：ui/ widgets/ network/ model/ common/
# 把各目录铺进头文件搜索路径，源码里的 include 一行都不用改，
# .ui 里 customwidgets 的 header 裸文件名也照样能解析。
# （注释里不要写带井号的 include 指令，qmake 解析会报错）

INCLUDEPATH += \
    $$PWD/ui \
    $$PWD/widgets \
    $$PWD/network \
    $$PWD/model \
    $$PWD/common

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    rc.qrc

DISTFILES += \
    config.ini \
    icon.ico


# 注：-ldbghelp / -lcrypt32 放在文件顶部的顶层，不能写进下面的 win32:CONFIG(debug) 块，
# 块内带 win32: 前缀的赋值不会进 Makefile 的 LIBS，链接时会报 undefined reference。

win32:CONFIG(debug, debug | release)
{
    #指定要拷贝的文件目录为工程目录下release目录下的所有dll、lib文件，例如工程目录在D:\QT\Test
    #PWD就为D:/QT/Test，DllFile = D:/QT/Test/release/*.dll

    TargetConfig = $${PWD}/config.ini

    #将输入目录中的"/"替换为"\\"

    TargetConfig = $$replace(TargetConfig, /, \\)

    #将输出目录中的"/"替换为"\\"

    OutputDir =  $${OUT_PWD}/$${DESTDIR}
    OutputDir = $$replace(OutputDir, /, \\)

    # ------------------------------------------------------------------
    # 构建后要把这些文件拷到 exe 同级目录，否则双击 exe 会失败：
    #   config.ini        —— ConfigMgr 从当前工作目录读它，缺了直接闪退
    #   MinGW 运行时 3 个  —— 缺了报「无法找到入口点 __emutls_v._ZSt11__once_call」
    #   Qt6 运行时 4 个   —— 缺了报「找不到 Qt6Core.dll」
    #   plugins/platforms  —— 缺了报「no Qt platform plugin could be initialized」
    #
    # ★ 两条纪律（踩过坑，务必保持）：
    #
    # 1) 必须写成【一条】QMAKE_POST_LINK，命令之间用 && 连接。
    #    原来分成 10 次 QMAKE_POST_LINK += 追加，qmake 生成 Makefile 时会用【空格】
    #    把它们拼成一行，于是 cmd 把后面的命令全当成 copy 的参数，编译报
    #    「error: [Makefile.Debug:350: bin/Hcat.exe] Error 1」。
    #
    # 2) 本段按【cmd 语法】写（Qt Creator 用的就是 cmd 执行 recipe）：
    #    copy 是 cmd 内建命令，系统里没有 copy.exe。
    #    若改用 Git Bash / MSYS2 的 mingw32-make，MSYS 会把 /Y、/E、/I 当成
    #    POSIX 路径转换掉，xcopy 报「参数无效」（Error 4）。两个绕法：
    #      a) 用 cmd 跑同一个 make：  cmd //c "mingw32-make -j4"
    #      b) 或加环境变量：         MSYS2_ARG_CONV_EXCL="*" mingw32-make -j4
    #    推荐 a，和 Qt Creator 行为一致。

    MinGWBin = D:\\QT6\\Tools\\mingw1120_64\\bin
    QtBin    = D:\\QT6\\6.5.3\\mingw_64\\bin

    QMAKE_POST_LINK += \
        copy /Y \"$$TargetConfig\" \"$$OutputDir\" \
        && copy /Y \"$$MinGWBin\\libstdc++-6.dll\" \"$$OutputDir\" \
        && copy /Y \"$$MinGWBin\\libgcc_s_seh-1.dll\" \"$$OutputDir\" \
        && copy /Y \"$$MinGWBin\\libwinpthread-1.dll\" \"$$OutputDir\" \
        && copy /Y \"$$QtBin\\Qt6Core.dll\" \"$$OutputDir\" \
        && copy /Y \"$$QtBin\\Qt6Gui.dll\" \"$$OutputDir\" \
        && copy /Y \"$$QtBin\\Qt6Widgets.dll\" \"$$OutputDir\" \
        && copy /Y \"$$QtBin\\Qt6Network.dll\" \"$$OutputDir\" \
        && xcopy /Y /E /I \"$$QtBin\\..\\plugins\\platforms\" \"$$OutputDir\\platforms\" \
        && xcopy /Y /E /I \"$$QtBin\\..\\plugins\\styles\" \"$$OutputDir\\styles\" \
        && xcopy /Y /E /I \"$$QtBin\\..\\plugins\\imageformats\" \"$$OutputDir\\imageformats\"
}
