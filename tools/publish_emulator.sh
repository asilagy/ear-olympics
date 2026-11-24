#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Publish the site with ONLY the browser emulator open: like fm1-base's tools/publish_site.sh, but the firmware
# download is left out and the install page is replaced by a note that installing is not open yet. Use this until the
# firmware is ready for other people's FM-1s; then use base/tools/publish_site.sh.
#   tools/publish_emulator.sh VERSION      (Docker running; PYTHON defaults to base/.venv/bin/python)
set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
BASE="$ROOT/base"
APP="$ROOT/app"
V="${1:?usage: tools/publish_emulator.sh VERSION}"
PY="${PYTHON:-$BASE/.venv/bin/python}"
NAME="$(sed -n 's/^name *= *"\([^"]*\)".*/\1/p' "$APP/fm1.toml" | tr 'A-Z' 'a-z')"
"$PY" "$BASE/tools/build.py" "$APP" --release "$V"
"$PY" "$BASE/tools/build.py" "$APP" --web --release "$V"
"$PY" "$BASE/tools/make_site.py" "$APP" "$APP/build/$NAME-$V.fwsc" "$V" "$APP/build/site"
SITE="$APP/build/site"
rm -rf "$SITE/firmware"               # no download
cat > "$SITE/install/index.html" <<'HTML'
<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Install: not open yet</title>
<style>body{font:17px/1.6 system-ui,sans-serif;max-width:40rem;margin:3rem auto;padding:0 1rem;color:#1e2b5a;background:#fbf8f1}a{color:#c23a22}</style>
</head><body>
<h1>Installing isn't open yet</h1>
<p>Ear Olympics is still being tested on real FM-1s, so the installer is closed for now. You can play it in your
browser instead.</p>
<p><a href="../emu/">Try it in the browser</a> &middot; <a href="../">Back to the front page</a></p>
</body></html>
HTML
REMOTE="$(git -C "$ROOT" remote get-url origin)"
SHA="$(git -C "$ROOT" rev-parse --short HEAD)"
cd "$SITE"
rm -rf .git
git init -q -b gh-pages
git add -A
git commit -q -m "Emulator-only site for $NAME $V (from $SHA)"
git push -q -f "$REMOTE" gh-pages
rm -rf .git
REPO="$(printf %s "$REMOTE" | sed -E 's#(git@github.com:|https://github.com/)##; s#\.git$##')"
gh api -X POST "repos/$REPO/pages/builds" >/dev/null 2>&1 || echo "(could not ask GitHub Pages to build: check the repository's Pages settings)"
echo "pushed the emulator-only site for $NAME $V to gh-pages"
