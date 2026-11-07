#!/usr/bin/env python3
"""mkassets.py DATADIR OUT.c -- Super Blit Kart's art, as C: upstream's PNGs,
.tmx maps and font (make sbk-data fetches them, data/README.md) become one
packed array that src/assets.c unpacks when the game starts. What 32blit's
tools do when upstream builds (image, font and map builders, and upstream's
extract_track_info.py), done the same way so the same colours land at the same
palette indices and the same bits in the fonts. Needs Pillow, as they do.

mkassets.py --ref DATADIR OUTDIR writes assets.cpp and assets.hpp instead, the
same pictures in the forms the 32blit SDK loads, for the original C++ game
that make sbk-ref builds to hold the rewrite to (ref/README.md).
"""
import io
import pathlib
import re
import struct
import sys
from xml.etree import ElementTree as ET

from PIL import Image, ImageFont

TRACKS = ['rainbow', 'desert']
TYPE = {'Static': 1, 'Item': 2}


class Palette:
    """ttblit.core.palette.Palette, the parts assets.yml reaches"""

    def __init__(self, path=None):
        self.transparent = None
        self.entries = []
        if path is not None:
            im = Image.open(path).convert('RGBA')
            w, h = im.size
            for y in range(h):
                for x in range(w):
                    self.entries.append(im.getpixel((x, y)))

    def get(self, c):
        if c in self.entries:
            return self.entries.index(c)
        if c[3] == 0 and self.transparent is not None:
            return self.transparent
        if len(self.entries) >= 256:
            raise TypeError('Out of palette entries')
        self.entries.append(c)
        if c[3] == 0:
            self.transparent = len(self.entries) - 1
        return len(self.entries) - 1


def load_image(path, palette=None):
    pal = Palette(palette)
    im = Image.open(path).convert('RGBA')
    w, h = im.size
    px = bytearray(w * h)
    for y in range(h):
        for x in range(w):
            px[x + y * w] = pal.get(im.getpixel((x, y)))
    return w, h, pal.entries, bytes(px)


def image_font(path, num_chars=96, spacing_h=1, spacing_v=0, space_width=4):
    """font builder, process_image_font"""
    image = Image.open(path).convert('1')
    w, h = image.size
    cw, ch = w // num_chars, h
    data = bytearray()
    widths = []
    for c in range(num_chars):
        char_w = 0
        for x in range(cw):
            byte = 0
            for y in range(ch):
                bit = y % 8
                if bit == 0 and y > 0:
                    data.append(byte)
                    byte = 0
                if image.getpixel((x + c * cw, y)) != 0:
                    byte |= 1 << bit
                    char_w = max(char_w, x + 1)
            data.append(byte)
        widths.append(space_width if c == 0 else char_w + spacing_h)
    return num_chars, cw, ch, spacing_v, bytes(widths), bytes(data)


def ttf_font(path, height, num_chars=96, base=32, spacing_v=1):
    """font builder, process_ft_font: FreeType's monochrome glyphs, through
    Pillow rather than freetype-py"""
    font = ImageFont.truetype(str(path), size=height)
    glyphs = []
    widths = []
    char_w = char_h = 0
    min_y = height
    for c in range(num_chars):
        ch = chr(c + base)
        mask, (left, off) = font.getmask2(ch, mode='1', anchor='ls')
        top = -off
        gw, gh = mask.size
        if ch == ' ' or gw == 0 or gh == 0:
            gw = gh = 0
            top = left = 0
        widths.append(int(font.getlength(ch, mode='1')))
        glyphs.append((mask, left, top, gw, gh))
        char_w = max(char_w, gw + left)
        char_h = max(char_h, (height - top) + gh)
        min_y = min(min_y, height - top)
    char_h -= min_y
    data = bytearray()
    for mask, left, top, gw, gh in glyphs:
        x_off = left
        y_off = height - top - min_y
        for x in range(char_w):
            byte = 0
            for y in range(char_h):
                bit = y % 8
                if bit == 0 and y > 0:
                    data.append(byte)
                    byte = 0
                if x < x_off or x - x_off >= gw or y < y_off or y - y_off >= gh:
                    continue
                if mask.getpixel((x - x_off, y - y_off)):
                    byte |= 1 << bit
            data.append(byte)
    return num_chars, char_w, char_h, spacing_v, bytes(widths), bytes(data)


def parse_points(s):
    return [[int(v) for v in p.split(',')] for p in s.split(' ')]


def template_props(path):
    root = ET.parse(path)
    return {p.get('name'): p.get('value') for p in root.findall('.//property')}


