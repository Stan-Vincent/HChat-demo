#include "global.h"

QString gate_url_prefix = "";   //在main函数实现完整url

std::function<void(QWidget*)> repolish = [](QWidget* w){
    if (!w) return;
    w->style()->unpolish(w);  // 拆掉：清掉该控件已缓存的样式、释放相关资源
    w->style()->polish(w);    // 重建：按当前属性/状态重新匹配规则，重新算缓存
};

std::function<QString(QString)> xorString = [](QString originString){
    // 复制原始字符串，以便进行修改
    QString result = originString;

    // 获取字符串的长度
    int length = originString.length();

    //用字符串长度 % 255 作为异或密钥
    ushort xor_code = length % 255;

    for (int i = 0; i < length; ++i) {
        // 对每个字符进行异或操作
        // 注意：这里假设字符都是ASCII，因此直接转换为QChar
        //QChar的originString[i]通过.unicode() --> ASCII --> 与xor_code 按位异或 --> 得到结果数字 --> QChar
        result[i] = QChar(static_cast<ushort>(originString[i].unicode() ^ xor_code));
    }
    return result;
};

void delay_run(int msecs) {
    QEventLoop loop;
    // singleShot 到时后会触发 loop.quit()，从而退出事件循环
    QTimer::singleShot(msecs, &loop, &QEventLoop::quit);
    loop.exec();
}
