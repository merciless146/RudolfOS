# Rudolf OS 1.5.2: the Rudolf OS Library

Released as stable 1.5.2.261106 (serial 2026110607) on 2026-11-06. The hardware parts first planned here (devices
without a driver, Bluetooth from Intel, MediaTek and Qualcomm, sound on newer laptops, Steam and Proton,
Thunderbolt and USB4, built-in Intel cameras) and the Library's remaining tests moved to **docs/plan-1.5.3.md**.

## What 1.5.2 has

- **The Rudolf OS Library** (docs/plan-library.md; #93, the rudolfos-library repository):
  - Quake's free game data and Emu's DS player moved out of the desktop layer into the Library, a package repository
    signed with its own key, which can add apps for the desktop but never change Rudolf OS itself;
  - App Store › Rudolf OS Library;
  - "download it?" when Quake or Emu needs its package;
  - Emu's DS player comes back by itself on computers that played DS games;
  - the disc's repository carries both.
- **Emu: Game Boy games in the library** (#94): Game Boy, Color and Advance games, and ZIP files whose names have
  spaces.
- **Windows games in Wine** (#95, #96): the game in front gets the keyboard (WM_TAKE_FOCUS) and gets it back when
  clicked; a game that goes full screen covers the whole screen, whether it asks (_NET_WM_STATE) or makes its
  borderless window the size of the screen.

Tested on the Vivobook: the Library's list and key, Quake from the Library, a Game Boy Color game added and played,
and Starport GE in full screen with the keyboard.

## The Library, as planned

All of docs/plan-library.md. In short:
- Quake and Emu move out of the desktop layer into the Library (the `rudolfos-library` repository, signed with its own
  key);
- an App Store tab for the Library;
- "download it?" when Quake or Emu is opened without its package;
- reinstalling them on computers that used them;
- the disc's repository still carries both.

The repository, the Library key pair, the secret `RPK_LIBRARY_KEY` and Pages were set up on 2026-11-04.
