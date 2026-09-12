#!/usr/bin/env python3
"""Build the SD-card quote database for the Literary Clock.

Reads data/litclock_annotated.csv (pipe-delimited, from the open
literature-clock project, CC BY-NC-SA 2.5) plus data/extra_quotes.csv if
present (your own curated additions, same format) and writes:

  sdcard/literaryclock/quotes.txt   one quote per line, ASCII only:
        HH:MM|quote text with {time phrase} marked|Title|Author|S
        S is 1 for safe-for-work, 0 for not, ? for unknown.
        Paragraph breaks inside a quote are written as the two
        characters backslash-n.
  sdcard/literaryclock/quotes.idx   binary index: "LCIX", uint32 size of
        quotes.txt, then 1440 x (uint32 byte offset, uint16 line count),
        little-endian, one entry per minute of the day.

The firmware only reads quotes.txt via this index, so the two files must
be copied together. The firmware rebuilds the index itself if the size
recorded in quotes.idx does not match quotes.txt.
"""
import csv
import re
import struct
import sys
import unicodedata
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "data" / "litclock_annotated.csv"
EXTRA = ROOT / "data" / "extra_quotes.csv"   # curated additions from tools/mine_gutenberg.py --merge
OUT_DIR = ROOT / "sdcard" / "literaryclock"
MAX_QUOTE_CHARS = 450   # longer quotes cannot fit a 320x240 panel legibly

# Characters that survive NFKD normalisation but are outside ASCII.
REPLACEMENTS = {
    "\u2019": "'", "\u2018": "'", "\u201c": '"', "\u201d": '"',
    "\u2014": " - ", "\u2013": "-", "\u2010": "-", "\u2212": "-",
    "\u2044": "/", "\u00b0": " degrees", "\u00a3": "GBP ", "\u00f8": "o",
    "\u00e6": "ae", "\u00c6": "AE", "\u0153": "oe", "\u0152": "OE",
    "\u00df": "ss", "\u2026": "...",
}
BR_RE = re.compile(r"<br\s*/?>", re.I)
TAG_RE = re.compile(r"</?time/?>", re.I)
SPACE_RE = re.compile(r"[ \t]+")


def to_ascii(s: str) -> str:
    s = unicodedata.normalize("NFKD", s)          # also unfolds math-italic letters
    s = "".join(c for c in s if not unicodedata.combining(c))
    s = "".join(REPLACEMENTS.get(c, c) for c in s)
    bad = {c for c in s if ord(c) > 126 or (ord(c) < 32 and c != "\n")}
    if bad:
        raise ValueError(f"unmapped characters {bad!r} in {s[:60]!r}")
    return s


def clean_quote(q: str) -> str:
    q = BR_RE.sub("\n", q)
    q = TAG_RE.sub("", q)
    q = q.replace("{", "(").replace("}", ")")     # braces mark the time phrase
    q = to_ascii(q)
    q = SPACE_RE.sub(" ", q)
    q = "\n".join(line.strip() for line in q.split("\n"))
    q = re.sub(r"\n{2,}", "\n", q).strip()
    return q


def mark_phrase(quote: str, phrase: str) -> str:
    """Wrap the first case-insensitive occurrence of phrase in braces."""
    i = quote.lower().find(phrase.lower())
    if i < 0:
        return None
    return quote[:i] + "{" + quote[i:i + len(phrase)] + "}" + quote[i + len(phrase):]


def sfw_flag(v: str) -> str:
    v = v.strip().lower()
    if v == "sfw":
        return "1"
    if v in ("nsfw", "nswf"):
        return "0"
    return "?"


def main() -> int:
    per_minute = defaultdict(list)
    dropped_phrase = dropped_long = 0
    sources = [SRC] + ([EXTRA] if EXTRA.exists() else [])
    rows = []
    for src in sources:
        with src.open(encoding="utf-8", newline="") as f:
            rows += list(csv.reader(f, delimiter="|", quoting=csv.QUOTE_NONE))
    if True:
        for row in rows:
            if len(row) < 5 or not re.fullmatch(r"\d\d:\d\d", row[0]):
                continue
            hhmm, phrase, quote, title, author = (c.strip() for c in row[:5])
            sfw = sfw_flag(row[5]) if len(row) > 5 else "?"
            quote = clean_quote(quote)
            phrase = clean_quote(phrase)
            marked = mark_phrase(quote, phrase)
            if marked is None:
                dropped_phrase += 1
                continue
            title = to_ascii(title).replace("|", "/")
            author = to_ascii(author).replace("|", "/")
            per_minute[hhmm].append((len(quote), marked, title, author, sfw))

    # Length cap, but never empty a minute that has only long quotes.
    lines_by_minute = {}
    for hhmm, items in per_minute.items():
        keep = [i for i in items if i[0] <= MAX_QUOTE_CHARS]
        if not keep:
            keep = [min(items)]
        dropped_long += len(items) - len(keep)
        lines_by_minute[hhmm] = [
            f"{hhmm}|{m.replace(chr(10), chr(92) + 'n')}|{t}|{a}|{s}" for _, m, t, a, s in keep
        ]

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    txt_path = OUT_DIR / "quotes.txt"
    idx_path = OUT_DIR / "quotes.idx"
    index = []
    with txt_path.open("wb") as out:
        for h in range(24):
            for m in range(60):
                hhmm = f"{h:02d}:{m:02d}"
                lines = lines_by_minute.get(hhmm, [])
                index.append((out.tell(), len(lines)))
                for line in lines:
                    out.write(line.encode("ascii") + b"\n")
        size = out.tell()
    with idx_path.open("wb") as out:
        out.write(b"LCIX" + struct.pack("<I", size))
        for off, cnt in index:
            out.write(struct.pack("<IH", off, cnt))

    # Self-checks.
    data = txt_path.read_bytes()
    assert all(b < 128 for b in data), "non-ASCII byte in output"
    for off, cnt in index:
        pos = off
        for _ in range(cnt):
            end = data.index(b"\n", pos)
            fields = data[pos:end].split(b"|")
            assert len(fields) == 5, fields
            assert fields[1].count(b"{") == 1 and fields[1].count(b"}") == 1, fields[1]
            pos = end + 1
    covered = sum(1 for _, c in index if c)
    total = sum(c for _, c in index)
    missing = [f"{i // 60:02d}:{i % 60:02d}" for i, (_, c) in enumerate(index) if not c]
    print(f"wrote {txt_path} ({size} bytes, {total} quotes)")
    print(f"wrote {idx_path} ({idx_path.stat().st_size} bytes)")
    print(f"minutes covered: {covered}/1440; missing: {' '.join(missing) or 'none'}")
    print(f"dropped: {dropped_phrase} phrase-not-found, {dropped_long} over {MAX_QUOTE_CHARS} chars")
    return 0


if __name__ == "__main__":
    sys.exit(main())
