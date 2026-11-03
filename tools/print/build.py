#!/usr/bin/env python3
# PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
"""Builds the printed quick start guide: quick-start.pdf (A6, two pages: front and back).

    python3 tools/print/build.py

Needs Python Playwright with Chromium. The page is white to the edge, so it needs no bleed;
if the printer asks for it anyway, it can be added in their artwork check.
qr-start.svg points at https://www.pocketos.co.uk/start, which the website redirects to
the setup guide (vercel.json), so the printed code keeps working if the guide moves.
"""
import os
from playwright.sync_api import sync_playwright

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "quick-start.pdf")

with sync_playwright() as p:
    b = p.chromium.launch()
    pg = b.new_page()
    pg.goto("file://" + os.path.join(HERE, "quick-start.html"))
    pg.wait_for_load_state("networkidle")
    pg.evaluate("document.fonts.ready")
    over = pg.evaluate("[...document.querySelectorAll('.sheet')].map(s => s.scrollHeight > s.clientHeight + 1)")
    if any(over):
        raise SystemExit("Text doesn't fit on page " + str(over.index(True) + 1))
    pg.pdf(path=OUT, width="105mm", height="148mm", print_background=True, prefer_css_page_size=True)
    b.close()
print("wrote", OUT)
