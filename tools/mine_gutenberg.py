#!/usr/bin/env python3
"""Mine Project Gutenberg books for sentences that mention an exact time of day.

Usage:
  tools/mine_gutenberg.py                      # mine every id in data/gutenberg_books.txt
  tools/mine_gutenberg.py --ids 103 1661       # mine specific books
  tools/mine_gutenberg.py --gaps               # only report candidates for minutes with <2 quotes
  tools/mine_gutenberg.py --merge data/candidates.csv
                                               # append rows marked keep=y to data/extra_quotes.csv

Downloads are cached in data/gutenberg/ (git-ignored). Output goes to
data/candidates.csv, pipe-delimited, one candidate per line:

  time|phrase|quote|title|author|ampm|score|keep

Review it in a spreadsheet, put y in the keep column for the ones you want,
then run --merge and rebuild the card with tools/build_quotes.py, which reads
data/extra_quotes.csv alongside the main dataset.
"""
import argparse
import csv
import re
import sys
import time
import urllib.request
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CACHE = ROOT / "data" / "gutenberg"
BOOK_LIST = ROOT / "data" / "gutenberg_books.txt"
CANDIDATES = ROOT / "data" / "candidates.csv"
EXTRA = ROOT / "data" / "extra_quotes.csv"
MAIN = ROOT / "data" / "litclock_annotated.csv"

MIN_LEN, MAX_LEN, IDEAL_LO, IDEAL_HI = 60, 450, 110, 320

# ---------------------------------------------------------------- grammar
HOURS = {
    "one": 1, "two": 2, "three": 3, "four": 4, "five": 5, "six": 6, "seven": 7,
    "eight": 8, "nine": 9, "ten": 10, "eleven": 11, "twelve": 12,
    "noon": 12, "midday": 12, "mid-day": 12, "midnight": 0,
}
MINUTES = {
    "five": 5, "ten": 10, "a quarter": 15, "quarter": 15, "one quarter": 15, "fifteen": 15,
    "twenty": 20, "twenty-five": 25, "twenty five": 25, "half": 30, "thirty": 30,
    "thirty-five": 35, "forty": 40, "forty-five": 45, "fifty": 50, "fifty-five": 55,
    "one": 1, "two": 2, "three": 3, "four": 4, "six": 6, "seven": 7, "eight": 8, "nine": 9,
    "eleven": 11, "twelve": 12, "thirteen": 13, "fourteen": 14, "sixteen": 16, "seventeen": 17,
    "eighteen": 18, "nineteen": 19, "twenty-one": 21, "twenty-two": 22, "twenty-three": 23,
    "twenty-four": 24, "twenty-six": 26, "twenty-seven": 27, "twenty-eight": 28, "twenty-nine": 29,
}
H = r"(?P<hour>one|two|three|four|five|six|seven|eight|nine|ten|eleven|twelve|noon|midday|mid-day|midnight)"
HN = r"(?P<hour>one|two|three|four|five|six|seven|eight|nine|ten|eleven|twelve)"
M = r"(?P<min>" + "|".join(sorted((re.escape(k) for k in MINUTES), key=len, reverse=True)) + r")"
OCLOCK = r"o['\u2019]?\s?clock"
PAST = r"(?P<dir>past|after)"
TO = r"(?P<dir>to|of|till|before)"
LEAD = r"(?:at|about|by|nearly|almost|until|till|before|after|towards|toward|past|exactly|precisely|just|struck|strike|striking|stroke of|was|is|it['\u2019]s|it is|it was|be|been|for|from|since|than)\s+"

PATTERNS = [
    # numeric: 8.15, 8:15, with optional am/pm or o'clock; needs a lead word or am/pm to avoid money/decimals
    ("num", re.compile(r"(?<![\d£$.])(?:" + LEAD + r")?(?P<h>1[0-2]|0?[1-9])[.:](?P<m>[0-5]\d)(?P<ampm>\s?(?:[ap]\.?\s?m\.?)|\s?" + OCLOCK + r")?(?![\d%])", re.I)),
    ("num24", re.compile(r"(?<![\d£$.])(?P<h>1[3-9]|2[0-3])[.:](?P<m>[0-5]\d)(?![\d%])", re.I)),
    # half past eight, a quarter past nine, twenty minutes past four
    ("past", re.compile(r"\b(?:" + M + r")(?:\s+minutes?)?\s+" + PAST + r"\s+" + H + r"\b(?:\s+" + OCLOCK + r")?", re.I)),
    # a quarter to nine, ten minutes to five
    ("to", re.compile(r"\b(?:" + M + r")(?:\s+minutes?)?\s+" + TO + r"\s+" + HN + r"\b(?:\s+" + OCLOCK + r")?", re.I)),
    # eight-thirty, eight fifteen
    ("compound", re.compile(r"\b" + HN + r"[-\s](?P<min>fifteen|thirty|forty-five)\b", re.I)),
    # nine o'clock
    ("oclock", re.compile(r"\b" + HN + r"\s+" + OCLOCK + r"(?:\s+(?:in the|at)\s+(?:morning|afternoon|evening|night))?", re.I)),
    # the clock struck nine, the stroke of midnight, at noon, at midnight
    ("struck", re.compile(r"\b(?:struck|strike|striking|stroke of|chimed|tolled|boomed)\s+" + H + r"\b", re.I)),
    ("noon", re.compile(r"\b(?:at|by|before|after|till|until|towards|toward|about|nearly|almost|past|exactly|precisely)\s+(?P<hour>noon|midday|mid-day|midnight)\b", re.I)),
]

