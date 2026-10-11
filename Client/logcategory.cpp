#include "logcategory.h"

// 默认全部关闭 debug：日常使用不会被数百行调试输出淹没。
// 需要排查时用 QT_LOGGING_RULES="hchat.*.debug=true" 打开。
// 第三参数 QtWarningMsg：warning 及以上默认输出，debug 默认关闭。
Q_LOGGING_CATEGORY(lcNetwork, "hchat.network", QtWarningMsg)
Q_LOGGING_CATEGORY(lcChat, "hchat.chat", QtWarningMsg)
Q_LOGGING_CATEGORY(lcUi, "hchat.ui", QtWarningMsg)
Q_LOGGING_CATEGORY(lcUser, "hchat.user", QtWarningMsg)
Q_LOGGING_CATEGORY(lcAvatar, "hchat.avatar", QtWarningMsg)

void init_log_categories()
{
    // 默认级别已由上面的 Q_LOGGING_CATEGORY 三参形式集中设定。
    // 如需代码内动态调整，可在此调用静态方法：
    //   QLoggingCategory::setFilterRules("hchat.*.debug=true\n");
    // 也可不改代码，运行时用环境变量控制（PowerShell）：
    //   $env:QT_LOGGING_RULES = "hchat.network.debug=true"
}
