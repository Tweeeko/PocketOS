#!/bin/sh
# PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
# Builds the website into _site/ (used by Vercel: see vercel.json).
# Copies site/ and CHANGELOG.md, then fetches the latest release's pocketos-full.bin for the
# browser installer. Browsers can't load firmware straight from GitHub's release downloads, so the
# installer needs its own copy on the same site.
set -eu
REPO="${POCKETOS_REPO:-Tweeeko/PocketOS}"
OUT=_site
rm -rf "$OUT"
mkdir -p "$OUT/firmware"
cp -R site/. "$OUT/"
rm -f "$OUT/build.sh"
cp CHANGELOG.md "$OUT/"

BASE="https://github.com/$REPO/releases/latest/download"
if curl -fsSL "$BASE/manifest.json" -o "$OUT/firmware/release.json"; then
  curl -fsSL "$BASE/pocketos-full.bin" -o "$OUT/firmware/pocketos-full.bin"
  node -e '
    const fs = require("fs");
    const r = JSON.parse(fs.readFileSync(process.argv[1] + "/firmware/release.json", "utf8"));
    fs.writeFileSync(process.argv[1] + "/firmware/manifest.json", JSON.stringify({
      name: "PocketOS", version: r.version,
      new_install_prompt_erase: true, new_install_improv_wait_time: 0,
      builds: [{ chipFamily: "ESP32-S3", parts: [{ path: "pocketos-full.bin", offset: 0 }] }]
    }, null, 2));
    fs.unlinkSync(process.argv[1] + "/firmware/release.json");
    console.log("Installer: PocketOS " + r.version);
  ' "$OUT"
else
  echo "No release published yet: the install page will say the installer isn't available."
  rm -f "$OUT/firmware/release.json"
fi
ls -la "$OUT" "$OUT/firmware"
