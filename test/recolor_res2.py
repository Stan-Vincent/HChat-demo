# -*- coding: utf-8 -*-
"""res 图标换色第二版：从备份恢复，按族统一替换为淡珊瑚橘。
上一版色相映射保持原明度(V=0.76)导致结果偏深(#d36e51 赤陶色)，
且这些图标是纯色图形(93%+ 单色)，直接统一替换目标色最干净。
alpha 通道保留，抗锯齿边缘自然过渡。
两族目标色（保留原来的相对明暗关系）：
  148.7 族(微信绿族) -> #ffab7e 淡珊瑚橘
   92.9 族(msg/video/voice) -> #ffc4a0 更淡一档
"""
import os
import shutil

from PIL import Image

GROUP_A = [  # 原色相 148.7 -> 淡珊瑚橘
    'add_friend.png', 'addtip.png', 'chat_icon_press.png',
    'chat_icon_select_press.png', 'contact_list_press.png',
    'filedir_press.png', 'settings_press.png',
    'settings_select_press.png', 'tipclose.png', 'readed.png',
]
GROUP_B = [  # 原色相 92.9 -> 更淡一档
    'msg_chat_hover.png', 'msg_chat_normal.png', 'msg_chat_press.png',
    'video_chat_hover.png', 'video_chat_normal.png', 'video_chat_press.png',
    'voice_chat_hover.png', 'voice_chat_normal.png', 'voice_chat_press.png',
]
COLOR_A = (255, 171, 126)   # #ffab7e
COLOR_B = (255, 196, 160)   # #ffc4a0

BACKUP = os.path.join('..', 'test', '_res_backup')

for name, color in [(n, COLOR_A) for n in GROUP_A] + [(n, COLOR_B) for n in GROUP_B]:
    src = os.path.join(BACKUP, name)
    dst = os.path.join('res', name)
    if not os.path.exists(src):
        print('SKIP (no backup):', name)
        continue
    shutil.copyfile(src, dst)  # 从原始绿色恢复
    img = Image.open(dst)
    img.load()
    rgba = img.convert('RGBA')
    data = bytearray(rgba.tobytes())
    # 只替换 RGB，alpha 原样保留
    for i in range(0, len(data), 4):
        if data[i + 3] > 0:
            data[i] = color[0]
            data[i + 1] = color[1]
            data[i + 2] = color[2]
    out = Image.frombytes('RGBA', rgba.size, bytes(data))
    out.save(dst)
    print('%-32s -> rgb%s' % (name, color))
print('done, %d icons' % (len(GROUP_A) + len(GROUP_B)))
