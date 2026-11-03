# Selling PocketOS in the UK: compliance checklist

Research notes from November 2026 for Leeeko LTD. Not legal advice: have a compliance
consultant or test lab confirm the plan before the first sale.

## 1. Radio and safety: Radio Equipment Regulations 2017

PocketOS has Wi-Fi, so it comes under the Radio Equipment Regulations, which cover radio use,
electrical safety and electromagnetic compatibility (EMC) in one. Because Leeeko LTD sells it under
its own name, **Leeeko LTD is the manufacturer** in law, even though Waveshare makes the board.

What the manufacturer must do (GB statutory guidance):
- Make sure it meets the essential requirements (radio, safety, EMC).
- Carry out conformity assessment, draw up **technical documentation** and a **declaration of
  conformity**, and keep both for **10 years**.
- Mark it with **UKCA or CE**. CE is now recognised in Great Britain indefinitely, so either works.
  Put the mark on the device, or on the box if the device is too small.
- Put **Leeeko LTD's name and address** and a **type, batch or serial number** on the device,
  or on the box if there's no room.
- Include **instructions and safety information in plain English** (the quick start guide does this).

**The open question for Waveshare.** Waveshare's product page lists no CE, FCC, UKCA or RoHS
certification for the ESP32-S3-Touch-AMOLED-1.75C. It uses the ESP32-S3 chip directly with
its own antenna, not a pre-certified module, so there's no module certificate to lean on. Ask them:
1. Do you have CE (RED) test reports for this board? Typically radio (EN 300 328), EMC
   (EN 301 489-1/-17), safety (EN 62368-1) and RF exposure (EN 62311 or EN 62479).
2. Can we use them in our technical file, as an unchanged board in your case?
3. Do you have a RoHS declaration and test report?
4. What's the battery's capacity (mAh), and do you have its **UN38.3 test summary** and safety
   data sheet?

If they can't provide test reports, a UK test lab can test PocketOS. Get a quote before setting
the launch date.

## 2. Hazardous substances: RoHS Regulations 2012

Electrical products must not exceed the limits for lead, mercury, cadmium and similar substances.
The manufacturer needs evidence, usually the supplier's RoHS declaration or test report. This
goes in the same technical file and declaration as above.

## 3. Product security: PSTI

Done: [statement of compliance](https://www.pocketos.co.uk/compliance), security reporting page and
support period (31 December 2029). A director signs and dates it before the first sale. Include it
in the box or link to it from the box.

## 4. Recycling: WEEE (electrical waste)

- Leeeko LTD is a producer: it sells electrical equipment under its own brand.
- Below **5 tonnes a year**, register as a **small producer** with the Environment Agency, within
  **28 days** of first selling, then by **31 January** each year. Report what you sold the year before.
- Mark the device with the **crossed-out wheeled bin** symbol and a date mark. The quick start guide
  has the symbol; it must also go on the product, or the box if there's no room.
- Selling to households also brings retailer take-back duties. Most small online sellers meet them
  by joining the **Distributor Take-back Scheme**. Check this on gov.uk when registering.

## 5. Batteries

- Selling equipment with a built-in battery makes Leeeko LTD a **battery producer** as well.
- Up to **1 tonne a year**, register as a **small producer** on the National Packaging Waste Database
  and report the battery weight and chemistry each year by 31 January. There's an annual charge.
- Put the battery producer registration number on invoices or delivery notes for any business
  buyers.

## 6. Shipping

PocketOS ships as lithium-ion batteries contained in equipment (UN3481). For Royal Mail, per
current carrier guidance:
- Batteries up to 20 Wh per cell and 100 Wh per battery; at most 2 batteries or 4 cells per parcel.
- The battery must have passed **UN38.3** testing. Get the test summary from Waveshare or the
  battery maker.
- Packaging must stop movement and short circuits (the fitted insert helps).
- Each parcel needs a **lithium battery handling label** (minimum 120 × 110 mm) with a contact phone
  number.

Check the chosen courier's current rules before the first shipment. They change.

## 7. On the box (for Pillbox Design)

- PocketOS name, and Leeeko LTD name and address
- UKCA or CE mark (once conformity is done)
- Crossed-out wheeled bin symbol
- Type or batch number, if not on the device
- Lithium battery handling label (on the outer shipping box, not the retail box)
- Statement of compliance (printed, or a link)

## Sources

- [Radio Equipment Regulations 2017: Great Britain (gov.uk)](https://www.gov.uk/government/publications/radio-equipment-regulations-2017/radio-equipment-regulations-2017-great-britain)
- [Waveshare ESP32-S3-Touch-AMOLED-1.75C documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75C)
- [RoHS Regulations 2012 (gov.uk)](https://www.gov.uk/government/publications/restriction-of-hazardous-substances-rohs-regulations)
- [Electrical and electronic equipment: producer responsibilities (gov.uk)](https://www.gov.uk/electrical-and-electronic-equipment-eee-producer-responsibility)
- [Waste batteries: producer responsibility (gov.uk)](https://www.gov.uk/guidance/waste-batteries-producer-responsibility)
- [Shipping lithium batteries in the UK with Royal Mail (World Options)](https://www.worldoptions.com/uk/news/shipping-lithium-batteries-in-the-uk-with-royal-mail/)
