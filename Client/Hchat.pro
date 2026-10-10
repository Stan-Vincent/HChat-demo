QT += widgets network

CONFIG += c++17

RC_ICONS = icon.ico
DESTDIR = ./bin

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    BubbleFrame.cpp \
    ChatItemBase.cpp \
    ChatView.cpp \
    MessageTextEdit.cpp \
    PictureBubble.cpp \
    TextBubble.cpp \
    adduseritem.cpp \
    applyfriend.cpp \
    applyfrienditem.cpp \
    applyfriendlist.cpp \
    applyfriendpage.cpp \
    authenfriend.cpp \
    chatdialog.cpp \
    chatpage.cpp \
    chatuserlist.cpp \
    chatuserwid.cpp \
    clickedbtn.cpp \
    clickedlabel.cpp \
    clickedoncelabel.cpp \
    contactuserlist.cpp \
    conuseritem.cpp \
    customizeedit.cpp \
    customizetextedit.cpp \
    findfaildlg.cpp \
    findsuccessdlg.cpp \
    friendinfopage.cpp \
    friendlabel.cpp \
    global.cpp \
    grouptipitem.cpp \
    httpmgr.cpp \
    imagecropperlabel.cpp \
    invaliditem.cpp \
    lineitem.cpp \
    listitembase.cpp \
    loadingdlg.cpp \
    logindialog.cpp \
    main.cpp \
    mainwindow.cpp \
    registerdialog.cpp \
    resetdialog.cpp \
    searchlist.cpp \
    statelabel.cpp \
    statewidget.cpp \
    tcpmgr.cpp \
    timerbtn.cpp \
    userdata.cpp \
    userinfopage.cpp \
    usermgr.cpp \
    uploadmanager.cpp \
    imagemanager.cpp

HEADERS += \
    BubbleFrame.h \
    ChatItemBase.h \
    ChatView.h \
    MessageTextEdit.h \
    PictureBubble.h \
    TextBubble.h \
    adduseritem.h \
    applyfriend.h \
    applyfrienditem.h \
    applyfriendlist.h \
    applyfriendpage.h \
    authenfriend.h \
    chatdialog.h \
    chatpage.h \
    chatuserlist.h \
    chatuserwid.h \
    clickedbtn.h \
    clickedlabel.h \
    clickedoncelabel.h \
    contactuserlist.h \
    conuseritem.h \
    customizeedit.h \
    customizetextedit.h \
    findfaildlg.h \
    findsuccessdlg.h \
    friendinfopage.h \
    friendlabel.h \
    global.h \
    grouptipitem.h \
    httpmgr.h \
    imagecropperdialog.h \
    imagecropperlabel.h \
    invaliditem.h \
    lineitem.h \
    listitembase.h \
    loadingdlg.h \
    logindialog.h \
    mainwindow.h \
    registerdialog.h \
    resetdialog.h \
    searchlist.h \
    singleton.h \
    statelabel.h \
    statewidget.h \
    tcpmgr.h \
    timerbtn.h \
    userdata.h \
    userinfopage.h \
    usermgr.h \
    uploadmanager.h \
    imagemanager.h

FORMS += \
    adduseritem.ui \
    applyfriend.ui \
    applyfrienditem.ui \
    applyfriendpage.ui \
    authenfriend.ui \
    chatdialog.ui \
    chatpage.ui \
    chatuserwid.ui \
    conuseritem.ui \
    findfaildlg.ui \
    findsuccessdlg.ui \
    friendinfopage.ui \
    friendlabel.ui \
    grouptipitem.ui \
    lineitem.ui \
    loadingdlg.ui \
    logindialog.ui \
    mainwindow.ui \
    registerdialog.ui \
    resetdialog.ui \
    userinfopage.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    rc.qrc

DISTFILES += \
    config.ini \
    icon.ico


# 崩溃日志的符号解析库（SymFromAddrW / CaptureStackBackTrace）
win32:LIBS += -ldbghelp
# DPAPI 的 CryptProtectData / CryptUnprotectData 在 crypt32.dll 里
#（注意不是 advapi32 —— 链 advapi32 会报 undefined reference to __imp_CryptProtectData）
win32:LIBS += -lcrypt32

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
    #    「error: [Makefile.Debug:350: bin/Hchat.exe] Error 1」。
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
