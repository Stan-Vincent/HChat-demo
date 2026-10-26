# -*- coding: utf-8 -*-
"""
P1 §5：把客户端图标资产从 PNG 重着色的位图，换成 Lucide 风线性 SVG
（24 网格 / 1.8 描边 / 圆角线帽）。仅替换资产与引用字符串，图标状态机逻辑不动。

- 深色侧边栏上的图标：normal=浅灰(#D5D8E0)，active(hover/selected)=靛蓝(#4C5FD7)
- 白底区域的图标：normal=深灰(#6B6F76)，active=靛蓝(#4C5FD7)
- 语义色：红(#E5484D) 用于 red_point / send_fail

用法：python test/gen_p1_icons.py  （需可写 Client/res 与 Client/rc.qrc）
"""
import os
import io

CLIENT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Client"))
RES = os.path.join(CLIENT, "res")
RC = os.path.join(CLIENT, "rc.qrc")
QSS = os.path.join(CLIENT, "style", "stylesheet.qss")

# 颜色档
LIGHT = "#D5D8E0"   # 深色侧栏上的 normal 描边
GRAY = "#6B6F76"    # 白底上的 normal 描边
INDIGO = "#4C5FD7"  # active / 主色强调
RED = "#E5484D"

# 形状（Lucide 风 path，24x24）
SHAPES = {
    "msg_square": '<rect x="3" y="3" width="18" height="18" rx="2" ry="2"/>'
                 '<path d="M9 9h6"/><path d="M9 13h4"/>',
    "users": '<path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/>'
             '<circle cx="9" cy="7" r="4"/>'
             '<path d="M22 21v-2a4 4 0 0 0-3-3.87"/>'
             '<path d="M16 3.13a4 4 0 0 1 0 7.75"/>',
    "sliders": '<line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/>'
               '<line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/>'
               '<line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/>'
               '<line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/>'
               '<line x1="17" y1="16" x2="23" y2="16"/>',
    "userplus": '<path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/>'
                '<circle cx="9" cy="7" r="4"/>'
                '<line x1="19" y1="8" x2="19" y2="14"/><line x1="22" y1="11" x2="16" y2="11"/>',
    "smile": '<circle cx="12" cy="12" r="10"/>'
             '<path d="M8 14s1.5 2 4 2 4-2 4-2"/>'
             '<line x1="9" y1="9" x2="9.01" y2="9"/><line x1="15" y1="9" x2="15.01" y2="9"/>',
    "file": '<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/>'
            '<path d="M14 2v6h6"/>',
    "eye": '<path d="M2 12s3-7 10-7 10 7 10 7-3 7-10 7-10-7-10-7z"/>'
           '<circle cx="12" cy="12" r="3"/>',
    "eyeoff": '<path d="M9.88 9.88a3 3 0 0 0 4.24 4.24"/>'
              '<path d="M10.73 5.08A10.43 10.43 0 0 1 12 5c7 0 10 7 10 7a13.16 13.16 0 0 1-1.67 2.68"/>'
              '<path d="M6.61 6.61A13.526 13.526 0 0 0 2 12s3 7 10 7a9.74 9.74 0 0 0 5.39-1.61"/>'
              '<line x1="2" y1="2" x2="22" y2="22"/>',
    "video": '<path d="m22 8-6 4 6 4V8Z"/><rect x="2" y="6" width="14" height="12" rx="2"/>',
    "mic": '<rect x="9" y="2" width="6" height="12" rx="3"/>'
           '<path d="M19 10v2a7 7 0 0 1-14 0v-2"/>'
           '<line x1="12" y1="19" x2="12" y2="22"/>',
    "xicon": '<line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>',
    "chevdown": '<polyline points="6 9 12 15 18 9"/>',
    "plus": '<line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>',
    "chevright": '<polyline points="9 18 15 12 9 6"/>',
    "search": '<circle cx="11" cy="11" r="7"/><line x1="21" y1="21" x2="16.65" y2="16.65"/>',
    "dot": '<circle cx="12" cy="12" r="4"/>',
    "check": '<polyline points="20 6 9 17 4 12"/>',
    "excl": '<line x1="12" y1="8" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>',
}

