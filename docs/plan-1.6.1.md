# Rudolf OS 1.6.1: what was moved out of 1.6.0

Asked for on 2026-11-15: "What items have been pushed beyond 1.6.0? Can they all be added to a plan for 1.6.1?" This
plan gathers every item that docs/plan-1.6.0.md moved past 1.6.0, with where it came from. Nothing here is built yet.
The owner's rule for the native desktop holds for every part: "Rudolf OS on the native desktop should appear and
function exactly as it does in Chromium mode."

## Parts

### 1. Several monitors, on the three workspaces (from 1.6.0 Part 12, item 7)
Decided by the owner on 2026-11-15: "Rudolf OS has three workspaces that can be selected in the bottom right of the
taskbar. These workspaces will be the basis for Rudolf being able to support up to three monitors/displays, as well
as having three panels for the mobile build that can be selected by swiping left/right similar to how Android phones
work." Built after the 1.6.0 beta.
- **Up to three displays,** each showing one of the three workspaces, with its own wallpaper and taskbar.
- **A window moved to another workspace moves to that display;** the taskbar on each display lists that workspace's
  windows (as the taskbar lists the shown workspace's windows today).
- **One display: as today.** The three workspaces are switched from the taskbar, Super+Ctrl+arrows and the menus.
- **Standard and Native alike:** rudolf-display gives Chromium one output today, so Standard needs the change too
  (decided by the owner, so the rule holds); Native follows RandR's outputs.
- **Plugged in and out:** a display unplugged takes its workspace back onto the remaining ones, with its windows; Settings
  › Display lists the displays and which workspace each shows.
- **Tests:** Xvfb with several screens (or Xephyr / RandR's virtual outputs) for both engines, compared with same_t.sh;
  on the Vivobook with an HDMI monitor (owner).

### 2. The mobile build's three panels (from the same decision)
The phone's home screen is the three workspaces as three panels, swiped left and right as on Android. It belongs with
the first phone (Part 4); the workspaces' code from Part 1 is written so a panel is a workspace shown full screen.

### 3. Lite, refined as a compatibility option (from 1.6.0 Part 15)
Decided by the owner on 2026-11-15: Native replaces Lite as the desktop; Lite stays in Settings › System health ›
Desktop engine as a compatibility option, "to be worked on and refined later". Left from 1.5.8 and 1.5.9:
- the desktop's menus and windows behind Oracle's tab pages (the covered tab's still picture while a menu is open);
- stutter in Oracle's tabs;
- Google sign-in for GitHub (symptoms to be described first);
- GPU drawing for Lite measured;
- Oracle's WebKit tabs: YouTube, Google sign-in with FedCM, a download, full screen, memory with 5 tabs;
- the lock and sign-in screen settings and the shutting-down screen;
- the dynamic backgrounds' moving parts.

### 4. The first phone, voice calls and emergency calls (from 1.6.0 "Not in 1.6.0", and docs/plan-1.5.8.md 8.3)
- **One well-supported phone first** (docs/plan-1.5.8.md 8.3: a PinePhone Pro or a OnePlus 6/6T), with ARM64 from
  1.6.0 Part 7.
- **Voice calls** through the modem, and **emergency calls**, which must be tested on each device before they are
  offered.
- The phone's home screen: Part 2.
- Nothing on a phone is flashed or unlocked without the owner's decision.

This is the largest part here. Owner's call: keep it in 1.6.1, or give it a release of its own.

### 5. eSIM and 5G (from 1.6.0 "Not in 1.6.0")
- eSIM provisioning (downloading a carrier's eSIM profile);
- 5G standalone features beyond what the modem does by itself.

1.6.0 left these without a release; they are here so they are not lost. They need hardware with an eSIM and a 5G
modem.

## Not moved here
- **elogind and polkit** (1.6.0 Part 11): left out by decision on 2026-11-15, since Rudolf OS's own inhibit portal and
  prompts cover what apps ask. Taken up again only if an app or a later part needs them.
- **Native as the default for new installations** (#384): still 1.6.0's, for the owner's decision after the week of
  betas.
- **The apps moved to native code:** docs/plan-2.0.md.

## Steps
1. Part 1, several monitors, after the 1.6.0 beta.
2. Part 3, Lite's leftovers, one PR each.
3. Parts 2 and 4 with the first phone; Part 5 when the hardware is there.
