# First Edition pre-orders (Stripe)

## What customers see

- **Buy** shows **Pre-order First Edition** while pre-orders are open, and how many are left once 10 or
  fewer remain. The home page says pre-orders are open. Otherwise both stay "Coming soon".
- The button opens **Stripe Checkout** (hosted by Stripe, on any device), for one PocketOS First
  Edition at the First Edition price of £59.99 (usually £99.99) plus UK delivery, with a UK address only.
- After paying, the customer lands on `/order-confirmed`. It shows their order number, or that a bank
  payment is still clearing. Stripe emails the receipt.

## How it works

| Part | File | Job |
|---|---|---|
| Pre-order API | `api/preorder.js` | Says whether pre-orders are open, starts checkouts, reports a finished checkout's status |
| Webhook | `api/stripe-webhook.js` | Checks Stripe's signature, then emails each paid order (name, address, phone, amount) to `CONTACT_TO` |
| Shared | `api/_stripe.js` | Stripe API calls (pinned to API version `2026-10-28.endive`), limits, signature check |
| Tests | `firmware/host/test_preorder.py` | Runs everything against a pretend Stripe |

- **25 units.** Paid checkouts, plus checkouts still in progress (each holds a unit for 31 minutes), are
  counted from Stripe itself, so there's no database to keep in step. A 26th checkout can't start.
- **Deadline.** No new checkouts after 31 January 2027, 23:59 UK time.
- **One per order.** The quantity is fixed at 1.
- **Orders are recorded by the webhook, not the thank-you page**, so an order isn't lost if someone
  closes the tab after paying. If the order email can't be sent, Stripe retries for up to three days.
- **Refunds don't free a unit automatically.** To re-offer a cancelled unit, raise `limit` in
  `api/_stripe.js`.

Change the price, limit, deadline or delivery date in `PREORDER` in `api/_stripe.js`.

## Setting it up (once)

### In Stripe

1. Activate your account for **Leeeko LTD** (business details, bank account).
2. **Settings → Business → Public details**: set the website to `https://www.pocketos.co.uk`, and
   the **terms of service** to `https://www.pocketos.co.uk/terms`.
3. **Settings → Emails**: turn on **Successful payments** and **Refunds**, so customers get receipts.
4. **Settings → Payment methods**: choose what to accept (cards, Apple Pay and Google Pay are typical).
   The code doesn't fix the list. Stripe shows each customer the methods that suit them.
5. **Tax → Settings**: set the head office address (Leeeko LTD's registered office). Leeeko LTD isn't
   registered for VAT, so **don't add a UK registration** yet: no VAT is charged. Prices are set as
   VAT-inclusive, so if you register later and add the registration in Stripe, the price stays the same with
   VAT inside it. Stripe's threshold monitoring (Tax → Locations) shows when registering might be
   needed. Check with your accountant.
6. **Developers → API keys → Create restricted key** named "PocketOS website", with **Checkout
   Sessions: Write** and everything else **None**. Copy it (`rk_live_...`).
7. **Developers → Webhooks → Add endpoint**: `https://www.pocketos.co.uk/api/stripe-webhook`, with the
   events `checkout.session.completed`, `checkout.session.async_payment_succeeded`,
   `checkout.session.async_payment_failed`, `charge.refunded` and `charge.dispute.created`. Copy its
   **signing secret** (`whsec_...`).

Try everything in a **sandbox** first (Stripe's test environment, with its own `rk_test_` key and
webhook secret), then repeat steps 6 and 7 in live mode.

### In Vercel (Settings → Environment Variables, Production, marked Sensitive)

| Name | Value |
|---|---|
| `STRIPE_SECRET_KEY` | the restricted key |
| `STRIPE_WEBHOOK_SECRET` | the webhook signing secret |
| `PREORDER_DELIVERY_PENCE` | optional: a different UK delivery charge in pence (it's £4.99 unless set; `0` for free) |
| `PREORDER_DELIVERY_NAME` | optional, e.g. `Royal Mail Tracked 48` |
| `STRIPE_TERMS_CONSENT` | `1` once step 2's terms URL is set (adds a "I agree to the terms" tick box) |
| `STRIPE_AUTOMATIC_TAX` | `1` once step 5's head office address is set |
| `PREORDERS_OPEN` | `1` to open pre-orders. Remove it to close them. |

Redeploy after changing them. Order emails use the contact form's `CONTACT_SMTP_*` and `CONTACT_TO`.

### Before opening

With the sandbox keys: pre-order with Stripe's test card `4242 4242 4242 4242`, check the order email
arrives and `/order-confirmed` shows the order number, then refund it in Stripe and check the refund
email. Then switch to live keys.