def load_track(data, name):
    """the map's tiles (map builder) and what extract_track_info.py takes from it"""
    tmx = data / (name + '.tmx')
    root = ET.parse(tmx).getroot()
    tileset = data / root.find('tileset').get('source')
    layers = sorted(root.findall('layer'), key=lambda l: int(l.get('id')))
    raw = [int(v) for v in layers[0].find('data').text.replace('\n', '').split(',') if v.strip()]
    tiles = bytes(0 if v == 0 else (v & 0x1FFFFFFF) - 1 for v in raw)
    trans = bytes(v >> 29 for v in raw)
    w, h = int(root.get('width')), int(root.get('height'))

    bg = root.get('backgroundcolor')
    bg = (0, 0, 0) if bg is None else tuple(int(bg[i:i + 2], 16) for i in (1, 3, 5))

    finish, route, sprites = [], [], []
    for obj in root.findall('.//objectgroup[@name="meta"]/object'):
        n = obj.get('name')
        x, y = int(obj.get('x')), int(obj.get('y'))
        if n == 'finish':
            finish = [[p[0] + x, p[1] + y] for p in parse_points(obj.find('polyline').get('points'))]
        elif n == 'route':
            pts = parse_points(obj.find('polygon').get('points'))
            pts.append(pts[0])
            route = [[p[0] + x, p[1] + y] for p in pts]
        elif obj.get('template'):
            props = template_props(data / obj.get('template'))
            if 'sprite' in props:
                sx, sy, sw, sh = [int(v) for v in props['sprite'].split(',')]
                ox, oy = [int(v) for v in props.get('origin', '0,0').split(',')]
                sprites.append([TYPE[props.get('type', 'Static')], int(float(props.get('scale', 1.0)) * 16),
                                x, y, sx, sy, sw, sh, ox, oy])
    rects = [[int(o.get(k)) for k in ('x', 'y', 'width', 'height')]
             for o in root.findall('.//objectgroup[@name="collisions"]/object')] or [[0, 0, 0, 0]]

    friction = [0.0] + [1.0] * 255
    for t in ET.parse(tileset).getroot().findall('tile'):
        p = t.find('.//property[@name="friction"]')
        if p is not None:
            friction[int(t.get('id'))] = float(p.get('value'))
    while friction[-1] == 1.0:
        friction.pop()
    return dict(name=name, w=w, h=h, tiles=tiles, trans=trans if any(trans) else None, bg=bg, finish=finish,
                route=route, sprites=sprites, rects=rects, friction=friction, tileset=tileset.stem)


def alt_palette(path):
    text = pathlib.Path(path).read_text()
    vals = [int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]{2})', text)]
    return vals[:1024] + [0] * (1024 - min(len(vals), 1024))


# ---- the array src/assets.c reads ----

def rle(b):
    """PackBits: n < 128, n + 1 bytes as they are; n > 128, the next byte 257 - n times"""
    out = bytearray()
    i, n = 0, len(b)
    while i < n:
        j = i
        while j + 1 < n and b[j + 1] == b[i] and j - i < 127:
            j += 1
        if j > i:
            out += bytes([257 - (j - i + 1), b[i]])
            i = j + 1
            continue
        j = i + 1
        while j < n and j - i < 128 and not (j + 2 < n and b[j] == b[j + 1] == b[j + 2]):
            j += 1
        out += bytes([j - i - 1]) + b[i:j]
        i = j
    return bytes(out)


def s32(*v):
    return struct.pack('<%di' % len(v), *v)


def pack(data):
    out = bytearray()

    def image(w, h, pal, px):
        nonlocal out
        out += s32(w, h, len(pal))
        for c in pal:
            out += bytes(c)
        r = rle(px)
        out += s32(len(r)) + r

    def font(num, cw, ch, sp, widths, bits):
        nonlocal out
        out += s32(num, cw, ch, sp) + widths
        r = rle(bits)
        out += s32(len(bits), len(r)) + r

    image(*load_image(data / 'tileset.png'))
    image(*load_image(data / 'track.png'))
    image(*load_image(data / 'kart.png', data / 'kart_robot_rust_palette.png'))
    image(*load_image(data / 'sbklogo.png'))
    image(*load_image(data / 'menu_sprites.png'))
    image(*load_image(data / 'kart_icons.png'))
    out += bytes(alt_palette(data / 'palettes.h'))
    font(*image_font(data / '8x12font.png'))
    font(*ttf_font(data / 'RandomFont3.ttf', 54, 26))
    font(*ttf_font(data / 'RandomFont3.ttf', 24))
    for name in TRACKS:
        t = load_track(data, name)
        nb = t['name'].encode()
        out += s32(len(nb)) + nb
        out += s32(t['w'], t['h'], t['bg'][0], t['bg'][1], t['bg'][2], 0 if t['tileset'] == 'tileset' else 1)
        for p in t['finish'][:2]:
            out += s32(*p)
        out += s32(len(t['route']))
        for p in t['route']:
            out += s32(*p)
        out += s32(len(t['rects']))
        for p in t['rects']:
            out += s32(*p)
        out += s32(len(t['sprites']))
        for p in t['sprites']:
            out += s32(*p)
        out += s32(len(t['friction'])) + struct.pack('<%df' % len(t['friction']), *t['friction'])
        r = rle(t['tiles'])
        out += s32(len(r)) + r
        if t['trans'] is None:
            out += s32(-1)
        else:
            r = rle(t['trans'])
            out += s32(len(r)) + r
    return bytes(out)


