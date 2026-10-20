#!/usr/bin/env python
# 生成 Cookie 抓取工具的应用图标（32x32 32bpp ICO，纯 Python 无依赖）
import struct
import math

W = H = 32

def make_pixels():
    px = [[(0, 0, 0, 0) for _ in range(W)] for _ in range(H)]
    cx, cy, r = 15.5, 15.5, 14.0
    chips = [(11, 10), (19, 12), (13, 19), (21, 21), (16, 15), (9, 22)]
    for y in range(H):
        for x in range(W):
            d = math.hypot(x - cx, y - cy)
            if d <= r:
                edge = d > r - 1.2
                if edge:
                    px[y][x] = (60, 90, 120, 255)      # 深色描边 BGR
                else:
                    px[y][x] = (89, 139, 198, 255)     # 饼干棕 BGR
    for (chx, chy) in chips:
        for y in range(H):
            for x in range(W):
                if math.hypot(x - chx, y - chy) <= 1.6:
                    if px[y][x][3] == 255 and math.hypot(x - 15.5, y - 15.5) <= r - 1.2:
                        px[y][x] = (55, 64, 93, 255)   # 巧克力豆 BGR
    return px

def write_ico(path):
    px = make_pixels()
    # XOR 位图：自底向上 BGRA
    xor = bytearray()
    for y in range(H - 1, -1, -1):
        for x in range(W):
            b, g, r_, a = px[y][x]
            xor += bytes((b, g, r_, a))
    # AND 掩码：32bpp 带 alpha 时全 0（每行 4 字节，32 像素 -> 4 字节）
    andmask = bytearray((W + 7) // 8 * H)  # 全零
    # 行需按 4 字节对齐：32 位 = 4 字节，已对齐
    image = bytearray()
    bih = struct.pack('<IiiHHIIiiII', 40, W, H * 2, 1, 32, 0, len(xor) + len(andmask), 0, 0, 0, 0)
    image += bih + xor + andmask
    header = struct.pack('<HHH', 0, 1, 1)
    entry = struct.pack('<BBBBHHII', W, H, 0, 0, 1, 32, len(image), 6 + 16)
    with open(path, 'wb') as f:
        f.write(header + entry + image)

if __name__ == '__main__':
    write_ico(r'D:\itemi\cookie\res\app.ico')
    print('app.ico written')