AM_CUES = re.compile(r"\b(morning|dawn|daybreak|sunrise|breakfast|a\.?\s?m\.?|before noon|forenoon|woke|awoke|waking|rose from (?:his|her|my) bed|early)\b", re.I)
PM_CUES = re.compile(r"\b(afternoon|evening|night|dusk|sunset|twilight|dinner|supper|tea|lamps?|candles?|dark|bed(?:time)?|p\.?\s?m\.?|after noon|theatre|theater|moon|stars|to-?morrow|to-?night|retired|retire)\b", re.I)
REJECT_AFTER = re.compile(r"^\s*(?:years?|months?|weeks?|days?|hours?|miles?|pounds?|shillings?|pence|dollars?|francs?|per cent|percent|thousand|hundred|times|men|women|people|of them|or twelve|or eleven|or ten|or nine|or eight|or seven|or six|or five|or four|or three|or two)\b", re.I)
REJECT_BEFORE = re.compile(r"(?:chapter|book|part|volume|act|scene|page|no\.|number|verse|psalm|aged|age of|nearly|about|some|only|other|these|those|first|last|another|the)\s*$", re.I)
UPPER_RUN = re.compile(r"[A-Z]{4,}")

# ---------------------------------------------------------------- download
def fetch(book_id: int) -> str | None:
    CACHE.mkdir(parents=True, exist_ok=True)
    path = CACHE / f"{book_id}.txt"
    if path.exists():
        return path.read_text(encoding="utf-8", errors="replace")
    urls = [f"https://www.gutenberg.org/cache/epub/{book_id}/pg{book_id}.txt",
            f"https://www.gutenberg.org/files/{book_id}/{book_id}-0.txt"]
    for url in urls:
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "literaryclock-miner/1.0"})
            with urllib.request.urlopen(req, timeout=60) as r:
                data = r.read()
            text = data.decode("utf-8", errors="replace")
            path.write_text(text, encoding="utf-8")
            time.sleep(1.0)   # be polite to the mirror
            return text
        except Exception as e:  # noqa: BLE001
            err = e
    print(f"  ! could not download {book_id}: {err}", file=sys.stderr)
    return None


def split_header(text: str):
    title = author = ""
    for line in text[:6000].splitlines():
        if line.startswith("Title:") and not title:
            title = line[6:].strip()
        elif line.startswith("Author:") and not author:
            author = line[7:].strip()
    start = re.search(r"\*\*\* ?START OF (?:THE|THIS) PROJECT GUTENBERG EBOOK.*?\*\*\*", text)
    end = re.search(r"\*\*\* ?END OF (?:THE|THIS) PROJECT GUTENBERG EBOOK", text)
    body = text[start.end() if start else 0: end.start() if end else len(text)]
    title = re.sub(r"\s+", " ", title)
    author = re.sub(r"\s*\(.*?\)\s*", " ", author).strip()
    author = re.sub(r",\s*\d{4}.*$", "", author).strip()   # "Dickens, Charles, 1812-1870" style
    return title, author, body


# ---------------------------------------------------------------- text prep
def paragraphs(body: str):
    body = body.replace("\r\n", "\n")
    body = body.replace("\u2019", "'").replace("\u2018", "'").replace("\u201c", '"').replace("\u201d", '"')
    body = body.replace("\u2014", " - ").replace("--", " - ").replace("_", "")
    for para in re.split(r"\n\s*\n", body):
        p = re.sub(r"\s+", " ", para).strip()
        if len(p) < 40 or UPPER_RUN.search(p[:30]):     # skip headings, tables of contents
            continue
        yield p


