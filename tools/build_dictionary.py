#!/usr/bin/env python3
"""
Build EnViDict's dictionary.db from one of several input formats:

  --json PATH       macOS Dictionary extracted JSON (array of objects with
                    headword, pronunciations, parts_of_speech, definitions,
                    examples fields)
  --stardict PATH   StarDict directory (.ifo + .idx + .dict/.dict.dz)
  (neither)         falls back to tools/seed_entries.tsv for smoke tests

Output schema:
    entries(word TEXT PRIMARY KEY COLLATE NOCASE, definition_ir BLOB)
    entries_fts (FTS5 virtual table over `word`)

The in-house IR format is:
    RUN := 0x01 STYLE_BYTE TEXT
where STYLE_BYTE bits map to ir::IrStyleBits in the C++ code.
"""

from __future__ import annotations

import argparse
import gzip
import json
import os
import re
import sqlite3
import struct
import sys
from pathlib import Path

STYLE_BOLD       = 1 << 0
STYLE_ITALIC     = 1 << 1
STYLE_POSTAG     = 1 << 2
STYLE_EXAMPLE    = 1 << 3
STYLE_VITRANS    = 1 << 4
STYLE_NEWLINE    = 1 << 5


def encode_runs(runs: list[tuple[int, str]]) -> bytes:
    out = bytearray()
    for style, text in runs:
        if not text:
            continue
        out.append(0x01)
        out.append(style & 0xFF)
        out.extend(text.encode("utf-8"))
    return bytes(out)


def plain_to_runs(text: str) -> list[tuple[int, str]]:
    text = text.replace("\r\n", "\n").strip()
    if not text:
        return []
    return [(0, text)]


_HTML_TAG_RE = re.compile(r"<[^>]+>")


def html_to_runs(text: str) -> list[tuple[int, str]]:
    """Very small HTML-to-IR mapper. Good enough for the typical StarDict
    EN-VI packs; we deliberately do not pull in a full HTML parser because it
    adds megabytes of build-time dependencies and this script runs offline."""
    runs: list[tuple[int, str]] = []
    style = 0
    i = 0
    buf: list[str] = []

    def flush() -> None:
        nonlocal buf
        if buf:
            s = "".join(buf).replace("&nbsp;", " ").replace("&amp;", "&")
            s = s.replace("&lt;", "<").replace("&gt;", ">")
            s = s.replace("&quot;", '"').replace("&#39;", "'")
            if s:
                runs.append((style, s))
            buf = []

    while i < len(text):
        c = text[i]
        if c == "<":
            end = text.find(">", i)
            if end == -1:
                buf.append(c)
                i += 1
                continue
            tag = text[i + 1 : end].lower().strip()
            flush()
            if tag in ("b", "strong"):
                style |= STYLE_BOLD
            elif tag in ("/b", "/strong"):
                style &= ~STYLE_BOLD
            elif tag in ("i", "em"):
                style |= STYLE_ITALIC
            elif tag in ("/i", "/em"):
                style &= ~STYLE_ITALIC
            elif tag.startswith("br"):
                runs.append((STYLE_NEWLINE, ""))
            elif tag in ("p", "/p", "div", "/div"):
                runs.append((STYLE_NEWLINE, ""))
            # other tags are stripped
            i = end + 1
        else:
            buf.append(c)
            i += 1
    flush()
    return runs


# -----------------------------------------------------------------------------
# macOS Dictionary JSON reader

