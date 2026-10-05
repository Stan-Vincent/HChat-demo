#include "global.h"

QString gate_url_prefix = "";   //在main函数实现完整url

std::function<void(QWidget*)> repolish = [](QWidget* w){
    if (!w) return;
    w->style()->unpolish(w);  // 拆掉：清掉该控件已缓存的样式、释放相关资源
    w->style()->polish(w);    // 重建：按当前属性/状态重新匹配规则，重新算缓存
};

std::function<QString(QString)> xorString = [](QString input){
    QString result = input; // 复制原始字符串，以便进行修改
    int length = input.length(); // 获取字符串的长度
    ushort xor_code = length % 255;
    for (int i = 0; i < length; ++i) {
        // 对每个字符进行异或操作
        // 注意：这里假设字符都是ASCII，因此直接转换为QChar
        result[i] = QChar(static_cast<ushort>(input[i].unicode() ^ xor_code));
    }
    return result;
};

void delay_run(int msecs) {
    QEventLoop loop;
    // singleShot 到时后会触发 loop.quit()，从而退出事件循环
    QTimer::singleShot(msecs, &loop, &QEventLoop::quit);
    loop.exec();
}
