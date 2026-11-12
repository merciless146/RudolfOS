# Rudolf OS games: a plan (asked for on 2026-11-11)

Games made for Rudolf OS, published in the Rudolf OS Library (merciless146/rudolfos-library) and installed from the App
Store. They are played first on Rudolf OS computers (keyboard, mouse, controllers), and every one is built so that it
runs unchanged on the 1.6.0 mobile version (touch, portrait screens, ARM64, little memory), as all of Rudolf OS's own
apps are to.

Rudolf OS already has: Checkers and Chess (Board Games), Five-Card Draw, Texas Hold'em, Pai Gow Poker, Hearts and
Solitaire (Card Games), Snake, Quake and the emulators (Library), the WOS Galaxy Map, BlockLua, and cloud gaming
(Xbox Cloud Gaming, GeForce NOW). These games fill what is missing.

## Where they live
- **The code:** each game is a `webapp` package in rudolfos-library (`apps/rudolf-<game>/PKGINFO` and `www/`), built,
  signed with the Library's key and published by its workflow like rudolf-quake and rudolf-emu. Installed, it is
  mounted at `/pkg/rudolf-<game>/` and served by rudolfd; removing it deletes one file.
- **In the system (RudolfOS):** a few lines per game, the Start menu and the App Store's Games tab entry with its icon,
  title and size. Opened without its package, it offers to download it, as Quake does. The system download does not
  grow with games.
- **This plan:** in RudolfOS (docs/plan-games.md), with the version plans.

## Rules for every game (desktop now, mobile in 1.6.0)
1. **One input layer** (a small shared library in the Library, `rudolf-gamekit`): keyboard, mouse, touch and gamepad
   at once, mapped to the game's actions. A game never assumes a mouse or a keyboard; on-screen controls appear only
   when touch is used.
2. **Any window shape:** the playfield scales to the window, portrait included (tablet mode, 1.5.8; phones, 1.6.0).
3. **Speed:** 60 frames a second drawn by the processor alone, Canvas 2D, no graphics card needed. The yardstick is
   the Vivobook on the Lite desktop (WebKit, software drawing). The same code on Chromium and WebKit.
4. **Size:** under about 2 MB per game, everything inside the package; nothing downloaded while playing.
5. **Saves:** through rudolfd's mirror (they follow the account and are backed up), with export and import as a file.
6. **Pausing:** on the lock screen, the screen saver, a call (1.5.9 Part 7), a notification the person opens, the window
   covered or minimised, and on a phone when the app goes to the background.
7. **Offline:** everything works without a connection; online leaderboards only where a game has them, and optional.
8. **Accessibility:** difficulty levels, a reduce-flashing setting, colours that do not rely on red and green alone,
   remappable keys and buttons, captions for any spoken or important sound.
9. **Original work:** original names, art, music and rules where rules are protected; no assets from other games.
   The falling-blocks game does not use the name or look of any trademarked game.

## The games

### Night Flight (the only Christmas-themed game)
A top-down, vertically scrolling bullet-hell shooter. Rudolf, the OS's mascot, flies up through a portrait playfield
and fires lasers from his nose.

**Play**
- **Nose laser:** tap or hold to fire a rapid stream; hold longer to charge a piercing beam. "Glow" pick-ups upgrade it
  (wider spread, then homing shots).
- **Focus:** hold a key (or a second finger) to move slowly; Rudolf's small hitbox, a glowing point at his nose,
  shows, for threading through dense patterns.
- **Glow Burst (the bomb):** a few per life; clears the bullets on screen and briefly turns corrupted enemies back to
  their colours.
- **Grazing:** passing close to bullets fills a meter for score and extra bombs.
- **Bosses:** one at the end of each stage, with phases and named bullet patterns.
- **Rescues:** corrupted friends can be saved (hit with a Glow Burst or the charged beam) for bonuses and extra lives.
- **Modes:** Story (6 stages), Practice (any stage reached, slow-motion assist), Score Attack, and after the ending a
  lighter "Restored" mode.

**The light-to-dark arc**
1. **Stages 1-2:** a bright, colourful winter night: twinkling lights, candy colours, friendly villagers and workshop
   helpers. The first corrupted enemies are few and easy to read against the colour.
2. **Stages 3-4:** the corruption spreads: colours drain, lights flicker and glitch, friends begin to turn.
3. **Stages 5-6:** dark and corrupted: deep purples and blacks, static, inverted ornaments, cracked toys and twisted
   machinery, bullets in sickly glowing colours; the final boss is the source of the corruption.
