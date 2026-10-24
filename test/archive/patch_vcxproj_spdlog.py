import re, sys, io

PROJ = [
    r"D:\QTcode\HChat\GateServer\GateServer.vcxproj",
    r"D:\QTcode\HChat\ChatServer\ChatServer.vcxproj",
    r"D:\QTcode\HChat\ChatServer2\ChatServer.vcxproj",
    r"D:\QTcode\HChat\StatusServer\StatusServer.vcxproj",
]

INC = r"$(ProjectDir)..\common;$(ProjectDir)..\common\third_party\spdlog\include"

def patch(path):
    raw = open(path, "rb").read()
    try:
        txt = raw.decode("utf-8")
    except UnicodeDecodeError:
        txt = raw.decode("gbk")
    orig = txt
    n_inc = n_utf8 = n_def = 0

    # 1) 往每个 AdditionalIncludeDirectories 前面插入 spdlog include
    def add_inc(m):
        nonlocal n_inc
        n_inc += 1
        inner = m.group(1) or ""
        return "<AdditionalIncludeDirectories>" + INC + ";" + inner
    txt = re.sub(r"<AdditionalIncludeDirectories>([^<]*)</AdditionalIncludeDirectories>",
                 add_inc, txt)

    # 2) 每个 ClCompile 里加 /utf-8（spdlog 的 fmt 要求，否则静态断言报错）
    def add_utf8(m):
        nonlocal n_utf8
        n_utf8 += 1
        return m.group(0) + "\n      <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>"
    txt = re.sub(r"<LanguageStandard>stdcpp17</LanguageStandard>", add_utf8, txt)

    # 3) 预处理器定义里加 SPDLOG_HEADER_ONLY（header-only 模式，不链接 .lib）
    def add_def(m):
        nonlocal n_def
        n_def += 1
        tag = m.group(1)
        if "SPDLOG_HEADER_ONLY" in m.group(2):
            return m.group(0)
        return "<PreprocessorDefinitions>" + tag + m.group(2) + ";SPDLOG_HEADER_ONLY"
    txt = re.sub(r"<PreprocessorDefinitions>([^;]*)([^<]*)</PreprocessorDefinitions>", add_def, txt)

    if txt == orig:
        print("  [skip] 无变化:", path)
        return False

    try:
        out = txt.encode("utf-8")
    except Exception:
        out = txt.encode("gbk")
    open(path, "wb").write(out)
    print("  [ok] %s  include=%d utf8=%d def=%d" % (path.split("\\")[-2], n_inc, n_utf8, n_def))
    return True

for p in PROJ:
    patch(p)
print("done")