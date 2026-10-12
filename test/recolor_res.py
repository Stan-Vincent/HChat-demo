# -*- coding: utf-8 -*-
"""res/ 绿色图标 -> 珊瑚橘色（HSV 色相重映射）。
映射：绿色相域 [60,180] 线性压到橘域 [8,31]（H_new = 43 - 0.2*H_old），
148.7 的微信绿 -> ~13 正珊瑚橘，92.9 那族 -> ~24 橙。饱和度/明度/alpha 不变。
"""
import glob
import os

import numpy as np
from PIL import Image

GREEN_FILES = [
    'add_friend.png', 'addtip.png', 'chat_icon_press.png',
    'chat_icon_select_press.png', 'contact_list_press.png',
    'filedir_press.png', 'msg_chat_hover.png', 'msg_chat_normal.png',
    'msg_chat_press.png', 'readed.png', 'settings_press.png',
    'settings_select_press.png', 'tipclose.png', 'video_chat_hover.png',
    'video_chat_normal.png', 'video_chat_press.png', 'voice_chat_hover.png',
    'voice_chat_normal.png', 'voice_chat_press.png',
]


def rgb_to_hsv_arr(rgb):
    """rgb float [0,1] (...,3) -> hsv, h in [0,360), s,v in [0,1]"""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx = np.max(rgb, axis=-1)
    mn = np.min(rgb, axis=-1)
    diff = mx - mn
    h = np.zeros_like(mx)
    mask = diff > 1e-9
    idx = mask
    # 标准 HSV 色相计算
    r_eq, g_eq, b_eq = (mx == r) & idx, (mx == g) & idx, (mx == b) & idx
    h = np.where(r_eq, (60 * ((g - b) / np.where(diff == 0, 1, diff))) % 360, h)
    h = np.where(g_eq, (60 * ((b - r) / np.where(diff == 0, 1, diff)) + 120), h)
    h = np.where(b_eq, (60 * ((r - g) / np.where(diff == 0, 1, diff)) + 240), h)
    s = np.where(mx > 1e-9, diff / np.where(mx == 0, 1, mx), 0)
    return h, s, mx


def hsv_to_rgb_arr(h, s, v):
    """h [0,360), s,v [0,1] -> rgb [0,1]"""
    c = v * s
    hp = h / 60.0
    x = c * (1 - np.abs(hp % 2 - 1))
    m = v - c
    rgb = np.zeros(h.shape + (3,), dtype=np.float64)
    for i, (cond, triple) in enumerate([
        ((hp >= 0) & (hp < 1), (c, x, 0)),
        ((hp >= 1) & (hp < 2), (x, c, 0)),
        ((hp >= 2) & (hp < 3), (0, c, x)),
        ((hp >= 3) & (hp < 4), (0, x, c)),
        ((hp >= 4) & (hp < 5), (x, 0, c)),
        ((hp >= 5) & (hp < 6), (c, 0, x)),
    ]):
        for j in range(3):
            rgb[..., j] = np.where(cond, triple[j] + m, rgb[..., j])
    return rgb


def recolor(path):
    img = Image.open(path).convert('RGBA')
    arr = np.asarray(img).astype(np.float64)
    alpha = arr[..., 3:]
    rgb = arr[..., :3] / 255.0
    h, s, v = rgb_to_hsv_arr(rgb)

    # 绿色判定：色相 60~180 且饱和度足够 且不透明
    is_green = (h >= 60) & (h <= 180) & (s > 0.15) & (alpha[..., 0] > 8)

    # 线性映射到橘域：H_new = 43 - 0.2 * H
    h_new = np.where(is_green, 43.0 - 0.2 * h, h)

    out = hsv_to_rgb_arr(h_new, s, v)
    arr[..., :3] = np.clip(out * 255.0, 0, 255)
    Image.fromarray(arr.astype(np.uint8), 'RGBA').save(path)
    return int(is_green.sum())


total = 0
for name in GREEN_FILES:
    p = os.path.join('res', name)
    if not os.path.exists(p):
        print('SKIP (missing):', name)
        continue
    n = recolor(p)
    total += n
    print('%-32s %6d px recolored' % (name, n))
print('total green px recolored:', total)
