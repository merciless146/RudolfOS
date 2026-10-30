# Rudolf OS 1.4.4: cleanup, the volume and brightness keys, and the network indicator (plan)

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

## Release

Branch `rudolf-1.4.4`, after 1.4.3 is out. Version 1.4.4. The kernel is rebuilt (item 7), so the release carries a
new kernel and modules. The changes list: "New: the volume and brightness keys work on laptops (ASUS, Lenovo, HP,
Dell, Acer), with an on-screen indicator, and Settings › Volume Controls has the system volume. Fixed: the taskbar
said Ethernet was connected on computers using Wi-Fi. Removed unused programs and code." A beta, the checks above, promote; no disc needed.
