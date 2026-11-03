# Concept renders

Makes the product images in `site/media/product/` with three.js in headless Chromium.

Setup, once, inside this folder:

- `lib/three.module.js` and `lib/jsm/{environments/RoomEnvironment.js,geometries/RoundedBoxGeometry.js,utils/BufferGeometryUtils.js}` from three.js r169
- `tex/home.png`, `tex/menu.png` (screens from `make -C firmware/host sim`, converted to PNG)
- `tex/font.ttf`: Onest Medium as TrueType (from `site/fonts/onest-500.woff`), for the size labels
- `tex/pocketos-logo-white.svg`, `tex/pocketos-mark-white.svg`, `tex/pocketos-logo-horizontal-white.svg` from `brand/`

Then: `python3 render.py OUT device,angle,box,open,inside,contents,size,scale,home` (needs Playwright and Pillow).
Packaging dimensions, insert layout and accessories are concepts, not final.
