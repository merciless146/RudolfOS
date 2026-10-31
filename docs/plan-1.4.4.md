# Rudolf OS 1.4.4: cleanup, the volume and brightness keys, the network indicator, the Settings layout, the screen saver, the taskbar and power saving, and the clock (plan)

A pass over everything Rudolf OS builds and ships, looking for code and files that nothing uses (2026-10-30, on
1.4.2). The system programs are clean: no unused functions in `iso/sys/*.c` (gcc `-Wunused-function`, and every
non-static function is called), and every rudolfd API route has a caller (`lsync` is called by the script rudolfd
injects into the page). What is left is small. 1.4.4 removes it and adds a build check so it does not come back.
Nothing a person uses changes by the cleanup. 1.4.4 also makes the volume and brightness keys work (part 2).

# Part 1: cleanup

## Items

| # | What | Where | Why it can go |
|---|---|---|---|
| 1 | `sysuMB` | `desktop/sysupd25.js` | defined, never called |
| 2 | `tmE` | `desktop/tm26.js` | defined, never called (Task Manager fills its rows with `textContent`) |
| 3 | `frames/_metadata/generated_indexed_rulesets/_ruleset1` | `iso/sys/root/usr/lib/rudolf/frames/` | a cache Chromium writes when it loads the web frames extension, committed by accident with 1.1; the session copies the extension to `/run/rudolf/frames` at every start and Chromium writes its own there. `.gitignore` gets `_metadata/` |
| 4 | `/usr/bin/ostree` | base layer | the ostree command line; Flatpak uses the ostree library (`libostree-1.so.1`), not this program, and nothing in Rudolf OS runs it |
| 5 | `/usr/bin/dbus-send`, `/usr/bin/dbus-run-session` | base layer | nothing in Rudolf OS runs them (the session starts `dbus-daemon` itself; Flatpak talks to D-Bus through its library) |
| 6 | `/usr/bin/pacat` and its links `paplay`, `parecord` | base layer | PulseAudio's command-line player and recorder; Rudolf OS uses the sound server through `pactl` and the library, and apps through the library, never these |

Kept, and why (checked the same way, but used):
- `gpg`, `gpgv`, `gpg-agent`, `gpgconf`: Flatpak runs them to check the signatures of app repositories.
- `fusermount3`: Flatpak's document portal mounts with it.
- The nine desktop functions that look unused in `desktop/*.js` (`homeRekey`, `kvDel`, `pwFind`, `calcModes`,
  `wpPrint`, `passkeyCheck`, `lxCheckLines`, `wpExtend`, `wpWmCss`): `build19.py` patches calls to them into the base
  page (`base_v24.html`), so they are only called from there.

## How

**Desktop (items 1-2).** Delete the two definitions; rebuild `iso/www/index.html`.

**The cache (item 3).** `git rm -r iso/sys/root/usr/lib/rudolf/frames/_metadata`; `iso/.gitignore` gets
`sys/root/usr/lib/rudolf/frames/_metadata/`.

**The base layer (items 3-6).** The release build copies `sys/root` over the previous base layer, so files that
leave the repository, and programs the full build used to copy, stay in the base layer until removed explicitly.
`ci_release.sh` gets a list of paths to remove from the base layer (`systools/base-remove.txt`):
```
usr/lib/rudolf/frames/_metadata
usr/bin/ostree
usr/bin/dbus-send
usr/bin/dbus-run-session
usr/bin/pacat
usr/bin/paplay
usr/bin/parecord
```
Each is removed only if present; the list is part of `BSRC`, so a change to it rebuilds the base layer.
`mkrootfs.sh` (full builds) stops copying the same programs: `dbus-send` and `dbus-run-session` from its D-Bus list,
`pacat` and its links from the sound server's, and `ostree` from Flatpak's.

