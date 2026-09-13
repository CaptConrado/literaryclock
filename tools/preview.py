#!/usr/bin/env python3
"""Pixel-exact preview of the clock screen, rendered on the Mac.

Uses the very same Adafruit GFX bitmap fonts that TFT_eSPI compiles into the
firmware and mirrors display.cpp's layout rules, so what you see here is what
the panel shows. Handy for tuning spacing without reflashing.

  tools/preview.py                     # current minute
  tools/preview.py 08:15 23:59 13:36   # specific minutes (up to 4 on a sheet)
  tools/preview.py --out shot.png --scale 3
"""
import argparse
import datetime
import re
import struct
import sys
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
FONT_DIR = Path.home() / "Documents/Arduino/libraries/TFT_eSPI/Fonts/GFXFF"
CARD = ROOT / "sdcard" / "literaryclock"

# ---- layout constants: keep in step with display.cpp ----
W, H = 320, 240             # swapped by --portrait
MARGIN_X, TOP, FOOTER_H = 12, 10, 40
BODY_W = W - 2 * MARGIN_X
BODY_H = H - FOOTER_H - TOP - 4
SIZES = [("FreeSerif12pt7b", "FreeSerifBold12pt7b"), ("FreeSerif9pt7b", "FreeSerifBold9pt7b")]
LEADING = [2, 1]            # extra pixels added to each font's yAdvance (same as display.cpp)
PAPER, INK, ACCENT, GREY, RULE = (0xF4, 0xEE, 0xDD), (0x24, 0x22, 0x20), (0x8A, 0x1C, 0x1C), (0x80, 0x78, 0x70), (0xC9, 0xC0, 0xAE)


