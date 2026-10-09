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
    usermgr.cpp

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
    usermgr.h

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

    #执行copy命令
    QMAKE_POST_LINK += copy /Y \"$$TargetConfig\" \"$$OutputDir\"

    # ------------------------------------------------------------------
    # 把运行时 DLL 拷到 exe 同级目录。
    # 原因：这台机器的 PATH 里有两套 MinGW（Qt 自带的 mingw1120 和 MSYS2 的 gcc16.1），
    # 两者 libstdc++-6.dll 的导出符号不同。从 Qt Creator 运行时 kit 的 MinGW 会被加进
    # PATH 没问题，但双击 exe 用的是系统 PATH，会捞到 MSYS2 那份，报
    # 「无法找到入口点 __emutls_v._ZSt11__once_call」。
    # Windows 的 DLL 搜索顺序里 exe 所在目录优先级最高，放这里即可保证加载正确版本。
    MinGWBin = D:\\QT6\\Tools\\mingw1120_64\\bin
    QtBin    = D:\\QT6\\6.5.3\\mingw_64\\bin

    QMAKE_POST_LINK += copy /Y \"$$MinGWBin\\libstdc++-6.dll\" \"$$OutputDir\"
    QMAKE_POST_LINK += copy /Y \"$$MinGWBin\\libgcc_s_seh-1.dll\" \"$$OutputDir\"
    QMAKE_POST_LINK += copy /Y \"$$MinGWBin\\libwinpthread-1.dll\" \"$$OutputDir\"
    QMAKE_POST_LINK += copy /Y \"$$QtBin\\Qt6Core.dll\" \"$$OutputDir\"
    QMAKE_POST_LINK += copy /Y \"$$QtBin\\Qt6Gui.dll\" \"$$OutputDir\"
    QMAKE_POST_LINK += copy /Y \"$$QtBin\\Qt6Widgets.dll\" \"$$OutputDir\"
    QMAKE_POST_LINK += copy /Y \"$$QtBin\\Qt6Network.dll\" \"$$OutputDir\"

    # Qt 插件：platforms 是必须的（少它会报 "no Qt platform plugin could be initialized"，
    # 因为 exe 同级有 Qt6Core.dll 后 Qt 会从 <exedir> 找插件，而不是 Qt 安装目录）。
    # styles / imageformats 是可选的（样式表、图片格式支持）。
    QMAKE_POST_LINK += xcopy /Y /E /I \"$$QtBin\\..\\plugins\\platforms\" \"$$OutputDir\\platforms\"
    QMAKE_POST_LINK += xcopy /Y /E /I \"$$QtBin\\..\\plugins\\styles\" \"$$OutputDir\\styles\"
    QMAKE_POST_LINK += xcopy /Y /E /I \"$$QtBin\\..\\plugins\\imageformats\" \"$$OutputDir\\imageformats\"
}

