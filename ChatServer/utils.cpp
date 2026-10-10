
#include "utils.h"
#include <cctype>

std::string getCurrentTimestamp() {
    namespace pt = boost::posix_time;
    // 取当前本地时间（精确到秒）
    pt::ptime now = pt::second_clock::local_time();
    // 用 time_facet 指定格式
    std::ostringstream oss;
    static std::locale loc(std::locale::classic(),
        new pt::time_facet("%Y-%m-%d %H:%M:%S"));
    oss.imbue(loc);
    oss << now;
    return oss.str();
}

//★ 安全的字符串转整数：解析失败返回 default_val，绝不抛异常。
int safeToInt(const std::string& s, int default_val) {
    if (s.empty()) {
        return default_val;
    }
    size_t i = 0;
    bool b_neg = false;
    if (s[0] == '+' || s[0] == '-') {
        b_neg = (s[0] == '-');
        i = 1;
    }
    if (i >= s.size()) {
        return default_val;          // 只有 "+" / "-" 这种
    }
    long long v = 0;
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            return default_val;      // 出现非数字就整个作废（比 std::stoi 更严格）
        }
        v = v * 10 + (s[i] - '0');
        if (v > 2147483647LL) {
            return default_val;      // 溢出保护
        }
    }
    return static_cast<int>(b_neg ? -v : v);
}