SENT_END = re.compile(r'(?:(?<=[.!?])|(?<=[.!?]["\')\]]))\s+(?=["\'(\[]?[A-Z])')


ABBREV = re.compile(r"\b(Mr|Mrs|Ms|Dr|St|Mme|Mlle|M|Capt|Col|Gen|Lieut|Prof|Rev|Hon|Esq|Jr|Sr|vs|etc|No|viz|i\.e|e\.g)\.\s")


def sentences(paragraph: str):
    guarded = ABBREV.sub(lambda m: m.group(0).replace(". ", ".\x00"), paragraph)
    return [s.replace("\x00", " ") for s in SENT_END.split(guarded) if s]


def find_minute(kind, m, context):
    """Return (minute_of_day, phrase_needs_ampm_guess) or None."""
    g = m.groupdict()
    if kind in ("num", "num24"):
        h, mi = int(g["h"]), int(g["m"])
        if kind == "num24":
            return h * 60 + mi, False
        ampm = (g.get("ampm") or "").lower().replace(" ", "").replace(".", "")
        if ampm.startswith("a"):
            return (0 if h == 12 else h) * 60 + mi, False
        if ampm.startswith("p"):
            return (12 if h == 12 else h + 12) * 60 + mi, False
        return h * 60 + mi, True
    hour_word = g["hour"].lower()
    h = HOURS[hour_word]
    fixed = hour_word in ("noon", "midday", "mid-day", "midnight")
    mi = 0
    if kind in ("past", "to", "compound"):
        mi = MINUTES[g["min"].lower()]
        if kind == "to":
            h, mi = h - 1, 60 - mi
            if h == 0:
                h = 12
    if fixed:
        return h * 60 + mi, False
    return h * 60 + mi, True


def guess_ampm(h12, context, phrase_pos):
    """Choose am/pm for a 1-12 hour from cue words near the phrase. Returns (hour24, basis)."""
    window = context[max(0, phrase_pos - 220): phrase_pos + 220]
    am = len(AM_CUES.findall(window))
    pm = len(PM_CUES.findall(window))
    explicit = re.search(r"\b(?:in the|of the)\s+(morning|afternoon|evening|night)\b", context[phrase_pos: phrase_pos + 60], re.I)
    if explicit:
        word = explicit.group(1).lower()
        am, pm = (1, 0) if word == "morning" else (0, 1)
        basis = word
    elif am > pm:
        basis = "am-cue"
    elif pm > am:
        basis = "pm-cue"
    else:
        # No cue: night-time small hours are rare in fiction; afternoon is common.
        basis = "guess"
        am, pm = (1, 0) if 7 <= h12 <= 11 else (0, 1)
    if h12 == 12:
        return (12 if pm >= am else 0), basis
    return (h12 + 12 if pm > am else h12), basis


def score_candidate(quote, phrase, basis):
    s = 0.0
    n = len(quote)
    s += 3 if IDEAL_LO <= n <= IDEAL_HI else 1
    if quote[0] in '"\'(' or quote[0].isupper():
        s += 1
    if quote.rstrip()[-1] in ".!?\"'":
        s += 1
    if basis in ("morning", "afternoon", "evening", "night"):
        s += 2
    elif basis == "guess":
        s -= 1
    if quote.count('"') % 2:
        s -= 2                       # unbalanced dialogue
    if re.search(r"\b(?:said|asked|replied|cried|exclaimed)\b", quote):
        s += 0.5
    if quote.lower().startswith(("and ", "but ", "or ", "which ", "who ", "that ")):
        s -= 2
    return round(s, 1)


