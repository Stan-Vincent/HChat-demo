#include "global.h"

QString gate_url_prefix = "";   //在main函数实现完整url

std::function<void(QWidget*)> repolish = [](QWidget* w){
    if (!w) return;
    w->style()->unpolish(w);  // 拆掉：清掉该控件已缓存的样式、释放相关资源
    w->style()->polish(w);    // 重建：按当前属性/状态重新匹配规则，重新算缓存
};

