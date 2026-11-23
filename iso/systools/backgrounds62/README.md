# Desert Camp (1.6.2)

The third moving background in Display Properties, drawn by `desert.py` (numpy, OpenCV, Pillow). Nothing in it is taken from
other pictures; the owner's photos were a reference for the look only.

- `desert.py OUT`: an open desert with sparse dry brush, clusters of tents of several styles (dome, ridge, bell, tunnel,
  cabin), each cluster round its campfire, one tent in front, and the campers' things (chairs facing the fires, coolers with
  lanterns, firewood, packs and boots by the doors, water jugs, a folding table with a stove). Light: sunrise, an orange sky
  with a few high clouds. Dark: night, stars and a nebula, the tents lit from inside. Writes `desert_l.png`, `desert_d.png`
  (1600x1067) and `desert-scene.json` (where the moving parts are).

The moving parts are drawn over the picture by `desktop/desert62.js` (Chromium and Lite) and by `rudolf-native.c`'s `ds_draw`
(Native), from the same numbers: at sunrise the sun's mirage on the horizon and the campfires' smoke; at night the flames, the
lit tents breathing, the twinkling stars and the shooting stars.

The pictures go into `desktop/base_v24.html`'s `DYN` table as `desert`: 1600x1067 WebP (`l`, `d`, quality 72) and 176x117
JPEG thumbnails (`tl`, `td`); `build19.py` moves them into `iso/www/a/`.
