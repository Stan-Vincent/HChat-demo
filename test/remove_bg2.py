# -*- coding: utf-8 -*-
"""第二步：把猫身体内部的白色也按亮度抠成透明，只留黑线 + 灰色细节。
亮度越高越透明（纯白=透明，黑线=不透明），中间做软过渡避免白边锯齿。"""
from PIL import Image

img = Image.open('res/Icon.png').convert('RGBA')
w, h = img.size
px = img.load()

for y in range(h):
    for x in range(w):
        r, g, b, a = px[x, y]
        if a == 0:
            continue
        lum = (r * 299 + g * 587 + b * 114) // 1000
        if lum >= 250:
            alpha = 0
        elif lum <= 175:
            alpha = 255
        else:
            alpha = 255 * (250 - lum) // (250 - 175)
        px[x, y] = (r, g, b, alpha)

img.save('res/Icon.png')

# 统计
opaque = sum(1 for y in range(h) for x in range(w) if px[x, y][3] > 128)
print('done. opaque pixels:', opaque, 'of', w * h)
