# Finding the hidden memory: one start without lockdown (planned 2026-11-12)

Why: on the Vivobook (3.4 GB, AMD 0x1506), Standard at idle uses about 1.0 to 1.2 GB, and about 200 to 250 MB of it
belongs to no process (docs/plan-1.5.9.md Part 10). On budget and old computers that is a large share. The only place
that says what it is, is the kernel's debug files (debugfs): the graphics driver's page pool, every graphics buffer
with the process that owns it, and the buffers shared between processes. With Secure Boot on, the start-up locks the
kernel down (integrity), and lockdown keeps debugfs closed. So the reading needs **one start with Secure Boot off**,
then Secure Boot back on. Nothing in Rudolf OS changes for it, and no release ever ships without lockdown.

## What is safe about it, and what is given up for that start
- Turning Secure Boot off in the firmware keeps the Rudolf OS key: it is in shim's key list (MOK), not the firmware's
  own keys. Rudolf OS starts as before (docs/secure-boot.md: "With Secure Boot off everything works as before, and
  the layers are still checked").
- For that one start: unsigned kernel modules and kernels could load, and kernel memory could be read or written by
  root. Nothing does that in Rudolf OS, but the start should do nothing else: no browsing, no downloads, no USB
  sticks, no other accounts. The devlink stays on (it is how the reading comes back); it is used only for the
  script below, which reads.
- **Never** choose "Reset to setup mode", "Clear Secure Boot keys" or "Restore factory keys" in the firmware: that
  could remove the keys the firmware checks shim with. Only the one switch, Secure Boot on/off.
- The test-only start options (RUDOLF_UKI_TEST_CMDLINE and the test keys) are not used: the release's own signed
  start, with Secure Boot off, is all that is needed.

## Steps
Each step needs the person at the Vivobook; Claude only reads over the devlink and changes nothing.

1. **Before (Secure Boot on):** the final 1.5.8 installed, Standard, signed in, nothing open, five minutes idle.
   The usual read (memgap.sh) for the baseline: the in-use figure, the processes, the gap.
2. **Secure Boot off:** restart into the firmware (on Asus laptops usually F2 at the logo, then Advanced mode › Security ›
   Secure Boot › Secure Boot Control: Disabled), save and restart. Sign in, open nothing, five minutes idle.
3. **Check the start:** lockdown reads `[none]` and the gap is about the same as in step 1 (the same start, only
   unlocked). If the gap is very different, the comparison is off: stop and note it.
4. **Read, three times** (memdbg.sh, read-only: it mounts debugfs read-only, reads, unmounts):
   - `idle`: as it is;
   - `video`: a YouTube video full screen in Oracle for two minutes, then the reading while it plays;
   - `closed`: the video's tab closed, two minutes, then the last reading (what stays after use).
   The script refuses to read anything if lockdown is on.
5. **Secure Boot back on, the same switch:** restart into the firmware, Secure Boot Control: Enabled, save, restart.
6. **Check it is back:** lockdown reads `[integrity]`, `/etc/rudolf/secureboot` is 1, and Task Manager › System
   and Settings › Security say Secure Boot is on. If the computer does not start with it on, the key was lost:
   enroll it again (MokManager, docs/secure-boot.md), never leave it off.

The whole start without lockdown takes about 20 minutes.

## What is read
- **TTM page pool** (`ttm/page_pool`): pages the graphics driver keeps for reuse after buffers are freed. Counted as
  used, owned by no process.
- **Graphics buffers by process** (`dri/*/amdgpu_gem_info`): every buffer, in VRAM (the 256 MB carve-out) or in GTT
  (system memory the graphics card uses), with its owner: Chromium's GPU process, the compositor (cage), Xwayland.
- **Memory managers** (`amdgpu_vram_mm`, `amdgpu_gtt_mm`): how much of each is handed out.
- **Shared buffers** (`dma_buf/bufinfo`): buffers passed between processes (the screen's own, video frames).
- With them, as before: meminfo, slab, vmalloc by caller.

## What each answer leads to
| Where the gap is | What it means | What can be done (1.5.9 or later) |
|---|---|---|
| The TTM pool | freed graphics memory kept for reuse | cap the pool (`ttm.page_pool_size`) for small computers; measure the frame rate before and after |
| GTT buffers of Chromium's GPU process | the desktop page's layers and textures | Part 10 (a lighter page), fewer layers (the overlays only when shown), Chromium's GPU memory limits |
| GTT buffers of cage or Xwayland | the screen's buffers | fewer screen buffers (double rather than triple), Xwayland only when an X app runs |
| Shared video buffers | frames kept after the video | check they are freed when the tab closes (the `closed` reading) |
| Slab or vmalloc | the kernel itself | the driver or module named, and its options |

The findings go into docs/plan-1.5.9.md (Part 10, or a new part), with the readings.