def mine_book(book_id, seen):
    text = fetch(book_id)
    if not text:
        return []
    title, author, body = split_header(text)
    out = []
    for para in paragraphs(body):
        sents = sentences(para)
        for i, sent in enumerate(sents):
            for kind, rx in PATTERNS:
                for m in rx.finditer(sent):
                    phrase = m.group(0)
                    # Trim the lead word for the highlighted phrase but keep the time words
                    core = phrase if kind == "struck" else re.sub(r"^(?:" + LEAD + r")", "", phrase, flags=re.I)
                    core = core.strip(" .,;:")
                    after = sent[m.end(): m.end() + 24]
                    before = sent[max(0, m.start() - 24): m.start()]
                    if REJECT_AFTER.match(after) or REJECT_BEFORE.search(before):
                        continue
                    if kind == "oclock" and re.search(r"\b(?:or|and)\s+" + HN + r"\s*$", before, re.I):
                        continue
                    if kind in ("to", "past") and re.search(r"\b(?:or|and|from|between|to)\s*$", before, re.I):
                        continue      # "from eight or ten to twelve inches" is a range, not a time
                    res = find_minute(kind, m, sent)
                    if res is None:
                        continue
                    minute, needs = res
                    basis = "explicit"
                    if needs:
                        h12 = minute // 60
                        h24, basis = guess_ampm(h12, para, para.find(sent) + m.start())
                        minute = h24 * 60 + minute % 60
                    # Build the quote: this sentence, padded with neighbours if short
                    quote = sent
                    j, k = i, i
                    while len(quote) < MIN_LEN and (j > 0 or k < len(sents) - 1):
                        if j > 0:
                            j -= 1; quote = sents[j] + " " + quote
                        elif k < len(sents) - 1:
                            k += 1; quote = quote + " " + sents[k]
                    if len(quote) < MIN_LEN or len(quote) > MAX_LEN:
                        continue
                    if UPPER_RUN.search(quote):
                        continue
                    key = quote.lower()[:80]
                    if key in seen:
                        continue
                    seen.add(key)
                    out.append({
                        "time": f"{minute // 60:02d}:{minute % 60:02d}",
                        "phrase": core, "quote": quote, "title": title, "author": author,
                        "ampm": basis, "score": score_candidate(quote, core, basis), "keep": "",
                    })
                    break     # one pattern per sentence is enough
    return out


def existing_counts():
    counts = defaultdict(int)
    for path in (MAIN, EXTRA):
        if not path.exists():
            continue
        with path.open(encoding="utf-8", newline="") as f:
            for row in csv.reader(f, delimiter="|", quoting=csv.QUOTE_NONE):
                if row and re.fullmatch(r"\d\d:\d\d", row[0]):
                    counts[row[0]] += 1
    return counts


def read_ids():
    ids = []
    for line in BOOK_LIST.read_text().splitlines():
        line = line.split("#", 1)[0]
        ids += [int(x) for x in line.split()]
    return ids


def merge(reviewed: Path):
    kept = []
    with reviewed.open(encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f, delimiter="|", quoting=csv.QUOTE_NONE):
            if (row.get("keep") or "").strip().lower() in ("y", "yes", "1", "true"):
                kept.append(row)
    with EXTRA.open("a", encoding="utf-8", newline="") as f:
        for r in kept:
            f.write("|".join([r["time"], r["phrase"], r["quote"].replace("|", "/"),
                              r["title"].replace("|", "/"), r["author"].replace("|", "/"), "unknown"]) + "\n")
    print(f"appended {len(kept)} quotes to {EXTRA}; now run tools/build_quotes.py")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ids", nargs="*", type=int, help="Gutenberg ids (default: data/gutenberg_books.txt)")
    ap.add_argument("--gaps", action="store_true", help="only keep candidates for minutes with fewer than 2 quotes")
    ap.add_argument("--min-score", type=float, default=2.0)
    ap.add_argument("--merge", type=Path, help="append keep=y rows from this reviewed file to data/extra_quotes.csv")
    ap.add_argument("--out", type=Path, default=CANDIDATES)
    args = ap.parse_args()
    if args.merge:
        merge(args.merge); return

    ids = args.ids or read_ids()
    counts = existing_counts()
    seen, rows = set(), []
    for n, book_id in enumerate(ids, 1):
        found = mine_book(book_id, seen)
        title = found[0]["title"] if found else "?"
        print(f"[{n}/{len(ids)}] {book_id}: {len(found):4d} candidates  {title[:60]}")
        rows += found

    rows = [r for r in rows if r["score"] >= args.min_score]
    if args.gaps:
        rows = [r for r in rows if counts[r["time"]] < 2]
    rows.sort(key=lambda r: (r["time"], -r["score"]))
    fields = ["time", "phrase", "quote", "title", "author", "ampm", "score", "keep"]
    with args.out.open("w", encoding="utf-8", newline="") as f:
        f.write("|".join(fields) + "\n")
        for r in rows:
            f.write("|".join(str(r[k]).replace("|", "/") for k in fields) + "\n")
    minutes = {r["time"] for r in rows}
    new_minutes = {m for m in minutes if counts[m] == 0}
    print(f"\n{len(rows)} candidates over {len(minutes)} minutes -> {args.out}")
    print(f"{len(new_minutes)} of those minutes currently have no quote at all: {' '.join(sorted(new_minutes))}")
    weak = sorted(m for m in minutes if counts[m] == 1)
    print(f"{len(weak)} minutes currently have a single quote and would gain a second")


if __name__ == "__main__":
    main()
