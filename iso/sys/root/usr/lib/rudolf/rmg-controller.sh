#!/bin/sh
# (1.6.2) Emu's Nintendo 64 games: controller 1 for Rosalie's Mupen GUI, set up once. A fresh install of it has no
# controller at all (its input plugin leaves player 1 unplugged until its settings are saved: RMG-Input's
# SaveDefaultSettings), so a game started from Emu took no key or pad. rudolfd's native/run runs this as the desktop's
# (or the account's) user before it starts the emulator: when its settings have no "Profile 0" yet, controller 1 is
# plugged in on "Automatic" (the first gamepad SDL finds, else the keyboard), with a gamepad's standard layout and the
# keyboard both mapped. A profile the player made in its settings (F4) is never changed.   GPL-2.0-or-later.
#   Gamepad: A = A (south), B = X (west), Start, Z = left trigger, L/R = the shoulders (R also the right trigger),
#            the D-pad, C = the right stick, the stick = the left stick.
#   Keyboard: A = X, B = C, Start = Enter, Z = Z, L = Q, R = E, D-pad = W A S D, C = I J K L, the stick = the arrows.
C=${RMG_CFG:-$HOME/.var/app/com.github.Rosalie241.RMG/config/RMG/mupen64plus.cfg}
S="[Rosalie's Mupen GUI - Input Plugin Profile 0]"
[ -f "$C" ] && grep -qF "$S" "$C" && exit 0
mkdir -p "$(dirname "$C")" || exit 0
# NAME  then pairs of TYPE DATA EXTRA: TYPE -1 a key (SDL scancode), 0 a gamepad button, 1 a gamepad axis (EXTRA 1: +)
m() { n=$1; shift; t=""; d=""; e=""; nm=""
	while [ $# -ge 3 ]; do t="$t;$1"; d="$d;$2"; e="$e;$3"; nm="$nm;$n"; shift 3; done
	printf '%s_Name = "%s"\n%s_InputType = "%s"\n%s_Data = "%s"\n%s_ExtraData = "%s"\n' "$n" "${nm#;}" "$n" "${t#;}" "$n" "${d#;}" "$n" "${e#;}"; }
{ [ -s "$C" ] && echo
  printf '%s\n\n' "$S"
  printf 'PluggedIn = True\nDeviceName = "Automatic"\nDeviceType = 2\nDevicePath = ""\nDeviceSerial = ""\nDeviceGuid = ""\n'
  printf 'Deadzone = 9\nSensitivity = 100\nPak = 0\nRemoveDuplicateMappings = True\nFilterEventsForButtons = True\nFilterEventsForAxis = True\n'
  m A 0 0 0 -1 27 0
  m B 0 2 0 -1 6 0
  m Start 0 6 0 -1 40 0
  m ZTrigger 1 4 1 -1 29 0
  m LeftShoulder 0 9 0 -1 20 0
  m RightShoulder 0 10 0 1 5 1 -1 8 0
  m DpadUp 0 11 0 -1 26 0
  m DpadDown 0 12 0 -1 22 0
  m DpadLeft 0 13 0 -1 4 0
  m DpadRight 0 14 0 -1 7 0
  m CButtonUp 1 3 0 -1 12 0
  m CButtonDown 1 3 1 -1 14 0
  m CButtonLeft 1 2 0 -1 13 0
  m CButtonRight 1 2 1 -1 15 0
  m AnalogStickUp 1 1 0 -1 82 0
  m AnalogStickDown 1 1 1 -1 81 0
  m AnalogStickLeft 1 0 0 -1 80 0
  m AnalogStickRight 1 0 1 -1 79 0
} >> "$C"