- Clearing the game brings the colour back to every stage.

**Art direction**
- **Rudolf and the friendly characters:** soft and cartoonish, child-friendly, in the spirit of open-source mascots
  such as Linux's Tux (without copying any of them): round shapes built from circles and ovals, no sharp corners; a
  big head and a small chubby body, short legs, stubby antlers with rounded tips; large friendly eyes with highlights,
  a small smile; Rudolf's nose glows warmly and pulses brighter as the laser charges. A few flat, warm, bright colours,
  a soft outline, one highlight and one shadow tone. Bouncy, squashy animation: Rudolf wobbles as he dodges, blinks,
  and cheers when a friend is rescued. Villagers, workshop helpers and small woodland animals in the same style.
- **Corrupted characters:** sinister, still simple enough to read at speed: angular and jagged silhouettes, cracks,
  thorny edges, uneven limbs; narrow glowing eyes (toxic purple, red or green) without highlights; dark, desaturated
  bodies with sickly glowing accents, flickering static and "corruption" dripping at the edges; jerky, glitching
  animation. A turning friend changes step by step from the round, colourful version to the sharp, dark one, and pops
  back when rescued, so the corruption is seen, not told.
- **Child-friendly:** creepy, not gory: no blood, nothing grotesque; beaten enemies burst into sparkles and static;
  bosses are menacing by shape, size and colour, not violence. The reduce-flashing setting tones the glitch effects
  down.
- **Rudolf's own design:** Rudolf is Rudolf OS's own character. Kept clearly distinct from Rudolph the Red-Nosed
  Reindeer (a trademarked character): Rudolf OS's own antler shape, colours, proportions and style, and nothing from
  that story (Santa's reindeer team, its well-known characters, songs or places). The nose laser is a mechanic, not a
  borrowing. Worth a check by someone qualified before the game is published.

**Controls**
- Desktop: arrows or WASD to move, Z to fire (hold to charge), Shift for focus, X for Glow Burst, Esc to pause; a
  controller (left stick or d-pad, A fire, right trigger focus, B bomb).
- Phone (1.6.0): drag anywhere to steer (Rudolf follows the finger at an offset, so the finger does not hide him), fire
  is automatic, a second finger held for focus, a two-finger tap for Glow Burst.

**Technical**
- Canvas 2D; bullets in fixed pools (no allocation while playing); circle hit tests; a fixed time step, so runs are
  deterministic and replays can be saved and watched.
- Up to about 600 bullets on screen on the Vivobook on Lite; more where the computer allows (measured at start-up and
  in the settings).
- The corruption effects (palette shifts, scanlines, glitch slices) drawn cheaply in Canvas; none needs a graphics card.
- Sound: Web Audio, short effects and looped music in Opus, all inside the package (about 2 MB in all as the target).

### The other games (neutral themes)
| Game | What it is | Desktop | Phone (1.6.0) |
|---|---|---|---|
| **Fault Lines** | Mine-sweeping logic puzzle: clear the field without opening a fault; daily challenge, three sizes | click, right-click to mark | tap, long-press to mark |
| **Block Drop** | Falling-blocks puzzle with its own twists (special blocks that clear by colour); original name and look | keyboard | swipes and taps |
| **Logic Pack** | Sudoku, nonograms and number puzzles, generated without end, with hints | mouse or keys | taps |
| **Wordsmith** | Daily word puzzle and free play, with an offline word list in the package | keyboard | on-screen keyboard |
| **Galaxy Conquest** | Turn-based strategy on the WOS Galaxy Map: claim systems, trade, defend | mouse | taps; turns suit phones |
| **BlockLua Arcade** | Small games people make in BlockLua, shared through the Library and played by anyone; a few starter games | as each game sets | as each game sets |
| **Blitzen's Quest** | A text adventure narrated by Blitzen, who answers what you type (offline with Blitzen's local model where the computer has one) | keyboard | typing or voice |
| **Rally** | Top-down racing: controller, keyboard or tilt, split screen on one computer, ghost laps | keyboard, controller | tilt or touch |
| **Tower Defense** | Build and upgrade towers along paths | mouse | taps |
| **Pinball** | A physics table in Canvas | flipper keys | a flipper on each side of the screen |

### Additions to what Rudolf OS has
- **Card Games:** FreeCell and Spider.
- **Chess:** levels and hints from Blitzen.
- **Hearts and Checkers:** play against other Rudolf OS computers on the same network (WebRTC between desktops, found
  through rudolfd); the first step towards phone and computer players meeting in 1.6.0.

