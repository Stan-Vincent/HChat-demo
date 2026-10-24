# -*- coding: utf-8 -*-
"""把 Client 下残留的 2 个 GBK 头文件转成 UTF-8。

为什么只转 Client、不动后端：
  - Client 用 MinGW/g++ 编译，Qt 的 QString 从 const char* 构造默认按 **UTF-8** 解析，
    GBK 字节会显示成乱码；g++ 也默认按 UTF-8 读源文件。
  - 后端（ChatServer/GateServer/StatusServer）由 MSVC 编译，按本地代码页(936)解析 GBK，
    **本来就是正确的**；转 UTF-8 反而要加 /utf-8，还会踩 spdlog/fmt 的坑（已用 FMT_UNICODE=0 规避）。
    所以后端保持 GBK，只在文档里写清楚。

已确认这两个文件的中文 100% 在注释里，没有字符串字面量 -> 转换零功能风险。
"""
import io

FILES = ['Client/model/userdata.h', 'Client/ui/applyfriend.h']

for p in FILES:
    raw = open(p, 'rb').read()
    try:
        raw.decode('utf-8')
        print('跳过（已是 UTF-8）:', p)
        continue
    except Exception:
        pass
    text = raw.decode('gbk')
    assert text.count('\ufffd') == 0, p + ' GBK 解码出现替换字符'
    eol = '\r\n' if b'\r\n' in raw else '\n'
    open(p, 'wb').write(text.encode('utf-8'))
    print('已转 UTF-8 (%s 换行): %s' % ('CRLF' if eol == '\r\n' else 'LF', p))
