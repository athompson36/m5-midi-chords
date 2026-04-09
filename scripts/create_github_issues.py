#!/usr/bin/env python3
"""Create GitHub issues from scripts/github-issues.json (run gen_github_issues.py first)."""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
JSON_PATH = ROOT / "scripts" / "github-issues.json"


def main() -> int:
    ap = argparse.ArgumentParser(description="Create GitHub issues via gh CLI")
    ap.add_argument("--dry-run", action="store_true", help="Print titles only")
    ap.add_argument(
        "--repo",
        default=os.environ.get("GITHUB_ISSUES_REPO", ""),
        help="owner/repo (default: current repo or GITHUB_ISSUES_REPO)",
    )
    args = ap.parse_args()

    if not JSON_PATH.is_file():
        print(f"Missing {JSON_PATH} — run: python3 scripts/gen_github_issues.py", file=sys.stderr)
        return 1

    issues = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    gh = subprocess.run(["which", "gh"], capture_output=True, text=True)
    if gh.returncode != 0:
        print("Install GitHub CLI: https://cli.github.com/  then: gh auth login", file=sys.stderr)
        return 1

    for i, it in enumerate(issues, start=1):
        title = it["title"]
        body = it["body"]
        print(f"{i}/{len(issues)} {title[:70]}...")
        if args.dry_run:
            continue
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".md", delete=False, encoding="utf-8"
        ) as f:
            f.write(body)
            path = f.name
        try:
            cmd = ["gh", "issue", "create"]
            if args.repo:
                cmd += ["--repo", args.repo]
            cmd += ["--title", title, "--body-file", path]
            r = subprocess.run(cmd, cwd=str(ROOT))
            if r.returncode != 0:
                return r.returncode
        finally:
            try:
                os.unlink(path)
            except OSError:
                pass

    print(f"Done ({len(issues)} issues).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
