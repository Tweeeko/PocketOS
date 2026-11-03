# PocketOS security design

Created by Leeko. Copyright (c) 2026 Leeko.

## In place

| Area | What PocketOS does |
|---|---|
| Default passwords | None. Each device makes an 8-character setup Wi-Fi password on first start (`storage_ap_password`). The factory tool erases each device fully, so nothing is shared. |
| Pairing | Settings and every change need a token, issued only for the 6-digit code shown on the device. 5 wrong codes lock pairing for a minute. Codes are single use. Tokens are compared in constant time. |
| Before pairing | `GET /api/v2/info` shows only name, version and address. Network, battery and home details need a token (or setup mode, on PocketOS's own network). |
| Home Assistant token | Write-only: stored in NVS, never returned by the API or shown on the page. Use a dedicated non-admin user (Home Assistant can't limit a user to particular devices, so it can still control everything). |
| Firmware updates | `pocketos.bin` carries an ECDSA P-256 signature over the image (80-byte trailer, `core/update.h`). Online updates and file uploads are refused unless the signature matches `esp/signing_key.pem` (`esp/sigcheck.c`). Online updates are also checked against the manifest's SHA-256, downloaded over HTTPS with certificate checks, validated by ESP-IDF and rolled back automatically if the new version doesn't start. |
| Release key | Private key: GitHub secret `POCKETOS_SIGNING_KEY`, plus Leeko's offline backup. The release workflow signs, verifies against the public key, and refuses to publish without it. |
| Staged releases | `manifest.json` `rollout` (0-100). Devices are split into 100 fixed groups by MAC; automatic offers go to groups below the rollout. The Rollout workflow raises, lowers or pauses (0) it. A manual check always offers the update. |
| Destructive actions | Factory reset and Wi-Fi change need the Boot button pressed on the device within 15 s, whether started from the website or the device. |
| Wi-Fi | Joins WPA2 or better only. The setup network is WPA2 with the per-device password. |
| Local only | No cloud, accounts or analytics. Outbound: Home Assistant (local), the daily update check (GitHub), NTP. |

## Accepted limits

- The settings page is plain HTTP on the home network (like most local devices). HTTPS would need a
  self-signed certificate and browser warnings. Revisit if a better local-trust option appears.
- Physical access: see below.

## Next: flash encryption and secure boot (needs a board to test)

Without these, someone holding a PocketOS and a USB cable can read its flash (Wi-Fi password,
Home Assistant token) or install other firmware over USB. Both features burn eFuses, which can't be
undone. Turn them on only after trying the full process on one test board.

1. Secure Boot V2 (RSA-3072): generate a key (`espsecure.py generate_signing_key --version 2`), keep
   it offline with the release key, and set `CONFIG_SECURE_BOOT=y`, `CONFIG_SECURE_BOOT_V2_ENABLED=y`,
   `CONFIG_SECURE_SIGNED_APPS_SCHEME_RSA_PSS`. The release workflow then also signs with this key.
2. Flash encryption, release mode: `CONFIG_SECURE_FLASH_ENC_ENABLED=y`,
   `CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y`, and NVS encryption
   (`CONFIG_NVS_ENCRYPTION=y`, HMAC-based on the ESP32-S3).
3. Factory: flash the plaintext image once; the device encrypts itself and burns its eFuses on first
   start. Update `tools/factory` to wait for that first start (it takes longer), and test that
   updates, rollback, factory reset and the recovery steps still work. In release mode, USB flashing
   of plaintext images is disabled for good, so the Software recovery page stops working: devices
   are then recovered with signed updates only.