## Order
1. **1.5.9:** `rudolf-gamekit` (input, scaling, saves, pausing); then **Fault Lines**, **Block Drop**, **Logic Pack**
   and **Night Flight** (stages 1-2 and the engine first; stages 3-6 when the art for the corruption is settled). Small,
   and they play as well with a mouse as with a finger: the first proof of the input layer.
2. **1.5.9-1.6.0:** Wordsmith, Galaxy Conquest, FreeCell and Spider, Chess levels.
3. **1.6.0:** every game checked on the mobile build (QEMU aarch64 with a touch screen, then the phone); Hearts and
   Checkers over the network; BlockLua Arcade.
4. **Later:** Blitzen's Quest, Rally, Tower Defense, Pinball.

## Progress
- **2026-11-11:** `rudolf-gamekit` (rudolfos-library `lib/gamekit/gamekit.js`, copied into each game by tools/publish.sh)
  and **Fault Lines** (`apps/rudolf-faultlines`, 16 KB), with `tests/faultlines_t.js` (21 checks, desktop and phone
  sizes) run by the Library's check.yml. The desktop's side: `desktop/games58.js` (LIB_GAMES: Start menu, Games tab,
  the download offer; the kit's messages: saves in the desktop's database per account, pause and resume, settings),
  tested by `iso/qt/shell2/games58_t.js`.
- **2026-11-11:** **Night Flight**, moved first: the engine and stages 1-2 with their bosses (rudolfos-library
  `apps/rudolf-nightflight`: art.js, audio.js, stages.js, nightflight.js; about 31 KB as a package), Story, Practice,
  Score Attack, three difficulties, replays; `tests/nightflight_t.js` (22 checks: a replay matching a minute of play,
  600 bullets drawn in about 2 ms in Chromium). In LIB_GAMES. Next: Night Flight on WebKit (the Lite engine's frame
  time), the Vivobook; then Block Drop and Logic Pack; Night Flight's stages 3-6 once the corrupted art is settled.
- **2026-11-11:** **Frost Barricade** (asked for; for older players, cartoon horror and combat): an original
  barricade-defence shooter, `apps/rudolf-frostbarricade` (about 26 KB as a package). Corrupted elves in Night Flight's
  style, nine winter weapons, days of repair, search and training, survivors, fifteen nights; the rescuer on Night 15
  is an original character, Captain Quill (a rockhopper penguin in a red winter coat, with a sleigh pulled by snowy
  owls), not another project's mascot. `tests/frostbarricade_t.js` (26 checks). In LIB_GAMES.
- **2026-11-12:** **Block Drop** (`apps/rudolf-blockdrop`, about 20 KB as a package). Its own rules and look, not a
  copy of a trademarked game: an 8-wide, 16-deep well; pieces of three and four blocks; colours that do not follow the
  shape, each with its own mark (circle, triangle, square, diamond, cross). The twist: every 12th piece carries a
  prism; where it lands, every block of its colour bursts, the blocks above tumble down their columns, and rows that
  fill clear in a chain (worth more each step); while it falls, the blocks it would burst are outlined. Classic
  (faster every 10 rows), Relaxed (one speed), Daily (3 minutes, the same blocks for everyone). Touch: drag to move,
  tap to turn, flick down to drop, and a touch bar; on a tall window the panel moves above the well.
  `tests/blockdrop_t.js` (33 checks). In LIB_GAMES. Next: Logic Pack.
- **2026-11-12:** **Logic Pack** (`apps/rudolf-logicpack`, about 24 KB as a package): Sudoku, Picture Grid (nonograms)
  and Cages (Latin squares with + − × ÷ cages), made in the game (`logic.js`, under 25 ms each), each with exactly one
  answer. Sudoku Easy solves with naked singles, Medium with hidden singles too, Hard is only unique; Picture Grid
  5×5, 10×10, 15×15, solvable line by line (a cell is given where line logic stalls); Cages 4×4 to 6×6. A Daily
  puzzle per kind. Notes, undo, clashes and wrong cages marked (with a bar, not colour alone), clues greyed when their
  line matches, hints that fix a wrong entry or do the next step and say why; each kind's puzzle in progress kept.
  The pad goes under the grid on a tall window. `tests/logicpack_t.js` (41 checks). In LIB_GAMES. Next (1.5.9-1.6.0):
  Wordsmith, Galaxy Conquest, FreeCell and Spider, Chess levels.

