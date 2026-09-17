#!/bin/sh
# Read-only documentation layout check. Verifies, for every Markdown file in the repository
# (excluding vendor/build trees):
#   - lowercase snake_case filename, with README.md and ADR NNNN_slug.md exceptions;
#   - exactly one H1 title;
#   - a Status: line before the first H2 section (READMEs may use "- **Status:**");
#   - relative Markdown links resolve.
# It does not judge prose quality or status truth. See docs/development/documentation_style.md.
#
# Usage: vqec_vision_check_docs_layout.sh [repo_root]
set -eu

root="${1:-.}"
cd "$root"

python3 - <<'PY'
import os, re, sys

SKIP_DIRS = (".git", "third_party")
# Repository meta files that are intentionally not template documents.
EXEMPT_FILES = {os.path.normpath("./AGENTS.md"), os.path.normpath("./README.md")}
violations = []
checked = 0

def is_skipped(path):
    parts = path.split(os.sep)
    return any(p in SKIP_DIRS or p.startswith("build-") for p in parts)

def strip_fences(text):
    out = []
    in_fence = False
    for line in text.splitlines():
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            continue
        if not in_fence:
            out.append(line)
    return out

for dirpath, dirnames, filenames in os.walk("."):
    dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS and not d.startswith("build-")]
    for name in filenames:
        if not name.endswith(".md"):
            continue
        path = os.path.join(dirpath, name)
        if is_skipped(path):
            continue
        rel = os.path.relpath(path)
        if os.path.normpath(path) in EXEMPT_FILES:
            continue
        checked += 1

        # filename
        if name == "README.md":
            pass
        elif re.fullmatch(r"[0-9]{4}_[a-z0-9_]+\.md", name):
            pass
        elif not re.fullmatch(r"[a-z0-9_]+\.md", name):
            violations.append(f"{rel}: filename must be lowercase snake_case (README.md or NNNN_slug.md)")

        text = open(path, encoding="utf-8", errors="ignore").read()
        lines = strip_fences(text)

        # single H1
        h1 = [i for i, l in enumerate(lines) if re.match(r"^# ", l)]
        if len(h1) != 1:
            violations.append(f"{rel}: expected exactly one '# ' title, found {len(h1)}")

        # status before first H2
        first_h2 = next((i for i, l in enumerate(lines) if re.match(r"^## ", l)), len(lines))
        head = "\n".join(lines[:first_h2])
        if not re.search(r"Status:", head):
            violations.append(f"{rel}: missing a 'Status:' line before the first '##' section")

        # relative links (scan raw text so code examples are still validated)
        for m in re.finditer(r"\]\(([^)]+)\)", text):
            link = m.group(1).strip()
            if link.startswith(("http://", "https://", "#", "mailto:", "<")):
                continue
            link = link.split("#", 1)[0]
            if not link:
                continue
            if "gst-plugins-qti-oss" in link or "gst-sample-apps" in link:
                continue  # external vendor-source citations
            target = os.path.normpath(os.path.join(dirpath, link))
            if not os.path.exists(target):
                violations.append(f"{rel}: broken link -> {link}")

print(f"checked {checked} markdown files")
if violations:
    for v in violations:
        print("FAIL:", v)
    print(f"{len(violations)} documentation layout violation(s)")
    sys.exit(1)
print("PASS: documentation filenames, titles, status lines and links")
PY