def convert_json(json_path: Path) -> list[tuple[str, bytes]]:
    """Convert the JSON exported from macOS Dictionary.app.

    Each entry is an object with keys:
        id, headword, pronunciations[], parts_of_speech[], definitions[], examples[]
    """
    with json_path.open("r", encoding="utf-8") as f:
        data = json.load(f)

    out: dict[str, bytes] = {}
    for entry in data:
        headword = entry.get("headword", "").strip()
        if not headword:
            continue
        key = headword.lower()

        runs: list[tuple[int, str]] = []

        # Pronunciation (IPA)
        prons = entry.get("pronunciations", [])
        if prons:
            pron_text = " /" + prons[0] + "/"
            runs.append((STYLE_ITALIC, pron_text))

        # Parts of speech + definitions
        pos_list = entry.get("parts_of_speech", [])
        defs = entry.get("definitions", [])

        for i, d in enumerate(defs):
            d = d.strip()
            if not d:
                continue
            # Add newline before each definition (except the first if no pron)
            if runs:
                runs.append((STYLE_NEWLINE, ""))
            # Show POS tag if there is one for this definition index
            if i < len(pos_list) and pos_list[i]:
                runs.append((STYLE_POSTAG | STYLE_ITALIC, pos_list[i] + " "))
            elif i == 0 and pos_list:
                # Only one POS for multiple defs — show it on the first
                runs.append((STYLE_POSTAG | STYLE_ITALIC, pos_list[0] + " "))
            runs.append((STYLE_VITRANS, d))

        # Examples
        examples = entry.get("examples", [])
        for ex in examples:
            ex = ex.strip()
            if not ex:
                continue
            runs.append((STYLE_NEWLINE, ""))
            runs.append((STYLE_EXAMPLE | STYLE_ITALIC, ex))

        if not runs:
            continue

        ir = encode_runs(runs)
        if key in out:
            # Merge duplicate headwords (e.g. EN-VI and VI-EN entries)
            out[key] = out[key] + b"\x01\x20\n" + ir
        else:
            out[key] = ir

    return sorted(out.items())


# -----------------------------------------------------------------------------
# StarDict reader

def read_ifo(path: Path) -> dict[str, str]:
    info: dict[str, str] = {}
    with path.open("r", encoding="utf-8") as f:
        header = f.readline().strip()
        if not header.startswith("StarDict"):
            raise RuntimeError(f"{path} is not a StarDict .ifo file")
        for line in f:
            line = line.strip()
            if not line or "=" not in line:
                continue
            k, v = line.split("=", 1)
            info[k.strip()] = v.strip()
    return info


def iter_idx(idx_bytes: bytes):
    """Yield (word, offset, length) triples from a StarDict .idx file."""
    i = 0
    while i < len(idx_bytes):
        nul = idx_bytes.find(b"\x00", i)
        if nul == -1:
            break
        word = idx_bytes[i:nul].decode("utf-8", "replace")
        i = nul + 1
        if i + 8 > len(idx_bytes):
            break
        offset, length = struct.unpack(">II", idx_bytes[i : i + 8])
        i += 8
        yield word, offset, length


def load_dict_blob(dict_path: Path) -> bytes:
    if dict_path.suffix == ".dz":
        with gzip.open(dict_path, "rb") as f:
            return f.read()
    return dict_path.read_bytes()


def convert_stardict(stardict_dir: Path) -> list[tuple[str, bytes]]:
    ifo_files = list(stardict_dir.glob("*.ifo"))
    if not ifo_files:
        raise RuntimeError(f"No .ifo file under {stardict_dir}")
    ifo = ifo_files[0]
    base = ifo.with_suffix("")
    info = read_ifo(ifo)
    seq = info.get("sametypesequence", "m")

    idx_path = base.with_suffix(".idx")
    if not idx_path.exists():
        idx_path = base.with_suffix(".idx.gz")
        if idx_path.exists():
            idx_bytes = gzip.decompress(idx_path.read_bytes())
        else:
            raise RuntimeError(f"Cannot find .idx next to {ifo}")
    else:
        idx_bytes = idx_path.read_bytes()

    dict_path = base.with_suffix(".dict.dz")
    if not dict_path.exists():
        dict_path = base.with_suffix(".dict")
    blob = load_dict_blob(dict_path)

    out: dict[str, bytes] = {}
    for word, offset, length in iter_idx(idx_bytes):
        try:
            raw = blob[offset : offset + length].decode("utf-8", "replace")
        except Exception:
            continue
        if seq == "h":
            runs = html_to_runs(raw)
        else:
            runs = plain_to_runs(raw)
        if not runs:
            continue
        key = word.strip().lower()
        if not key:
            continue
        ir = encode_runs(runs)
        if key in out:
            out[key] = out[key] + b"\x01\x20\n" + ir  # merge with newline
        else:
            out[key] = ir
    return sorted(out.items())