**A check that keeps it so** (`systools/deadcheck.py`, run by `ci_release.sh` before anything is built, and as a host
test):
- every top-level function and arrow function in `desktop/*.js` is named somewhere else in `desktop/` (its own
  module, another one, or `build19.py`'s patches);
- every C program in `sys/` compiles with `-Wunused-function -Werror=unused-function`;
- every file in `sys/root` is tracked by git (no build output);
- every program in `systools/base-remove.txt` is absent from `mkrootfs.sh`'s copy lists.
It fails the release build with the name of what is unused, like the duplicate-function check `build19.py` has
had since 1.4.0.

## Tests

| Test | What it shows |
|---|---|
| host: `deadcheck.py` | passes on 1.4.4; fails on 1.4.3 (names `sysuMB`, `tmE` and `_ruleset1`) |
| host: devlink / layout / bootfiles | unchanged (41/41, 7/7, 12/12) |
| `dl140_t.js` (Settings page) | unchanged |
| new `t144_t.py` | on the 1.4.4 test disc: the six programs and the cache are gone from the system; the web frames extension loads (Chromium writes its own `_metadata` under `/run/rudolf/frames`); `flatpak --version`, `flatpak remote-ls` from a local test repository with a signed tiny app, `flatpak install` and `flatpak run` of it; the sound server starts and `pactl info` answers; the D-Bus session starts |
| t125 (Wi-Fi, apps, Luau) | unchanged: Flatpak apps install and run |
| t141, t142, t143, t140, t131, t130, t128b, sb_t, v120 | unchanged |
| The Vivobook (after release) | Task Manager › System: the services as before; an installed Flatpak app (Oolite, after 1.4.3) still starts; sound plays; Settings › Update and recovery works |

## Not removed now (fallback paths kept on purpose)

Older paths that only run in rare situations. Retiring them changes what happens there, so they stay until the
conditions below hold; each is noted in the code where it starts.
- **The separate browser window for sign-in sites and Oracle's site frames** (`desktop/web25.js`,
  `desktop/signin26.js`, the web frames extension): since 1.2, Oracle uses real tabs; these run only when the desktop
  starts without its shell (the session's fallback). Retire when the fallback desktop is retired.
- **Start-up code for boot files from before 1.3** (`initramfs/init`: the first-found data partition, the
  slot-complete scan, 1.4.2's build check): it is what lets old boot files and old sticks hand over to newer systems.
  Retire when no supported install can still have boot files older than 1.3 (Get new boot files replaces them).
- **The software display (Xvfb)**: the display when there is no usable graphics driver; not a leftover.

# Part 2: the volume and brightness keys

On the ASUS Vivobook Go, F1 (mute), F2 and F3 (volume down and up), F4 and F5 (brightness down and up) do nothing.
Found through the development link (2026-10-30, 1.4.2):
- **The keys never arrive.** Recording every input device for 40 seconds while they were pressed showed no key
  events at all. On ASUS laptops these keys are not sent by the keyboard but by the firmware (ACPI WMI events), and
  the kernel turns them into key presses only with the `asus-nb-wmi` driver, which Rudolf OS's kernel does not have
  (`kconfig.sh` builds none of the laptop platform drivers; only `mxm-wmi` is there by default). Other makers do the
  same through their own drivers: Lenovo (`ideapad-laptop`, `thinkpad_acpi`), HP (`hp-wmi`), Dell (`dell-wmi`), Acer
  (`acer-wmi`).
- **Nothing would act on them.** `rudolf-display` passes every key below 256 to the X server; the desktop has no
  handler for the volume keys (XF86AudioRaiseVolume…) or the brightness keys (XF86MonBrightnessUp…).
- **There is no system volume.** Settings › Volume Controls sets the desktop's own mix (inside the desktop page);
  apps with their own sound (Flatpak apps such as Oolite, Linux apps) play through the sound server and do not follow
  it. The sound server's output on the Vivobook: `alsa_output.card1.analog-stereo`, at 100 %.
- **Brightness can be set already**: rudolfd's `brightness?value=` writes `/sys/class/backlight/amdgpu_bl1`
  (19005 of 65535, 29 %); only Settings › Display Properties uses it.

## Items

| # | What | Where |
|---|---|---|
| 7 | The laptop hotkey drivers, as modules loaded by modalias at start-up (as the graphics and Wi-Fi drivers are): `ASUS_WMI ASUS_NB_WMI IDEAPAD_LAPTOP THINKPAD_ACPI HP_WMI DELL_WMI ACER_WMI` (with what they need: `ACPI_WMI`, `INPUT_SPARSEKMAP`, `LEDS_CLASS`, `HOTPLUG_PCI`, `ACPI_PLATFORM_PROFILE`). `kconfig.sh` checks after `olddefconfig` that each one ended up as a module, so a missing dependency fails the build instead of dropping the driver without a word. The kernel's command line gets `video.brightness_switch_enabled=0`: otherwise the ACPI video driver also changes the brightness by itself on some laptops, and every press would count twice. | `iso/kconfig.sh` |
| 8 | `/usr/lib/rudolf/hotkey` (new): `hotkey volume up|down|mute`, `hotkey mic mute`, `hotkey brightness up|down`, `hotkey volume get`, `hotkey volume set N`. Volume: the sound server's default output (`pactl` as the desktop user), in 5 % steps, 0-100 % (never above 100 %); up or down also unmutes. Brightness: the first backlight in `/sys/class/backlight`, in 5 % steps of its range, never below 5 % (as rudolfd's `brightness` does). Each change writes the new level to `/run/rudolf/events` (`osd volume 45`, `osd volume muted`, `osd brightness 60`, `osd brightness none` when the screen has no adjustable backlight). | `iso/sys/root/usr/lib/rudolf/hotkey` |
| 9 | `rudolf-display`: `KEY_MUTE`, `KEY_VOLUMEDOWN`, `KEY_VOLUMEUP`, `KEY_MICMUTE`, `KEY_BRIGHTNESSDOWN`, `KEY_BRIGHTNESSUP` from any device run `hotkey` (in the background, never waiting for it) and are not passed to the X server, so they work the same whether the desktop, a game or a Linux app is in front, and nothing acts on them twice. Held down, they repeat where the device repeats them (a keyboard does; hotkey drivers send single presses). | `iso/sys/rudolf-display.c` |
| 10 | rudolfd: `GET volume` (level and mute, through `hotkey volume get`) and `POST volume?set=N|mute=0/1`; `GET events/wait` waits (up to 25 s) until `/run/rudolf/events` has something and returns it at once, so the desktop hears of a key press within a moment rather than at its next 2.5-second check. | `iso/sys/rudolfd.c` |
| 11 | The desktop: a volume or brightness indicator (icon, bar and percentage, near the top of the screen, for 1.5 s) on each `osd` event; the taskbar's volume icon shows the system volume and mute; Settings › Volume Controls gets **System volume** (a slider and Mute, the same as the keys), above the desktop's own mix, which stays for per-app levels; Settings › Display Properties' brightness slider follows the keys. In the browser version of Rudolf OS (no rudolfd) all of this is as before. | `desktop/` (new module `osd31.js`; `settings25.js`, `shell25.js`) |

The indicator shows on the desktop. While a game covers the screen, the keys still work, but it is not shown over
the game.

## Tests

| Test | What it shows |
|---|---|
| host: `hotkey` with a stand-in `pactl` and a stand-in backlight folder | the steps, the limits (0 %, 100 %, never below 5 % brightness), unmute on up and down, the `osd` lines, "no backlight" |
| new `t144k_t.py` (QEMU) | the volume keys sent as real key presses (QEMU's `volumeup`, `volumedown`, `audiomute`, through the PS/2 keyboard, as a laptop's keyboard or hotkey driver sends them): the sound server's volume and mute change; the indicator appears in the desktop; the keys do not reach X (a test X client sees none). The brightness keys from a test input device (uinput, as `asus-nb-wmi` creates one): with QEMU's screen, which has no backlight, the indicator says so and nothing fails. Settings › Volume Controls › System volume moves with the keys and sets the same level. The kernel has `asus-nb-wmi` and the other drivers as modules |
| The Vivobook (after release) | Task Manager › Hardware lists "Asus WMI hotkeys"; F1 mutes and unmutes, F2/F3 change the volume of the desktop and of a Flatpak app (Oolite) alike, F4/F5 change the brightness, each with the indicator |

# Part 3: the network indicator

On the Vivobook, the taskbar's network icon says **"Ethernet — connected"**, with a wired icon, while the laptop
is on Wi-Fi; it has no Ethernet port at all. Found in the code (2026-10-30):
- **The indicator never looks at the real network.** `netUpd()` (desktop, `base_v24.html`) draws it from the
  browser version's simulated connections (`NETC`, saved as `bd95net`), whose default is one wired connection named
  "Ethernet", active. On installed Rudolf OS nothing replaces that, so the icon shows the simulated "Ethernet" as
  connected whenever the browser is online, through whatever network.
- **The simulation is on Settings › Network Settings too.** "Connections" and "Add a connection" (name, type, SSID,
  security, password, addresses) are the browser version's simulation; on installed Rudolf OS they change nothing,
  next to the real Wi-Fi and Manual address sections.
- **`sit0` is listed as a wired adapter.** It is the kernel's IPv6-in-IPv4 tunnel device (`IPV6_SIT`, on in the
  default configuration), not hardware; Task Manager shows "sit0 (wired)", and when Wi-Fi was missing, Network
  Settings showed it as the only network hardware.