class GFXFont:
    """Parses an Adafruit GFX font .h file."""
    def __init__(self, name):
        src = (FONT_DIR / f"{name}.h").read_text()
        bm = re.search(r"Bitmaps\[\] PROGMEM = \{(.*?)\};", src, re.S).group(1)
        self.bitmaps = bytes(int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", bm))
        gl = re.search(r"Glyphs\[\] PROGMEM = \{(.*?)\};", src, re.S).group(1)
        self.glyphs = [tuple(int(v) for v in m) for m in re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+)\s*\}", gl)]
        hdr = re.search(r"GFXfont \w+ PROGMEM = \{.*?(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*(\d+)\s*\}", src, re.S)
        self.first, self.last, self.yAdvance = int(hdr.group(1), 16), int(hdr.group(2), 16), int(hdr.group(3))
        # TFT_eSPI setFreeFont: max ascent / descent over all glyphs
        self.ab = max(-yo for (_, _, h, _, _, yo) in self.glyphs)
        self.bb = max(h + yo for (_, _, h, _, _, yo) in self.glyphs)

    def glyph(self, ch):
        c = ord(ch)
        if c < self.first or c > self.last:
            return None
        return self.glyphs[c - self.first]

    def text_width(self, s):          # mirrors TFT_eSPI::textWidth for GFX fonts
        w = 0
        for i, ch in enumerate(s):
            g = self.glyph(ch)
            if not g:
                continue
            off, gw, gh, xadv, xo, yo = g
            w += xadv if i < len(s) - 1 else (xo + gw)
        return w

    def draw(self, img, x, y, s, colour):   # TL datum: y is the top of the tallest glyph
        px = img.load()
        base = y + self.ab
        for ch in s:
            g = self.glyph(ch)
            if not g:
                continue
            off, gw, gh, xadv, xo, yo = g
            bit = 0
            for yy in range(gh):
                for xx in range(gw):
                    byte = self.bitmaps[off + bit // 8]
                    if byte & (0x80 >> (bit % 8)):
                        X, Y = x + xo + xx, base + yo + yy
                        if 0 <= X < img.width and 0 <= Y < img.height:
                            px[X, Y] = colour
                    bit += 1
            x += xadv
        return x


FONTS = {}
def font(name):
    if name not in FONTS:
        FONTS[name] = GFXFont(name)
    return FONTS[name]


def load_quotes(minute):
    idx = (CARD / "quotes.idx").read_bytes()[8:]
    data = (CARD / "quotes.txt").read_bytes()
    off, cnt = struct.unpack_from("<IH", idx, minute * 6)
    out, pos = [], off
    for _ in range(cnt):
        end = data.index(b"\n", pos)
        out.append(data[pos:end].decode().split("|"))
        pos = end + 1
    return out


def tokenize(text):
    toks, cur, in_phrase, cur_bold = [], "", False, False
    def flush():
        nonlocal cur, cur_bold
        if cur:
            toks.append((cur, cur_bold, False)); cur = ""
        cur_bold = False
    for c in text.replace("\\n", "\n"):
        if c == "{": in_phrase = True; continue
        if c == "}": in_phrase = False; continue
        if c in " \n":
            flush()
            if c == "\n": toks.append(("", False, True))
            continue
        if in_phrase: cur_bold = True
        cur += c
    flush()
    return toks


def layout(toks, size):
    reg, bold = font(SIZES[size][0]), font(SIZES[size][1])
    space = reg.glyph(" ")[3]          # xAdvance of a space, as display.cpp measures it
    lines, cur, x = [], [], 0
    for word, b, para in toks:
        if para:
            lines.append(cur); cur, x = [], 0; continue
        w = (bold if b else reg).text_width(word)
        if x > 0 and x + w > BODY_W:
            lines.append(cur); cur, x = [], 0
        cur.append((x, word, b)); x += w + space
    if cur: lines.append(cur)
    return lines


def render(minute, show_time=True, hhmm=None):
    img = Image.new("RGB", (W, H), PAPER)
    d = ImageDraw.Draw(img)
    qs = load_quotes(minute)
    hhmm = hhmm or f"{minute // 60:02d}:{minute % 60:02d}"
    if not qs:
        big = font("FreeSerifBold24pt7b")
        big.draw(img, (W - big.text_width(hhmm)) // 2, H // 2 - 60, hhmm, ACCENT)
        sm = font("FreeSerif9pt7b")
        for i, t in enumerate(["No one has written about this minute yet.", "Add a line to personal.txt on the card."]):
            sm.draw(img, (W - sm.text_width(t)) // 2, H // 2 + 10 + 22 * i, t, GREY)
        d.line([(MARGIN_X, H - FOOTER_H), (W - MARGIN_X, H - FOOTER_H)], fill=RULE)
        return img
    def score(q):
        _, text, title, author, sfw = q
        n = len(text.replace("{", "").replace("}", "")); s = 0
        if 90 <= n <= 330: s += 3
        elif n < 60 or n > 400: s -= 2
        if sfw == "0": s -= 3
        elif sfw == "1": s += 1
        if "\\n" in text: s -= 1
        if text.startswith("{"): s -= 1
        return s
    qs.sort(key=score, reverse=True)
    _, text, title, author, sfw = qs[0]
    toks = tokenize(text)
    for size in range(len(SIZES)):
        lines = layout(toks, size)
        line_h = font(SIZES[size][0]).yAdvance + LEADING[size]
        if len(lines) * line_h <= BODY_H:
            break
    else:
        size = len(SIZES) - 1
        line_h = font(SIZES[size][0]).yAdvance + LEADING[size]
        lines = lines[: BODY_H // line_h]
        if lines and lines[-1]:
            x, w, b = lines[-1][-1]; lines[-1][-1] = (x, w + " ...", b)
    reg, bold = font(SIZES[size][0]), font(SIZES[size][1])
    y = TOP + (BODY_H - len(lines) * line_h) // 2
    for ln in lines:
        for x, word, b in ln:
            (bold if b else reg).draw(img, MARGIN_X + x, y, word, ACCENT if b else INK)
        y += line_h
    # footer
    d.line([(MARGIN_X, H - FOOTER_H), (W - MARGIN_X, H - FOOTER_H)], fill=RULE)
    it = font("FreeSerifItalic9pt7b")
    t = title
    while it.text_width(t) > BODY_W - 4 and len(t) > 4:
        t = t[:-4] + "..."
    it.draw(img, W - MARGIN_X - it.text_width(t), H - FOOTER_H + 3, t, INK)
    # built-in font 2 (16 px sans) approximated with the 9pt serif in grey
    sm = font("FreeSerif9pt7b")
    if show_time:
        sm.draw(img, MARGIN_X, H - 20, hhmm, GREY)
    sm.draw(img, W - MARGIN_X - sm.text_width(author), H - 20, author, GREY)
    return img


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("times", nargs="*", help="HH:MM values (default: now)")
    ap.add_argument("--out", default=str(ROOT / "docs" / "preview.png"))
    ap.add_argument("--scale", type=int, default=2)
    ap.add_argument("--portrait", action="store_true", help="render the 240x320 portrait layout")
    args = ap.parse_args()
    global W, H, BODY_W, BODY_H
    if args.portrait:
        W, H = 240, 320
        BODY_W, BODY_H = W - 2 * MARGIN_X, H - FOOTER_H - TOP - 4
    times = args.times or [datetime.datetime.now().strftime("%H:%M")]
    frames = []
    for t in times[:4]:
        h, m = map(int, t.split(":"))
        frames.append(render(h * 60 + m))
    S, pad = args.scale, 16
    cols = 1 if len(frames) == 1 else 2
    rows = (len(frames) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * W * S + (cols + 1) * pad, rows * H * S + (rows + 1) * pad), (0x30, 0x30, 0x30))
    for i, f in enumerate(frames):
        sheet.paste(f.resize((W * S, H * S), Image.NEAREST), (pad + (i % cols) * (W * S + pad), pad + (i // cols) * (H * S + pad)))
    sheet.save(args.out)
    print("wrote", args.out, "fonts:", {k: (v.yAdvance, v.ab, v.bb) for k, v in FONTS.items()})


if __name__ == "__main__":
    sys.exit(main())
