# PocketOS brand

Created by Leeko. Copyright (c) 2026 Leeko. All rights reserved.

The logo is the companion's two eyes and the PocketOS wordmark. The companion on the device and
website is its own artwork (firmware/main/ui/ui_home.c); this mark doesn't replace it.

## Files

| File | Use |
|---|---|
| `pocketos-logo-*.svg` | Stacked logo: eyes above the wordmark. Box lid, title cards |
| `pocketos-logo-horizontal-*.svg` | Eyes beside the wordmark. Website header, banners |
| `pocketos-wordmark-*.svg` | The word only |
| `pocketos-mark-*.svg` | The eyes only. Small spaces, stickers, watermark |
| `pocketos-app-icon.svg`, `-512.png`, `-180.png`, `favicon-32.png` | App, Home Screen and browser icons |
| `pocketos-app-icon-square.svg`, `-square-512.png` | The same icon without rounded corners, for platforms that apply their own mask (Apple touch icon, maskable web app icon) |

Each comes in `gradient` (white to `#56B9FF` to `#9860F5`, for dark backgrounds), `white` and
`black`. Every file is cropped tight to the artwork.

## How it was made

Rebuilt from the traced "Corrected Vector Pack": the eyes are exact pill shapes, and every letter
is drawn from straight lines and circular arcs measured from that artwork (cap height 120, x-height
91, upright strokes 19, horizontals 16 to 18). So edges stay clean at any size, including foil.
The tagline from the pack isn't included: its traced letters were too rough to reuse.

## Print

- One-colour foil (e.g. silver on a matte black box): use a `black` file as the foil plate and ask
  the printer to assign it to a spot colour such as `FOIL_SILVER`; it should not print as black ink.
- Keep clear space around the logo of at least the height of one eye.
- The smallest sensible size for the stacked logo is about 25 mm wide; for foil, check the
  printer's minimum line width (the thinnest strokes are about 1.5 % of the wordmark width).
- Place the artwork on the manufacturer's dieline, and confirm bleed, safe area and foil
  tolerances with them.
