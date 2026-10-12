# -*- coding: utf-8 -*-
"""把 res/Icon.png 的白色背景抠成透明（flood fill 从边缘开始，
淹没与边缘连通的近白区域；被黑线包围的猫内部白色保留为前景）。"""
from collections import deque
from PIL import Image

img = Image.open('res/Icon.png').convert('RGBA')
w, h = img.size
px = img.load()


def is_bg(p):
    r, g, b, a = p
    return a > 0 and r > 240 and g > 240 and b > 240


visited = bytearray(w * h)
q = deque()

# 四边作为种子
for x in range(w):
    for y in (0, h - 1):
        if is_bg(px[x, y]) and not visited[y * w + x]:
            visited[y * w + x] = 1
            q.append((x, y))
for y in range(h):
    for x in (0, w - 1):
        if is_bg(px[x, y]) and not visited[y * w + x]:
            visited[y * w + x] = 1
            q.append((x, y))

while q:
    x, y = q.popleft()
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nx, ny = x + dx, y + dy
        if 0 <= nx < w and 0 <= ny < h and not visited[ny * w + nx] and is_bg(px[nx, ny]):
            visited[ny * w + nx] = 1
            q.append((nx, ny))

# 外部背景 -> 透明
for y in range(h):
    for x in range(w):
        if visited[y * w + x]:
            r, g, b, a = px[x, y]
            px[x, y] = (r, g, b, 0)

img.save('res/Icon.png')
print('done. remaining opaque (rough):', (w * h - sum(visited)))
