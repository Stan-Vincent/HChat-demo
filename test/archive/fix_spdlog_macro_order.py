import re

PROJ = [
    r"D:\QTcode\HChat\GateServer\GateServer.vcxproj",
    r"D:\QTcode\HChat\ChatServer\ChatServer.vcxproj",
    r"D:\QTcode\HChat\ChatServer2\ChatServer.vcxproj",
    r"D:\QTcode\HChat\StatusServer\StatusServer.vcxproj",
]

for path in PROJ:
    raw = open(path, "rb").read()
    try:
        txt = raw.decode("utf-8")
    except UnicodeDecodeError:
        txt = raw.decode("gbk")

    # 把 ";SPDLOG_HEADER_ONLY" 从 %(PreprocessorDefinitions) 之后挪到最前面。
    # 原因：%(PreprocessorDefinitions) 是 MSBuild 的"继承已有定义"标记，
    # 写在它后面的内容不会成为有效宏 —— SPDLOG_HEADER_ONLY 必须排在它前面。
    old_tok = "%(PreprocessorDefinitions);SPDLOG_HEADER_ONLY"
    new_tok = "SPDLOG_HEADER_ONLY;%(PreprocessorDefinitions)"
    if old_tok not in txt:
        print("  [skip]", path.split("\\")[-2])
        continue
    txt = txt.replace(old_tok, new_tok)

    try:
        out = txt.encode("utf-8")
    except Exception:
        out = txt.encode("gbk")
    open(path, "wb").write(out)
    print("  [ok] fixed", path.split("\\")[-2])
print("done")