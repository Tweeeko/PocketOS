# Putting out an update

Updates are published as **GitHub releases** of this repository. Every PocketOS checks once a
day, and installs the update when its owner taps **Install** on the settings website, or by
itself overnight while charging if they chose "Install overnight". There's no PocketOS server:
GitHub hosts the files.

## One-time setup

1. **The repository must be public.** PocketOS downloads release files without logging in to
   GitHub, and GitHub only allows that for public repositories. The code contains no
   passwords or keys (Wi-Fi and Home Assistant details are typed in on each device), so making
   it public is safe. Repository **Settings → General → Danger zone → Change visibility**.
   If you'd rather keep the code private, create a second, public repository just for releases
   and change `POCKETOS_UPDATE_URL` (in `firmware/main/Kconfig.projbuild`) to point at it.
2. The Release workflow needs permission to create releases. It asks for it itself
   (`permissions: contents: write`). If your account restricts that, enable it under
   **Settings → Actions → General → Workflow permissions → Read and write**.

## Every release (all from the GitHub website on an iPad)

1. **Bump the version.** Edit `firmware/version.txt`, e.g. `0.1.0` → `0.2.0`. Use a bigger
   number every time: PocketOS only offers versions newer than the one it's running.
2. **Say what's new.** Add a section to the top of `CHANGELOG.md`:
   ```
   ## 0.2.0
   Smoother menus and longer battery life.
   ```
   Keep it to a sentence or two. It's shown on the settings website next to the Install button.
3. Commit both changes to `main`.
4. Open **Actions → Release → Run workflow**. It:
   - runs the tests;
   - builds the firmware;
   - checks the version inside the build matches `version.txt`;
   - publishes release `v0.2.0` with `pocketos.bin` (the update), `pocketos-full.bin` (first
     USB flash of a new board) and `manifest.json` (version, size and SHA-256 of `pocketos.bin`).
5. The **Website** workflow then runs by itself and updates the website's installer and
   "What's new" page.
6. Done. Each PocketOS notices within a day. To see it straight away, open its settings website,
   **More → Updates → Check now**.

If something's wrong with a release, **don't delete and reuse the version number**. Publish a
fixed one with the next number.

## What PocketOS does with it

1. Downloads `manifest.json` from `…/releases/latest/download/manifest.json` over HTTPS
   (certificate checked). Only a newer version is offered.
2. On Install (needs 30 % battery or USB power), downloads `pocketos.bin` into the spare
   firmware slot, checking the size and the SHA-256 against the manifest as it goes.
3. ESP-IDF checks the image itself, PocketOS checks it says it's "pocketos", and only then is it
   marked to start next time.
4. After restarting, the new version marks itself good once Wi-Fi, the screen and the web
   server are up. If it crashes before that, the bootloader goes back to the previous version
   by itself.

Nothing about the home, its devices or its owner is sent. Each check is a plain download, so
GitHub sees the home's internet address, the same as when anyone opens a web page. Owners
who don't want that can choose **Don't check** (More → Updates); they can still install a
`pocketos.bin` file by hand.

## Testing a build before releasing it

Every push runs **Build firmware**, which attaches `pocketos.bin` to the run (Actions → the run
→ Artifacts). Install it on your own unit from the settings website: **More → Install from a
file**.

## Limits worth knowing

- The download is protected by HTTPS and the checksum in the manifest, which both come from
  GitHub. Anyone who can publish releases on this repository can publish firmware, so keep the
  GitHub account secure (two-factor login on). Cryptographically signed firmware (ESP-IDF
  "secure boot") is possible later. It needs a signing key kept in GitHub secrets and burns
  settings into the chip permanently, so it isn't switched on.
- Changing `firmware/partitions.csv` can't be delivered as an update. It needs a USB re-flash.
