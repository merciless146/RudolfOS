# Carbon Fibre and Moonlit Stag (1.5.9)

Two of Display Properties' static backgrounds, drawn by these scripts (numpy, OpenCV, Pillow and rsvg-convert). Nothing
in them is taken from other pictures.

- `rudolf-logo.json`: Rudolf's logo as SVG paths (the white parts and the red nose), traced from the logo image with
  `python3 vec2.py LOGO.png rudolf-logo.json`.
- `carbon.py rudolf-logo.json OUT`: Carbon Fibre: a 2x2 twill carbon weave under a resin sheen, with the logo in brushed
  metal (dark: silver on black carbon; light: graphite on silver carbon). Writes `carbon_d.png` and `carbon_l.png`.
- `real.py OUT [d|l]`: Moonlit Stag: a mountain lake at night (dark) and at dawn (light), with Rudolf (`stagdef2.py`'s white
  low-poly stag with his red nose, head high, a front hoof on a ledge) on a boulder; his hooves rest on the rock's top. Writes `stagr_d.png` and `stagr_l.png`.

The pictures go into `desktop/base_v24.html`'s `DYN` table as 1600x900 WebP (`l`, `d`) and 176x99 JPEG thumbnails
(`tl`, `td`); `build19.py` moves them into `iso/www/a/`.
