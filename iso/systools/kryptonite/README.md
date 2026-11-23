# Rudolf Kryptonite's logo (2.0, now WendigOS)

Rudolf Kryptonite (renamed WendigOS on 2026-11-22; a new logo comes in a step of its own) was Rudolf OS 2.0 on the newest kernel with Wayland (docs/plan-2.0.md). Its logo is Rudolf's logo with
two changes (the owner, 2026-11-20): carbon-fibre off-black instead of white, and the glowing nose green instead of red.

- `kryptonite_logo.py ../backgrounds59/rudolf-logo.json OUT [SIZE]`: draws it from the logo's own traced paths
  (`rudolf-logo.json`) with the same 2x2 twill carbon weave as the Carbon Fibre background (`../backgrounds59/carbon.py`),
  a thin light rim so the off-black reads on dark backgrounds, and the nose `#2bff6a` with its glow. Needs numpy, Pillow
  and rsvg-convert. Nothing in it is taken from other pictures.
- `kryptonite-logo.png` (1024x1024, transparent) and `kryptonite-logo-256.png`: its output.
