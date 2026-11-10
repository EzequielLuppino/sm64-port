#!/usr/bin/env python3
"""Generate the bottom-screen UI label textures for the 3DS port.

The textures are white text on a transparent background, drawn with a
hand-rolled 5x7 pixel font scaled 2x (10x14 glyphs).  Every canvas is a
power of two because the t3x/GPU pipeline expects POT textures.
"""

from PIL import Image

FONT = {
    'A': ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    'B': ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    'C': ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
    'D': ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    'E': ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    'F': ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    'G': ["01110", "10001", "10000", "10111", "10001", "10001", "01111"],
    'H': ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    'I': ["01110", "00100", "00100", "00100", "00100", "00100", "01110"],
    'J': ["00111", "00010", "00010", "00010", "00010", "10010", "01100"],
    'K': ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    'L': ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    'M': ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    'N': ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    'O': ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    'P': ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    'Q': ["01110", "10001", "10001", "10001", "10101", "10010", "01101"],
    'R': ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    'S': ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    'T': ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    'U': ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    'V': ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    'W': ["10001", "10001", "10001", "10101", "10101", "10101", "01010"],
    'X': ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
    'Y': ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    'Z': ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
    '0': ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    '1': ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    '2': ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    '3': ["11110", "00001", "00001", "01110", "00001", "00001", "11110"],
    '4': ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    '5': ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
    '6': ["01110", "10000", "10000", "11110", "10001", "10001", "01110"],
    '7': ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    '8': ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    '9': ["01110", "10001", "10001", "01111", "00001", "00001", "01110"],
    ' ': ["00000", "00000", "00000", "00000", "00000", "00000", "00000"],
    '-': ["00000", "00000", "00000", "11111", "00000", "00000", "00000"],
    ':': ["00000", "00100", "00100", "00000", "00100", "00100", "00000"],
    '(': ["00010", "00100", "01000", "01000", "01000", "00100", "00010"],
    ')': ["01000", "00100", "00010", "00010", "00010", "00100", "01000"],
    '.': ["00000", "00000", "00000", "00000", "00000", "00100", "00100"],
    '/': ["00001", "00010", "00010", "00100", "01000", "01000", "10000"],
}

SCALE = 2


def text_width(text, scale=SCALE):
    advance = 6 * scale  # 5px glyph + 1px letter spacing
    return len(text) * advance - scale if text else 0


def draw_text(img, text, x, y, scale=SCALE):
    px = img.load()
    advance = 6 * scale
    for ch in text:
        glyph = FONT[ch]
        for row in range(7):
            for col in range(5):
                if glyph[row][col] == '1':
                    for sy in range(scale):
                        for sx in range(scale):
                            px[x + col * scale + sx, y + row * scale + sy] = (255, 255, 255, 255)
        x += advance


def make_texture(name, width, height, text, x=None, y=None, scale=SCALE):
    img = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    if x is None:
        x = (width - text_width(text, scale)) // 2
    if y is None:
        y = (height - 7 * scale) // 2
    draw_text(img, text, x, y, scale)
    path = 'src/minimap/textures/%s.png' % name
    img.save(path)
    print('wrote', path, img.size, repr(text))


def main():
    # Tab bar labels (128x32 canvas, drawn as 102x26 in the UI)
    make_texture('tab_map', 128, 32, 'MAP', scale=3)
    make_texture('tab_cheats', 128, 32, 'CHEATS', scale=3)
    make_texture('tab_setup', 128, 32, 'SETUP', scale=3)
    make_texture('tab_states', 128, 32, 'STATES', scale=3)

    # Cheat row labels (256x16 canvas, left aligned)
    labels = [
        ('lbl_inf_health', 'INF HEALTH'),
        ('lbl_inf_air', 'INF AIR'),
        ('lbl_inf_lives', 'INF LIVES'),
        ('lbl_no_damage', 'NO DAMAGE'),
        ('lbl_moon_jump', 'MOON JUMP (A)'),
        ('lbl_enemies_ignore', 'ENEMIES IGNORE'),
        ('lbl_power_none', 'POWER-UP: NONE'),
        ('lbl_power_wing', 'POWER-UP: WING'),
        ('lbl_power_metal', 'POWER-UP: METAL'),
        ('lbl_power_vanish', 'POWER-UP: VANISH'),
    ]
    for name, text in labels:
        make_texture(name, 256, 16, text, x=8, y=1)

    # ON / OFF indicators (64x16 canvas)
    make_texture('lbl_on', 64, 16, 'ON')
    make_texture('lbl_off', 64, 16, 'OFF')

    # Savestate slot labels (128x16 canvas, left aligned) and buttons.
    for slot in range(1, 4):
        make_texture('lbl_slot%d' % slot, 128, 16, 'SLOT %d' % slot, x=8, y=1)
    make_texture('lbl_save', 64, 16, 'SAVE')
    make_texture('lbl_load', 64, 16, 'LOAD')


if __name__ == '__main__':
    main()
