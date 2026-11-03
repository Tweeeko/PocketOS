import sys, os, threading, functools, http.server, base64, io
from playwright.sync_api import sync_playwright
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1]
shots = sys.argv[2].split(',')
scale = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0   # 1.0 = final (2400x1800 -> 1600x1200)
W, H = int(2400 * scale), int(1800 * scale)

h = functools.partial(http.server.SimpleHTTPRequestHandler, directory=HERE)
class Q(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a): pass
srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Q, directory=HERE))
threading.Thread(target=srv.serve_forever, daemon=True).start()
port = srv.server_address[1]

def backdrop(w, h):
    # the site's night blue, with a soft studio pool of light behind the product
    bg = Image.new('RGB', (w, h), (7, 11, 28))
    glow = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(glow)
    d.ellipse([w * .08, h * .02, w * .92, h * 1.05], fill=255)
    glow = glow.filter(ImageFilter.GaussianBlur(w * .16))
    top = Image.new('RGB', (w, h), (24, 32, 72))
    bg = Image.composite(top, bg, glow)
    return bg

with sync_playwright() as p:
    b = p.chromium.launch(args=['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'])
    pg = b.new_page(viewport={'width': W, 'height': H})
    pg.on('console', lambda m: print('console:', m.text) if m.type in ('error', 'warning') else None)
    pg.on('pageerror', lambda e: print('pageerror:', e))
    for s in shots:
        pg.goto(f'http://127.0.0.1:{port}/scene.html?shot={s}&w={W}&h={H}')
        pg.wait_for_function('window.__done === true', timeout=600000)
        data = pg.evaluate("document.querySelector('canvas').toDataURL('image/png')")
        im = Image.open(io.BytesIO(base64.b64decode(data.split(',')[1]))).convert('RGBA')
        bg = backdrop(W, H).convert('RGBA')
        bg.alpha_composite(im)
        marks = pg.evaluate('window.__marks || null')
        if marks:   # size drawing: dimension lines under the front view and the side view
            from PIL import ImageFont
            dr = ImageDraw.Draw(bg); col = (196, 205, 232, 255); lw = max(2, int(3 * scale))
            try: font = ImageFont.truetype(os.path.join(HERE, 'tex', 'font.ttf'), int(54 * scale))
            except Exception: font = ImageFont.load_default()
            y = marks['w'][0][1] + 90 * scale
            for (x0, _), (x1, _), label in ((marks['w'][0], marks['w'][1], 'About 55 mm'), (marks['d'][0], marks['d'][1], 'About 15 mm')):
                dr.line([(x0, y), (x1, y)], fill=col, width=lw)
                for x in (x0, x1): dr.line([(x, y - 22 * scale), (x, y + 22 * scale)], fill=col, width=lw)
                tw = dr.textlength(label, font=font)
                dr.text(((x0 + x1) / 2 - tw / 2, y + 34 * scale), label, font=font, fill=col)
        fw, fh = int(1600 * scale), int(1200 * scale)
        out = bg.convert('RGB').resize((fw, fh), Image.LANCZOS)
        out.save(os.path.join(OUT, s + '.png'))
        print('rendered', s)
    b.close()