## Frost Barricade 1.1: twenty-five nights, the weather radio, the weather, wendigos
Asked for on 2026-11-11 and built (rudolfos-library#6, on #5's eye colours, bloaters and muggers).
- **Twenty-five nights**; the sleigh comes at the end of the last, in the great blizzard.
- **A prep screen before every night** with a **weather radio**: tonight's weather and tomorrow night's, a warning
  the day before a blizzard, and on Day 22 always the warning of the three-night blizzard.
- **The schedule**, fixed per game from its seed: days 1-7 clear or fog; a blizzard on day 8 or 9 and on 17 or 18
  (the other day clear or fog); the great blizzard on 23-25; one full moon (20% a day on days 8-14, 60% on 15-20, 80%
  on 21, otherwise 22), never on a blizzard's day.
- **Clear:** full sight; searching goes quicker. **Full moon:** brighter; every white-eyed elf becomes a red-eyed
  sprinter; a wendigo comes. **Fog:** sight is a cone of light from Rudolf's nose towards the crosshair; elves out of
  it show as ripples of sound; their sounds louder; searching finds less and scouts are more often lost.
  **Blizzard:** a shorter, narrower cone; no ripples; nothing sprints; the survivors' aim suffers; no searching.
- **The wendigo** (reworked in 1.2): a mini-boss, always red-eyed; a screech (and a caption) a few seconds before it
  appears; huge, very tough and slow, drifting towards Rudolf's lane; only one on the field at a time. Every 5-8 s it
  stops for 1.5 s to draw in a screech, then lets it go down its lane with the screen shaking: it hurts survivors in
  its path (not the barricade), and if it reaches Rudolf he is stunned, his sight blurs and his ears ring, fully for
  3 s, then fading. One under the full moon, one on Night 24, two on Night 25 (one after the other). An original
  design: gaunt, long-limbed, sharp branching antlers, frightening by shape rather than gore.
- **Tests:** `tests/frostbarricade_t.js` (54 checks in 1.1).

## Frost Barricade 1.2: shielders and Bash
Asked for on 2026-11-11 and built (rudolfos-library#7).
- **Shielders:** cyan eyes, above-average health, from Night 5. Their shield covers the waist, chest and head (not
  the lower legs and feet) and stops every shot, survivors' bolts included; piercing shots stop at it; fire reaches
  round it for half. Explosives and **Bash** knock it loose and the shielder back. From Night 10 a shielder goes back
  for its own shield; from Night 18 an ordinary white-eyed elf walking over a fallen shielder's shield may take it up
  (it keeps its white eyes and its own health).
- **Bash:** Rudolf's melee blow over the barricade at the elf against it nearest the crosshair (F, Y on a controller,
  the middle mouse button, or the weapon bar's button).
- **Tests:** `tests/frostbarricade_t.js` (63 checks).

## Tests
- Each game: a browser test (qt/shell2 in RudolfOS for the Start menu and Games tab entries and the download offer; in
  the Library, a test per game: it starts, takes input from keyboard, mouse, touch and gamepad, pauses and resumes,
  saves and loads, at a desktop size and a phone size in portrait).
- Night Flight: frame time with 600 bullets in Chromium and in WebKit (the Lite engine check, webkit-check.yml), a
  deterministic replay that ends with the same score twice, the reduce-flashing setting.
- On the Vivobook (devlink and remote access): each game played on Standard and Lite, with memory and frame rate noted
  in docs/testing.md.

## 25 Days (Frost Barricade 1.3)

Asked for on 2026-11-12. The game is renamed **25 Days** (the package stays `rudolf-frostbarricade`, so saves carry on).

- A grittier look, like the old violent Flash games: darker, washed-out colours, film grain and a vignette, dead trees
  and fence posts, dark ichor splatter, tumbling chunks and stains that last the night, muzzle smoke and spent casings.
- Rudolf redrawn after Rudolf OS's portrait (the boot splash): an angular, faceted white stag with branching antlers, a
  diamond ear, a slanted eye, a ruff and the red nose, full body, with a bandolier.
- A wider field and lanes, a longer barricade. Rudolf moves up and down behind it (W/S, the arrows, a controller's left
  stick, a drag behind the barricade) and aims within 15 degrees of level.
- Shots fly on past the crosshair until an elf or a shield stops them; the Ornament Launcher still bursts at the crosshair.
- Unlimited ammunition, quicker reloads (searching finds scrap instead of ammunition); the elves 30% faster, the
  sprinters most (the wendigo unchanged).
- The survivors shoot each in their own time, about 40% hits untrained to 85% trained; below 5% of the barricade they
  panic, drop their crossbows and run until morning.