def c_string(blob):
    """one literal, in lines: printable bytes as they are, the rest in octal"""
    lines, cur, n = [], [], 0
    for b in blob:
        if 32 <= b < 127 and b not in (34, 63, 92):
            s = chr(b)
        else:
            s = '\\%03o' % b
        cur.append(s)
        n += len(s)
        if n >= 96:
            lines.append('"' + ''.join(cur) + '"')
            cur, n = [], 0
    if cur:
        lines.append('"' + ''.join(cur) + '"')
    return '\n'.join(lines)


def write_c(data, out):
    blob = pack(data)
    with open(out, 'w') as f:
        f.write('// generated by c4/mkassets.py from data/, not committed\n')
        f.write('int sbk_blob_len = %d;\n' % len(blob))
        f.write('char *sbk_blob =\n%s;\n' % c_string(blob))


# ---- the same, for the SDK ----

def raw_image(w, h, pal, px):
    """SPRITERW: what the tools write unpacked, which Surface::load takes"""
    body = b''.join(bytes(c) for c in pal) + px
    n = len(pal)
    return b'SPRITERW' + struct.pack('<IHHBB', 18 + len(body), w, h, 2, 0 if n == 256 else n) + body


def font_asset(num, cw, ch, sp, widths, bits):
    return b'FONT' + bytes([num, cw, ch, sp]) + widths + bits


def map_asset(t):
    flags = 2 if t['trans'] is not None else 0
    return struct.pack('<4sHHHHHH', b'MTMX', 16, flags, 0, t['w'], t['h'], 1) + t['tiles'] + (t['trans'] or b'')


def write_ref(data, outdir):
    assets = [
        ('tileset_tiles', raw_image(*load_image(data / 'tileset.png'))),
        ('track_tiles', raw_image(*load_image(data / 'track.png'))),
        ('kart', raw_image(*load_image(data / 'kart.png', data / 'kart_robot_rust_palette.png'))),
        ('logo', raw_image(*load_image(data / 'sbklogo.png'))),
        ('menu_sprites', raw_image(*load_image(data / 'menu_sprites.png'))),
        ('kart_icons', raw_image(*load_image(data / 'kart_icons.png'))),
        ('tall_font', font_asset(*image_font(data / '8x12font.png'))),
        ('eight_font', font_asset(*image_font(data / '8x8font.png'))),
        ('big_number_font', font_asset(*ttf_font(data / 'RandomFont3.ttf', 54, 26))),
        ('medium_number_font', font_asset(*ttf_font(data / 'RandomFont3.ttf', 27, 26))),
        ('menu_font', font_asset(*ttf_font(data / 'RandomFont3.ttf', 24))),
    ] + [(n + '_map', map_asset(load_track(data, n))) for n in TRACKS]
    outdir = pathlib.Path(outdir)
    with open(outdir / 'assets.hpp', 'w') as f:
        f.write('#pragma once\n#include <cstdint>\n')
        for n, _ in assets:
            f.write('extern const uint8_t asset_%s[];\n' % n)
    with open(outdir / 'assets.cpp', 'w') as f:
        f.write('#include "assets.hpp"\n')
        for n, b in assets:
            f.write('const uint8_t asset_%s[] = {%s};\n' % (n, ','.join(str(v) for v in b)))


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == '--ref':
        write_ref(pathlib.Path(sys.argv[2]), sys.argv[3])
    elif len(sys.argv) == 3:
        write_c(pathlib.Path(sys.argv[1]), sys.argv[2])
    else:
        sys.exit(__doc__)
