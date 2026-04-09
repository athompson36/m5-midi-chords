#!/usr/bin/env python3
"""
Parse docs/TODO_IMPLEMENTATION.md checkbox lines (one GitHub issue per bullet).
Generates scripts/github-issues.json and docs/GITHUB_ISSUES.md

Usage:
  python3 scripts/gen_github_issues.py
  python3 scripts/gen_github_issues.py --print   # summary only
  python3 scripts/create_github_issues.py      # requires gh CLI + auth

See docs/GITHUB_ISSUES.md for manual creation if gh is unavailable.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TODO_PATH = ROOT / "docs" / "TODO_IMPLEMENTATION.md"
OUT_JSON = ROOT / "scripts" / "github-issues.json"


def slugify(text: str, max_len: int = 72) -> str:
    s = re.sub(r"[^a-zA-Z0-9]+", "-", text.lower()).strip("-")
    return s[:max_len].rstrip("-")


def parse_todo(path: Path) -> list[dict]:
    text = path.read_text(encoding="utf-8")
    phase = "?"
    phase_title = ""
    issues: list[dict] = []
    phase_header = re.compile(r"^##\s+Phase\s+(\d+)\s+—\s+(.+)$")

    for line in text.splitlines():
        m = phase_header.match(line)
        if m:
            phase = m.group(1)
            phase_title = m.group(2).strip()
            continue
        if line.startswith("- [ ]"):
            raw = line[5:].strip()
            title = raw
            # Short GitHub title: [P{n}] first ~80 chars of bullet
            short = raw.replace("`", "").strip()
            if len(short) > 85:
                short = short[:82] + "…"
            gh_title = f"[P{phase}] {short}"
            body = f"""## Phase {phase} — {phase_title}

From `docs/TODO_IMPLEMENTATION.md`:

{raw}

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase {phase})
- `docs/DEV_ROADMAP.md`
"""
            if "MIDI_INPUT_SPEC" in raw:
                body += "- `docs/MIDI_INPUT_SPEC.md`\n"
            if (
                "SEQUENCER" in raw
                or "Shift" in raw
                or "arpeggiator" in raw.lower()
                or "X–Y" in raw
                or "X-Y MIDI" in raw
            ):
                body += "- `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`\n"
            issues.append(
                {
                    "phase": int(phase),
                    "phase_title": phase_title,
                    "title": gh_title,
                    "body": body,
                    "slug": slugify(raw[:60]),
                }
            )
    return issues


def write_json(issues: list[dict]) -> None:
    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    # Drop slug from json if desired — keep for URL anchors
    serializable = [{k: v for k, v in i.items() if k != "slug"} for i in issues]
    OUT_JSON.write_text(json.dumps(serializable, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_human_index(issues: list[dict]) -> None:
    p = ROOT / "docs" / "GITHUB_ISSUES.md"
    lines = [
        "# GitHub issues (one per TODO checkbox cluster)",
        "",
        "Generated from `docs/TODO_IMPLEMENTATION.md` via:",
        "",
        "```bash",
        "python3 scripts/gen_github_issues.py",
        "```",
        "",
        "Create on GitHub:",
        "",
        "1. **CLI (recommended):** ensure [`gh`](https://cli.github.com/) is installed and run:",
        "",
        "   ```bash",
        "   python3 scripts/create_github_issues.py",
        "   ```",
        "",
        "   Dry run: `python3 scripts/create_github_issues.py --dry-run`",
        "",
        "   Other repo: `GITHUB_ISSUES_REPO=owner/name python3 scripts/create_github_issues.py`",
        "",
        "2. **Manual:** use `scripts/github-issues.json` (title + body per entry) or the numbered list below.",
        "",
        f"**Total issues:** {len(issues)}",
        "",
        "---",
        "",
    ]
    for i, it in enumerate(issues, start=1):
        lines.append(f"## {i}. {it['title']}")
        lines.append("")
        lines.append("```")
        lines.append(it["body"].rstrip())
        lines.append("```")
        lines.append("")
    p.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--print", action="store_true", help="print issue titles and exit")
    args = ap.parse_args()

    if not TODO_PATH.is_file():
        print(f"Missing {TODO_PATH}", file=sys.stderr)
        return 1

    issues = parse_todo(TODO_PATH)
    if not issues:
        print("No - [ ] lines found.", file=sys.stderr)
        return 1

    write_json(issues)
    write_human_index(issues)

    print(f"Wrote {OUT_JSON} ({len(issues)} issues)")
    print(f"Wrote docs/GITHUB_ISSUES.md")

    if args.print:
        for i, it in enumerate(issues, start=1):
            print(f"{i:3}. {it['title']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
