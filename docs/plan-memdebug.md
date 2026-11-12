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

## What the first reading found (2026-11-12, released 1.5.8, 2026111219)
| Reading | In use | Unaccounted | Graphics pool (TTM) | Graphics in system memory (GTT) |
|---|---|---|---|---|
| idle | 1,074 MB | 198 MB | 21 MB | 17 MB |
| a video full screen | 1,906 MB | 245 MB | 27 MB | 38 MB |
| two minutes after closing Oracle | 1,228 MB | 206 MB | 46 MB | 19 MB |

- Graphics buffers are small (the X server about 27 MB, Chromium's GPU process 8 to 23 MB, mostly in the 256 MB of the
  graphics card's own memory); no shared buffers (dma-buf) at all.
- The graphics pool grows a little with use (21 → 46 MB) and its limit is 1.7 GB: a cap (64 MB) goes into 1.5.9.
- **140 to 180 MB is neither graphics nor any counter**: memory taken page by page by the kernel or a driver, there
  from the start and steady. This kernel has nothing that names it (no /proc/allocinfo, no page_owner).

## Part 2: a test kernel that names it
The workflow *Test kernel* (iso/systools/testkernel.sh) builds the stable release's kernel with allocation profiling
(MEM_ALLOC_PROFILING, on from the start) and its own modules layer, as the pre-release `test-allocinfo`. No update
ever installs it. On the Vivobook, with Secure Boot off again (the same switch):
1. The devlink brings the three files (put), and a temporary system slot `r` is made on the drive: the test kernel
   and its modules, and the base, Chromium and desktop layers of the running slot (hard links, no extra room).
2. The running system hands over to the test kernel (sync, file systems read-only, kexec with `rudolf.slot=r`):
   the same start as an update's, without the firmware. The modules layer is not the release's, so it starts as
   "unchecked", which only a start with Secure Boot off allows.
3. Five minutes idle, then `/proc/allocinfo` is read (read-only): every place in the kernel with how much it holds,
   the largest first, along with the counters above.
4. A normal restart (the released kernel again), the slot `r` removed, and Secure Boot back on and checked. Nothing
   else on the drive changes; if the test kernel does not start, the power button and a normal start bring the
   released system back.

## What the test kernel found (2026-11-12, idle, 204 MB unaccounted)
`/proc/allocinfo` on the Vivobook, five minutes idle (the test kernel through slot `r`, Secure Boot off):

| Held by | MB | What it is |
|---|---|---|
| `drivers/iommu/dma-iommu.c` (`__iommu_dma_alloc_pages`) | 65 | the SSD's host memory buffer: the Kingston OM8PGP4128P has no DRAM of its own and borrows 64 MiB (`nvme0: allocated 64 MiB host memory buffer`) |
| `ttm_pool_alloc_page` [ttm] | 42 | the graphics driver's pool and the graphics card's system memory |
| `net/core/skbuff.c` (`kmalloc_reserve`) | 16 | network (Wi-Fi) receive buffers |
| `blk-mq` requests, `dm-bufio` | 9 | the drive's requests; dm-verity's cache of checked blocks |
| IOMMU tables, smaller | 5 | |

About 135 of the 204 MB is named; the other 60 to 70 MB was taken before tracking could see it (early start-up).
- **Nothing is a leak and nothing is wasted by Rudolf OS.** The largest part is the hardware of a budget laptop: an
  SSD without DRAM borrows RAM, and it is faster for it. Limiting it (`nvme.max_host_mem_size_mb`) would trade the
  drive's speed for 64 MB: left as it is.
- The graphics pool cap stays in 1.5.9 (Part 10); the network and drive buffers are normal and small.
- The memory to win is the desktop page's (Part 10), not hidden memory.

After the reading: a normal restart into the released kernel, slot `r` and the test files removed, Secure Boot back
on and checked (lockdown `[integrity]`, 2026111219 from slot b). The pre-release `test-allocinfo` stays for a later
measurement; it is never in a channel.
- The test kernel's screen was garbled: handing over by kexec from a running desktop leaves the graphics card set up
  by the first kernel, which amdgpu does not fully reset. A test of this kind reads over the devlink only; updates
  hand over before any graphics driver has started, so they are not affected.
