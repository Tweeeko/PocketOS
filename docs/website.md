# The website (Vercel)

`site/` is the public PocketOS website: home page with the promo film, the browser installer,
help and what's new. Vercel hosts it for free.

## How it's built

`vercel.json` tells Vercel to run `site/build.sh`, which:
1. copies `site/` and `CHANGELOG.md` into `_site/`;
2. downloads the latest release's `manifest.json` and `pocketos-full.bin` from GitHub;
3. writes `firmware/manifest.json` for the installer (ESP Web Tools).

The installer needs its own copy of the firmware because browsers aren't allowed to load files
from GitHub's release downloads into a web page.

## One-time setup (from an iPad)

1. Sign in at vercel.com with GitHub. Choose **Add New → Project** and import
   **Tweeeko/PocketOS**. Leave the settings as they are (`vercel.json` supplies them) and press
   **Deploy**. Your address is shown when it finishes, e.g. `pocketos.vercel.app`. A custom
   domain can be added under the project's **Settings → Domains**.
2. In the Vercel project: **Settings → Git → Deploy Hooks**. Create one called `release` for
   branch `main` and copy its URL.
3. On GitHub: **Settings → Secrets and variables → Actions → New repository secret**.
   Name `VERCEL_DEPLOY_HOOK`, value the URL from step 2.

After that:
- pushing to `main` redeploys the site (Vercel does this by itself);
- running **Actions → Release** redeploys it too, so the installer always has the new version.

## Privacy

No cookies, analytics or tracking scripts. Don't switch on Vercel Web Analytics or Speed
Insights; PocketOS promises no tracking. Vercel, like any host, sees visitors' internet
addresses in its logs. The installer button loads ESP Web Tools from unpkg.com.

## Trying it locally

```
sh site/build.sh && python3 -m http.server -d _site 8000
```
The installer only works over `https://` or `localhost`, in Chrome or Edge.

## Contact form

`site/contact.html` posts to `api/contact.js`, a Vercel function that emails each message through
iCloud Mail. No address is written anywhere in the website or the code: it comes from three
environment variables, set in Vercel under **Project → Settings → Environment Variables**
(Production):

| Name | Value |
|---|---|
| `CONTACT_SMTP_USER` | the iCloud Mail address messages are sent from |
| `CONTACT_SMTP_PASS` | an app-specific password for that Apple Account (appleid.apple.com → Sign-In and Security → App-Specific Passwords) |
| `CONTACT_TO` | where messages arrive (can be the same address) |

Redeploy after setting them. Until they're set, the form says messages can't be sent right now.
Visitors' replies go straight to them (the email's Reply-To is the visitor). Nothing is stored.
Tests: `python3 firmware/host/test_contact.py`.

## Domain

The website's address is **https://www.pocketos.co.uk**. In Vercel → Project → Settings → Domains,
add both `www.pocketos.co.uk` and `pocketos.co.uk`, with `pocketos.co.uk` redirecting to `www`.
The printed quick start guide's QR code opens `https://www.pocketos.co.uk/start`, which
`vercel.json` forwards to the setup guide, so keep that redirect in place.
