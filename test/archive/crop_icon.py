# -*- coding: utf-8 -*-
"""裁掉 res/Icon.png 的留白，让猫图案撑满画布（提升小尺寸可读性），
并重新生成多尺寸 icon.ico。"""
from PIL import Image

img = Image.open('res/Icon.png').convert('RGBA')
w, h = img.size

# 内容边界（暗于 200 的像素 = 猫的线条）
gray = img.convert('L')
mask = gray.point(lambda p: 255 if p < 200 else 0)
l, t, r, b = mask.getbbox()
print('content bbox:', (l, t, r, b))

# 加 ~4% 边距，避免图案贴边
margin = int(max(r - l, b - t) * 0.04)
l = max(0, l - margin)
t = max(0, t - margin)
r = min(w, r + margin)
b = min(h, b + margin)

# 裁成正方形（以内容中心，取较长边）
cw, ch = r - l, b - t
side = max(cw, ch)
cx, cy = (l + r) // 2, (t + b) // 2
l = max(0, cx - side // 2)
t = max(0, cy - side // 2)
r = min(w, l + side)
b = min(h, t + side)
l = max(0, r - side)
t = max(0, b - side)

cropped = img.crop((l, t, r, b))
print('cropped to:', cropped.size)

# 保存 PNG（不放大，保留原始线条清晰度；QIcon 会自动缩放）
cropped.save('res/Icon.png')
print('saved res/Icon.png')

# 重新生成多尺寸 ico
sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
cropped.save('icon.ico', format='ICO', sizes=sizes)
print('saved icon.ico')

# 验证
back = Image.open('res/Icon.png')
print('Icon.png now:', back.size)
g2 = back.convert('L').point(lambda p: 255 if p < 200 else 0)
bb2 = g2.getbbox()
print('content now occupies %.0f%% x %.0f%%' % ((bb2[2]-bb2[0])/back.size[0]*100, (bb2[3]-bb2[1])/back.size[1]*100))