# 每个被引用的图标基名 -> (形状, 颜色档). 颜色档: light/gray/indigo/red
MAPPING = {
    # 侧边栏（深底）：normal=浅灰, 其余=靛蓝
    "chat_icon": ("msg_square", "light"),
    "chat_icon_hover": ("msg_square", "indigo"),
    "chat_icon_press": ("msg_square", "indigo"),
    "chat_icon_select_hover": ("msg_square", "indigo"),
    "chat_icon_select_press": ("msg_square", "indigo"),
    "contact_list": ("users", "light"),
    "contact_list_hover": ("users", "indigo"),
    "contact_list_press": ("users", "indigo"),
    "settings": ("sliders", "light"),
    "settings_hover": ("sliders", "indigo"),
    "settings_press": ("sliders", "indigo"),
    "settings_select_hover": ("sliders", "indigo"),
    "settings_select_press": ("sliders", "indigo"),
    # 白底区域：normal=深灰, active=靛蓝
    "add_friend": ("userplus", "gray"),
    "add_friend_normal": ("userplus", "gray"),
    "add_friend_hover": ("userplus", "indigo"),
    "smile": ("smile", "gray"),
    "smile_hover": ("smile", "indigo"),
    "smile_press": ("smile", "indigo"),
    "filedir": ("file", "gray"),
    "filedir_hover": ("file", "indigo"),
    "filedir_press": ("file", "indigo"),
    "unvisible": ("eyeoff", "gray"),
    "unvisible_hover": ("eyeoff", "indigo"),
    "visible": ("eye", "gray"),
    "visible_hover": ("eye", "indigo"),
    "msg_chat_normal": ("msg_square", "gray"),
    "msg_chat_hover": ("msg_square", "indigo"),
    "msg_chat_press": ("msg_square", "indigo"),
    "video_chat_normal": ("video", "gray"),
    "video_chat_hover": ("video", "indigo"),
    "video_chat_press": ("video", "indigo"),
    "voice_chat_normal": ("mic", "gray"),
    "voice_chat_hover": ("mic", "indigo"),
    "voice_chat_press": ("mic", "indigo"),
    "search": ("search", "gray"),
    "close_transparent": ("xicon", "gray"),
    "close_search": ("xicon", "gray"),
    "tipclose": ("xicon", "gray"),
    "arowdown": ("chevdown", "gray"),
    "addtip": ("plus", "gray"),
    "right_tip": ("chevright", "gray"),
    "unread": ("dot", "gray"),
    "readed": ("check", "gray"),
    "red_point": ("dot", "red"),
    "send_fail": ("excl", "red"),
}

COLOR = {"light": LIGHT, "gray": GRAY, "indigo": INDIGO, "red": RED}


def make_svg(shape, color, filled=False):
    stroke = "none" if filled else color
    fill = color if filled else "none"
    sw = "2" if (shape == "excl") else "1.8"
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" '
        'viewBox="0 0 24 24" fill="{fill}" stroke="{stroke}" '
        'stroke-width="{sw}" stroke-linecap="round" stroke-linejoin="round">'
        '{paths}</svg>\n'
    ).format(fill=fill, stroke=stroke, sw=sw, paths=SHAPES[shape])


def main():
    generated = []
    for base, (shape, kind) in MAPPING.items():
        filled = (shape == "dot")  # 圆点用填充
        svg = make_svg(shape, COLOR[kind], filled)
        path = os.path.join(RES, base + ".svg")
        with io.open(path, "w", encoding="utf-8") as f:
            f.write(svg)
        generated.append(base)

    # ---- 重写 rc.qrc：保留非图标资源，图标 png -> svg ----
    kept = [
        "style/stylesheet.qss",
        "res/head_1.jpg", "res/head_2.jpg", "res/head_3.jpg",
        "res/head_4.jpg", "res/head_5.jpg",
        "res/loading.gif", "res/loading2.gif",
        "res/Icon.png", "res/LoginIcon.png",
        "res/female.png", "res/male.png",
    ]
    lines = ['<RCC>', '    <qresource prefix="/">']
    for k in kept:
        lines.append('        <file>%s</file>' % k)
    for base in sorted(MAPPING.keys()):
        lines.append('        <file>res/%s.svg</file>' % base)
    lines.append('    </qresource>')
    lines.append('</RCC>')
    with io.open(RC, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    # ---- 改写 QSS：图标引用 png -> svg（QSS 内全部 png 均为本次转换的图标）----
    with io.open(QSS, "r", encoding="utf-8") as f:
        qss = f.read()
    for base in MAPPING.keys():
        qss = qss.replace("res/%s.png" % base, "res/%s.svg" % base)
    with io.open(QSS, "w", encoding="utf-8") as f:
        f.write(qss)

    print("generated %d svg files; rc.qrc + stylesheet.qss updated" % len(generated))


if __name__ == "__main__":
    main()