# -----------------------------------------------------------------------------
# Seed (fallback)

DEFAULT_SEED = Path(__file__).parent / "seed_entries.tsv"


def load_seed(path: Path) -> list[tuple[str, bytes]]:
    """seed_entries.tsv format:
       <word>\t<vi-definition>     (optional '|' separates multiple senses)
    """
    entries: list[tuple[str, bytes]] = []
    if not path.exists():
        return entries
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t", 1)
            if len(parts) != 2:
                continue
            word, definition = parts
            senses = [s.strip() for s in definition.split("|") if s.strip()]
            runs: list[tuple[int, str]] = []
            for i, sense in enumerate(senses):
                if i > 0:
                    runs.append((STYLE_NEWLINE, ""))
                runs.append((STYLE_VITRANS, sense))
            entries.append((word.strip().lower(), encode_runs(runs)))
    return entries


# -----------------------------------------------------------------------------
# SQLite writer

def write_sqlite(output: Path, entries: list[tuple[str, bytes]]) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()
    con = sqlite3.connect(str(output))
    try:
        cur = con.cursor()
        cur.execute("PRAGMA page_size = 4096;")
        cur.execute("PRAGMA journal_mode = OFF;")
        cur.execute(
            "CREATE TABLE entries(word TEXT PRIMARY KEY COLLATE NOCASE, "
            "definition_ir BLOB) WITHOUT ROWID;"
        )
        cur.execute(
            "CREATE VIRTUAL TABLE entries_fts USING fts5("
            "word, tokenize='unicode61 remove_diacritics 2');"
        )
        cur.executemany(
            "INSERT OR REPLACE INTO entries(word, definition_ir) VALUES (?, ?);",
            entries,
        )
        cur.executemany(
            "INSERT INTO entries_fts(word) VALUES (?);",
            [(w,) for w, _ in entries],
        )
        con.commit()
        cur.execute("VACUUM;")
    finally:
        con.close()


DEFAULT_JSON = Path(__file__).resolve().parent.parent / "data" / "viet_en_dictionary.json"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, help="Path to dictionary.db")
    parser.add_argument("--json", help="Path to macOS Dictionary JSON export")
    parser.add_argument("--stardict", help="Path to StarDict directory (optional)")
    parser.add_argument("--seed", default=str(DEFAULT_SEED),
                        help="Path to seed TSV used when no other source is provided")
    args = parser.parse_args()

    out = Path(args.output)

    entries: list[tuple[str, bytes]]
    if args.json:
        jp = Path(args.json)
        if not jp.is_file():
            print(f"error: --json path not found: {jp}", file=sys.stderr)
            return 1
        print(f"Converting macOS Dictionary JSON from {jp}")
        entries = convert_json(jp)
    elif args.stardict:
        sd = Path(args.stardict)
        if not sd.is_dir():
            print(f"error: --stardict path not found: {sd}", file=sys.stderr)
            return 1
        print(f"Converting StarDict pack from {sd}")
        entries = convert_stardict(sd)
    elif DEFAULT_JSON.is_file():
        # Auto-detect the JSON in data/ folder
        print(f"Auto-detected macOS Dictionary JSON: {DEFAULT_JSON}")
        entries = convert_json(DEFAULT_JSON)
    else:
        seed = Path(args.seed)
        print(f"No --json / --stardict given, using seed file: {seed}")
        entries = load_seed(seed)
        if not entries:
            # Generate a minimal inline seed so the build still produces a DB.
            entries = [
                ("hello", encode_runs([(STYLE_VITRANS, "xin chào")])),
                ("world", encode_runs([(STYLE_VITRANS, "thế giới")])),
                ("dictionary", encode_runs([(STYLE_VITRANS, "từ điển")])),
            ]

    print(f"Writing {len(entries)} entries to {out}")
    write_sqlite(out, entries)
    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
