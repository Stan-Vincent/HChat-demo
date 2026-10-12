# -*- coding: utf-8 -*-
"""把已换成珊瑚橘的 19 个 res 图标整体淡化：向白色混合 30%。
当前是主色级饱和橙（~#ff7a45），做按压/激活态图标偏重；
混合后约 #ffa684（淡珊瑚橘），和聊天气泡的 #ffd0b8 同一族。
只动 RGB，alpha 和形状不变。"""
import os

import numpy as np
from PIL import Image

FILES = [
    'add_friend.png', 'addtip.png', 'chat_icon_press.png',
    'chat_icon_select_press.png', 'contact_list_press.png',
    'filedir_press.png', 'msg_chat_hover.png', 'msg_chat_normal.png',
    'msg_chat_press.png', 'readed.png', 'settings_press.png',
    'settings_select_press.png', 'tipclose.png', 'video_chat_hover.png',
    'video_chat_normal.png', 'video_chat_press.png', 'voice_chat_hover.png',
    'voice_chat_normal.png', 'voice_chat_press.png',
]

T = 0.3  # 向白色混合比例

for name in FILES:
    p = os.path.join('res', name)
    img = Image.open(p).convert('RGBA')
    arr = np.asarray(img).astype(np.float64)
    a = arr[..., 3:]
    rgb = arr[..., :3]
    arr[..., :3] = rgb * (1.0 - T) + 255.0 * T
    Image.fromarray(arr.astype(np.uint8), 'RGBA').save(p)
    # 取一个中心不透明像素展示
    h, w = a.shape[0], a.shape[1]
    sample = arr[h // 2, w // 2]
    print('%-32s sample rgb(%d,%d,%d)' % (name, sample[0], sample[1], sample[2]))
print('lightened %d icons, t=%s' % (len(FILES), T))
