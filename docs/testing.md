# Testing rules for Rudolf OS

Written on 2026-11-10 (plan 1.5.8, item 5.4), after the Lite desktop engine passed every check and then closed as it
started on the first real computer it reached.

## Anything that changes how the computer or the desktop starts

This covers the start-up (initramfs/init), rudolf-init, the session, the desktop engine, the shell, the display
(Xorg, Xvfb, rudolf-display), system updates and the boot files.

1. **The fallback comes first.** Before the change itself reaches a computer, there is a way back that needs no
   working desktop, and it is tested: a failing desktop engine gives way to the standard one (qt/host/deskfall57_t.sh),
   a system slot that cannot start gives way to the previous one (qt/host/startfall57_t.sh), Recovery mode starts the
   installed system with safe settings, and Ctrl+Alt+Backspace starts the safe desktop (qt/host/desksafe58_t.sh).
2. **Test it from the system itself, not from the build machine.** A program run on the release machine finds that
   machine's libraries. Run it from the built layer (a chroot of base.sfs, as webkit-check.yml does), so a library it
   opens while running (dlopen: graphics, sound, plug-ins) and the layer lacks fails the check. ldd does not see those.
3. **Test it on the real graphics path.** Xvfb and LIBGL_ALWAYS_SOFTWARE hide what the GPU path does. Before such a
   change reaches the test laptop, it runs once on Xorg with a GPU (QEMU with virtio-gpu, or the laptop over the
   devlink by hand for a few seconds, without making it the default: `rudolf-webkit` was run this way for 15 s and
   showed "Couldn't open libGLESv2.so.2" at once).
4. **The test laptop last, with a way to read what happened.** The devlink stays on across restarts, and each start's
   logs are kept (var/log/starts, System health › Last starts), so a start that never reaches the desktop can be
   read at the next one.

## Everything else

- Host tests (qt/host/*_t.sh) for scripts and C helpers, with the paths they use moved into a temporary folder.
- Browser tests (qt/shell2/*_t.js) for the desktop, each on its own port.
- QEMU only where it is needed: installing, Encrypt my data, moving to a disk, updates and rollback, and the final
  set before a release is promoted.
- `python3 systools/deadcheck.py` (in iso/) before every pull request.