## Items

| # | What | Where |
|---|---|---|
| 12 | rudolfd `net`: each interface also says `"virtual"` (no `/sys/class/net/X/device`: tunnels, bridges, VPNs) and which one carries the default route (`"default":true`); `wifi` gives the network name (SSID) of a wireless interface that is connected | `iso/sys/rudolfd.c` |
| 13 | The taskbar indicator on installed Rudolf OS: from rudolfd's `net`, every 10 s, at once on `wifi connected`/`wifi disconnected` events (1.4.4's `events/wait`), and when the browser goes on- or offline. The interface with the default route decides: wireless, with its signal and "Wi-Fi — connected (SSID)"; wired, with "Ethernet — connected (eth0, 1000 Mb/s)"; none, "Not connected". The internet check (`generate_204`) stays as it is ("connected, no internet access detected"). The popup lists the real adapters. In the browser version all of this is as before | `desktop/` (new module `net31.js`, replacing `netUpd` on installed Rudolf OS) |
| 14 | Settings › Network Settings on installed Rudolf OS: "Connections" and "Add a connection" are hidden (the Wi-Fi and Manual address sections above them are the real ones); a line says so for anyone who remembers them. The browser version keeps them | `desktop/settings25.js` |
| 15 | Virtual interfaces are left out of Network hardware, Task Manager's adapters, the hardware check and the system report (rudolfd's `virtual`); and the kernel no longer has the IPv6-in-IPv4 tunnel (`n IPV6_SIT`), so `sit0` is not created at all (Rudolf OS has no use for it) | `desktop/settings25.js`, `tm26.js`, `hwcheck30.js`; `iso/kconfig.sh` |

## Tests

| Test | What it shows |
|---|---|
| `dl140_t.js`-style page test with a stand-in rudolfd (`net31_t.js`) | wireless with a default route: the Wi-Fi icon, "Wi-Fi — connected (Home)"; wired: the wired icon and "Ethernet"; none: "Not connected"; a `wifi disconnected` event changes the icon within a second; a virtual interface is never shown; Network Settings has no "Connections" on installed Rudolf OS and still has them in the browser version |
| t144k (QEMU, above) | its network card (e1000, wired, no Wi-Fi): the icon says wired, connected; no `sit0` in `/sys/class/net` |
| The Vivobook (after release) | the icon says Wi-Fi with the network's name; Task Manager › Hardware lists only `wlan0` |

# Part 4: the Settings layout

Every Settings page opened in the desktop with a stand-in rudolfd, and what each control does checked in the code
(2026-10-30). Two settings do nothing, two controls with the same name do different things, and several are on a
page where nobody would look for them. Saved values keep their keys, so nothing a person set is lost by a move.

| # | What | Now | Then |
|---|---|---|---|
| 16 | **Natural scrolling** and **Horizontal speed** | Accessibility › Pointer assistance › Scrolling: they save `S.natscroll` and `S.hscroll`, which nothing reads | removed; natural scrolling is Mouse and touchpad's (`S.ptr.natural`, which rudolf-display applies) |
| 17 | **Pointer speed**, twice | Mouse and touchpad (`S.ptr.speed`: the real pointer, through rudolf-display) and Accessibility › Mouse and pointer (`S.msens`: only games that capture the mouse inside the desktop, such as Quake) | one, in Mouse and touchpad, applied to both; `S.msens` follows it (a person's old Accessibility value is taken over once if they had changed it and not the other) |
| 18 | Primary button, double-click speed, scroll speed, single-click opening | Accessibility › Mouse and pointer | Mouse and touchpad (cursor size and colour, pointer helpers, mouse keys and dwell click stay in Accessibility) |
| 19 | Language | Accessibility › Language and region | System Settings › **Date, time and language** (System Settings keeps its name: it also gets **Power and battery**, part 6) |
| 20 | Set the clock from the internet | Network Settings › Clock | System Settings › Date, time and language › Clock |
| 21 | Web search (Oracle), Downloads | Network Settings | the Oracle page, renamed from "Oracle sign-in" to **Oracle** (real tabs, sign-in sites, web search, downloads) |
| 22 | Screen saver, Sleep | User Settings › Sign-in options | Display Properties › **Screen saver** (part 5), with the password after the screen saver; **Sleep** goes to System Settings › Power and battery (part 6); Lock stays in Sign-in options |
| 23 | Cyberdeck test mode; Update from a file and its Roll back (the desktop page) | Display Properties; Update and recovery › Update | About › **For developers** (next to "Roll back system…" for real updates, a second roll-back on the same page was confusing) |
| 24 | Speech and hearing | Accessibility: "Hearing and speech" and "Hearing and voice", captions in three places | **Vision** (screen reader, text-to-speech voice, magnifier, motion, reading), **Hearing** (visual sound alerts, live captions, caption style), **Voice control** |
| 25 | Taskbar | Display Properties: in Appearance and again in Colours and theme | one Taskbar group |
| 26 | Backups | Storage: "Back up everything", "Back up to a USB stick", "Make a USB stick… also copy my files" apart | one **Backup** group, each with a line saying when to use it |
| 27 | Master volume | Volume Controls (the desktop's own mix) | renamed **Desktop sounds**, under part 2's System volume |

Settings' search finds each control under its new page. The page test (`settings144_t.js`, with a stand-in
rudolfd) opens every page and checks the list of controls against the table above, that each moved control still
changes the same saved value, that Pointer speed reaches both rudolfd's `pointer` and the in-page mouse capture, and
that item 16's two settings are gone.

# Part 5: the screen saver

Today: one screen saver (a starfield with the time), set only by Settings › User Settings › Screen saver (Off, 2, 5,
10, 15 or 30 minutes), drawn on a page canvas, which Chromium draws with the graphics card. 1.4.4 gives it its own
section in **Display Properties › Screen saver**:

- **Screen saver**: On or Off (a saved "Off" or a time from before stays as it was).
- **Start after**: 1, 2, 3, 5, 10, 15, 20, 30, 45 or 60 minutes without input.
- **Style**: eight presets, each with a live preview in the page and **Preview** (full screen until a key or the
  pointer moves):
  1. **Starfield**: today's, flying through stars.
  2. **Clock**: a large clock and the date, drifting slowly so it never stays in one place.
  3. **Rudolf**: the Rudolf OS logo bouncing off the screen edges, changing colour at each bounce.
  4. **Code rain**: falling columns of characters.
  5. **Mystify**: two outlines of joined lines that bounce and change colour, leaving trails.
  6. **Bubbles**: soft bubbles rising and drifting.
  7. **Aurora**: slow bands of colour waving across a night sky.
  8. **Fireworks**: rockets bursting into falling sparks.
- **Show the time**: over any style except Clock (on by default).
- **Ask for the password when it ends** (from User Settings; Lock stays there).

**Drawn by the processor.** Each style draws into a canvas made with `getContext('2d', {willReadFrequently:
true})`, which Chromium keeps in memory and draws with the processor (Skia's software renderer) instead of on the
graphics card; no WebGL, no CSS animations or filters. To keep the processor's load small: at most 30 frames a
second, at most 1280 pixels wide (larger screens show it scaled up), no drawing while it is hidden. Only the
finished picture goes to the screen, as everything on the desktop does. The page test checks that every style
makes its canvas this way (and none other), and measures its frame time in a CPU-only Chromium.

**When it starts.** As now, not while something is full screen, the mouse is captured or media plays. On installed
Rudolf OS the time without input comes from rudolf-display, which sees every key and pointer movement, also in
games and Linux apps (the desktop page sees only its own), so it never starts over a game being played; rudolfd's
`idle` gives the milliseconds since the last input. In the browser version, as now.

Tests: the page test runs each preset for two seconds (frames drawn, the canvas is a software one, nothing thrown),
the Start after times (with a stand-in clock), On/Off, Preview, the password after it, and the old saved value
carried over; t144k checks rudolfd's `idle` against input sent to QEMU. On the Vivobook: each style, and that it
does not start during Oolite.

# Part 6: the taskbar's clock, quick settings and battery; power saving

Checked in the code (2026-10-30):
- **The calendar shows another time than the taskbar.** The taskbar clock (`clk2`, `base_v24.html`) uses the time
  zone set in System Settings (`S.tz`) and the 24-hour setting (`S.h24`). The calendar that opens from it
  (`shell25.js`) formats the time and date with `toLocaleTimeString([], …)`, without either: Chromium's own zone and
  format. With a time zone set, the two differ by hours (and the calendar may mark the wrong day as today); with
  24-hour on, they differ in format. The lock screen and the screen saver's clock do the same as the calendar.
- **Quick settings (the ☰ panel) has a volume and a brightness slider** besides the toggles, and the battery as a
  line of text at the bottom. The taskbar already has its own volume control (the 🔊 icon and its popup); brightness
  has none.
- **There is no power management** beyond "Sleep after" (in User Settings): no power modes, nothing for the lid,
  the screen is never turned off, no battery saver. Task Manager on the Vivobook lists two batteries: the laptop's
  (45 %, charging) and a second one at -1 % (a device that reports a battery it does not have).

## Items

| # | What | Where |
|---|---|---|
| 28 | **One clock.** A shared formatter (`clockParts(d)`: time, date, weekday and today's date in the set time zone, 12- or 24-hour, the date format of System Settings) used by the taskbar clock (digital and analog), the calendar (its time, date and which day is today), the lock screen, the screen saver's clock (part 5) and the notification times | `desktop/` (new module `clock31.js`; `shell25.js`, `core25.js`) |
| 29 | **Quick settings** without the volume and brightness sliders and the battery line: its toggles, the network line (part 3), Lock, Settings and Power | `desktop/shell25.js` |
| 30 | **Brightness on the taskbar**: a ☀ icon next to the volume icon, its brightness shown in the icon, with a popup like the volume one (a slider, 5-100 %, and the percentage); the mouse wheel over either icon changes it by 5 %. Shown only on installed Rudolf OS when the screen has an adjustable backlight (rudolfd `info.backlight`); it follows the brightness keys (part 2) | `desktop/` (`osd31.js` with part 2) |
| 31 | **The battery on the taskbar**, next to the workspace buttons: an icon filled to the level (green; amber at 20 % or less; red at 10 % or less), a lightning bolt while charging, the percentage beside it; its tooltip says the state and the time left or to full (from the battery's energy and power, when it reports them); a click opens System Settings › Power and battery. Only the computer's own battery: of rudolfd's `stats.battery`, the ones of type Battery with scope System and a level of 0-100 (the Vivobook's -1 % device battery is left out, also in Task Manager). Updated every 30 s and at once when the charger is plugged in or out (rudolfd watches the power supply and writes `power ac`/`power battery` to the events). No battery: no icon. In the browser version, from `navigator.getBattery()` as quick settings did | `desktop/`; `iso/sys/rudolfd.c` |
| 32 | **System Settings › Power and battery** (installed Rudolf OS; what the computer cannot do is shown greyed out with the reason): | `desktop/` (new module `power31.js`); `iso/sys/rudolfd.c` (`power/*`), `iso/sys/rudolf-display.c` |

Power and battery:
- **Battery**: the level, charging or not, the time left or to full, its health (capacity now against new, from
  `energy_full`/`energy_full_design`) and charge cycles, when the battery reports them.
- **Power mode**: Power saver, Balanced (the default) or Performance. It sets the firmware's platform profile
  (`/sys/firmware/acpi/platform_profile`: low-power, balanced, performance; ASUS laptops have it through
  `asus-nb-wmi`, part 2) and the processors' energy preference (`energy_performance_preference` of `amd-pstate` or
  `intel_pstate`: power, balance_performance, performance), whichever the computer has. Separate choices on battery
  and when plugged in.
- **Battery saver**: on now, or on by itself at 10, 20 or 30 % (or never); off by itself when the charger is
  plugged in. While it is on: Power saver mode, the screen 30 % dimmer (not below 10 %), the desktop's animations
  and transparency reduced, the screen saver at 10 frames a second, update checks and the app catalogue refresh
  waiting until it is off, and Wi-Fi power saving on (`iw dev wlan0 set power_save on`). The battery icon shows a
  leaf while it is on.
- **Turn the screen off after**: Never, 1, 2, 5, 10, 15 or 30 minutes without input, on battery and plugged in
  (rudolf-display: DPMS off on Xorg, the framebuffer blanked on the software display; any input turns it back on).
- **Sleep after** (from User Settings): Never, 5, 10, 15, 30, 60 minutes, on battery and plugged in.
- **When the lid is closed**: Sleep, Turn the screen off, or Do nothing, on battery and plugged in (rudolf-display
  reads the lid switch, which it ignores today, and writes `lid closed`/`lid open` to the events; rudolfd acts).
- **When the power button is pressed**: Ask (the Shut down dialog, as now), Sleep, or Shut down.
- **Battery care** (where the firmware supports it: `charge_control_end_threshold`, ASUS and others): stop charging
  at 60, 80 (recommended when the laptop is mostly plugged in) or 100 %.
- Each choice is kept on RUDOLFDATA (`/persist/etc/power.conf`) and applied by rudolfd at every start and whenever
  the charger is plugged in or out, so it holds without anyone signed in.

## Tests

| Test | What it shows |
|---|---|
| page test (`taskbar144_t.js`, a stand-in rudolfd) | with a time zone 9 hours from the browser's and 24-hour on and off: the taskbar, the calendar, the lock screen and the screen saver show the same time and date, and the calendar marks the right day; quick settings has no volume, brightness or battery; the brightness icon and popup (only with a backlight; the wheel; the keys' events move it); the battery icon at 45 % charging, 15 % (amber), 8 % (red), no battery (no icon), the -1 % device battery ignored, plug and unplug events; Power and battery's choices sent to rudolfd |
| host: rudolfd's `power/*` against a stand-in `/sys` (platform profile, energy preference, battery, charge threshold, backlight) | each mode writes what it should; what the computer lacks is reported, not failed; battery saver on and off at the level and on plugging in; the settings file kept and applied again at start |
| t144k (QEMU) | QEMU's machine has no battery or lid: `power/status` says so and no battery icon shows; the power button through QEMU's ACPI power button (`system_powerdown`): Ask shows the Shut down dialog, Sleep sleeps the machine and a key wakes it; the screen off after 1 minute (rudolf-display's DPMS state) and on again at a key; the power mode where the machine has no platform profile shows greyed out with the reason |
| The Vivobook (after release) | the battery icon (45 %, charging) and no -1 % one; the three power modes (platform profile and energy preference read back); battery saver at 20 % (on battery); the screen off after a minute; closing the lid sleeps and opening it wakes; charging stops at 80 % with Battery care on; the taskbar and the calendar show the same time |

# Part 7: the clock sets itself

Found on the Vivobook (2026-10-30, 1.4.2): its clock had gone back to 2016-01-01 (the hardware clock reset, most
likely when the battery ran flat). Every HTTPS connection then failed ("SSL peer certificate or SSH remote key was not
OK", since no certificate is valid in 2016): updates could not be checked and the development link could not switch
on. Rudolf OS sets the clock from the internet only when someone presses Settings › Network Settings › Clock › Set
the clock from the internet (rudolfd `time/sync`, BusyBox `ntpd`, which needs no certificates); it did, and all
worked again.

| # | What | Where |
|---|---|---|
| 33 | **Never earlier than the release.** At start, before anything uses the network, `rudolf-init` sets the clock to the running release's build date when it reads earlier than that (the date is in `/usr/share/rudolf/VERSION`'s release; a clock cannot honestly be older than the system it runs). | `iso/sys/root/sbin/rudolf-init` |
| 34 | **From the internet by itself.** rudolfd sets the clock with `ntpd` (pool.ntp.org, then time.google.com) once the network is up after a start, at each `wifi connected` and when a wired link comes up, and every 12 hours; the result goes to the hardware clock (`hwclock -w -u`). An HTTPS failure about a certificate while the clock is more than a day off what the time servers say is retried after the sync. | `iso/sys/rudolfd.c` |
| 35 | **The button stays**, in System Settings › Date, time and language › Clock (part 4), with the last automatic sync shown next to it ("set from the internet at 18:59"). | `desktop/` |

Tests: host, `rudolf-init`'s floor with a stand-in clock (earlier: set to the release date; later: left alone);
t144k (QEMU) starts with the machine's clock set to 2016 (`-rtc base=2016-01-01`): after the start the clock is at
least the release date; QEMU has no internet, so the time servers do not answer and that is logged, not failed. On
the Vivobook: the clock after a start with the hardware clock wrong (set back in the BIOS), and the update check
right after.

## Release

Branch `rudolf-1.4.4`, after 1.4.3 is out. Version 1.4.4. The kernel is rebuilt (item 7), so the release carries a
new kernel and modules. The changes list: "New: the volume and brightness keys work on laptops (ASUS, Lenovo, HP,
Dell, Acer), with an on-screen indicator, and Settings › Volume Controls has the system volume. Fixed: the taskbar
said Ethernet was connected on computers using Wi-Fi. New: eight screen savers, drawn by the processor, set in
Display Properties › Screen saver. Changed: Settings is reorganised (each page and what moved). Fixed: the
calendar showed another time than the taskbar. Fixed: the clock now sets itself from the internet, and never falls back before the release (a reset clock
broke updates). New: the battery and brightness on the taskbar; power modes, battery
saver, screen off, the lid and the power button, and battery care in System Settings › Power and battery. Removed unused
programs and code." A beta, the checks above, promote; no disc needed.
