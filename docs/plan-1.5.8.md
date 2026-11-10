# Rudolf OS 1.5.8: diagnosing a computer that does not start, and smoother recovery

Asked for on 2026-11-10, after the Vivobook could not start the 1.5.7 beta with the Lite desktop engine. The safeguards
that stop such a failure from leaving a computer unable to start went into 1.5.7 (priorities 1 to 3 below); this plan
is the rest (priorities 4 and 5). It starts once 1.5.7 is promoted.

## What happened (2026-11-10)

1. Lite (WebKit) was chosen for the desktop. On the Vivobook's graphics it closed as it started, every 7 to 9 seconds;
   the session started it again without end, because its safety net only caught a desktop that hangs. The screen
   stayed grey and the computer never reached the sign-in screen. Lite had been tried on Xvfb and in Debian's own
   WebKit (cog), never on the Vivobook's real graphics.
2. Recovery mode skipped the installed system for the drive's own, and an installed computer's drive has boot files
   only: "this drive has boot files only (1.2.8)". The rescue stick at hand had boot files only too, and both drives
   showed the same message, so it took three tries to see which drive had started. Fast Boot also skipped the stick
   until it was chosen in the ASUS boot menu.
3. A full 1.5.6 stick (written with Rufus in DD mode from the Asus desktop) got through. Started in Recovery mode, it
   ran the stick's 1.5.6 with the Vivobook's 1.5.7 files: the desktop asked for the shell again and again and kept
   restarting. Its updater then put 1.5.7 in the spare slot taking the Chromium layer from the stick, and scheduled it;
   without the stick that slot could not start, and the start-up stopped at the same boot-files-only message (it went
   back to the good system at the following start only because the slot's tries had run out).
4. Through the devlink the Lite choice was removed, and the computer started 1.5.7 on the standard engine. The speaker
   chip then did not answer at start-up ("Codec #0 probe error", after the forced power-offs); a full shut-down fixed it.
5. During the failure, the devlink never answered while the screen was grey, and the session's and Lite's logs were
   in /run, lost at each restart.

## Done in 1.5.7 (priorities 1 to 3)

- **A desktop that cannot start is given up** (session: `desk_exited`, `webkit_trial_*`, `shell_switch_limit`;
  test qt/host/deskfall57_t.sh): Lite closing 3 times in a row as it starts, or not having answered before the
  computer was switched off, is turned off for good (Settings says when and why); the shell likewise until the next
  start; a desktop asking for the shell more than 3 times in 5 minutes is not obeyed.
- **A system that cannot be put together goes back to the previous one** (initramfs: a slot missing a layer is
  given up like a damaged one; test qt/host/startfall57_t.sh).
- **Recovery mode starts the installed system with safe settings** and changes nothing (initramfs; the session
  already leaves out Lite, the shell and the graphics driver in recovery). Needs 1.5.7 boot files.
- **Recovery and stick starts never change the installed system** (rudolf-update: no download in recovery, or from a
  disc that is not on RUDOLFDATA's drive; boot-ok does nothing in recovery; Settings says why; tests
  qt/host/updsafe57_t.sh and qt/shell2/updsafe57_t.js).
- **Updates are self-contained and checked complete** (rudolf-update: a layer shared with a removable disc is copied
  into the slot; a slot is only scheduled when every file is there at its listed size).

## Priority 4: diagnosing a computer whose desktop does not start

### 4.1 Start logs that outlast a restart
- Keep the session log, Lite's log (webkit-desktop.log), the start-up's choice of system (which slot, why, recovery or
  not) and the kernel's warnings of the last 4 starts in /persist/var/log/starts/ (rotated like rudolfd.log), written
  as they happen, not at shut-down.
- The initramfs writes its decisions (slot chosen, tries left, layer missing, slot given up) to RUDOLFDATA before
  kexec, so a start that never reaches rudolf-init still leaves a record.
- System health shows "Last starts" with the outcome of each; the self-test and Blitzen's diagnostics read them.

### 4.2 The devlink and remote access without the desktop
- Find why the devlink did not answer at the grey screen (it is turned on from the desktop; check whether rudolfd
  starts it at boot when its time is left, and whether it waits for the desktop or the network the desktop sets up).
- Start the devlink and remote access from rudolf-init when they were on (time left, or remote access allowed),
  independent of the desktop process, once the network is up.
- Remote access serves the X screen, so it shows a grey or broken desktop too: make sure it does not depend on the
  desktop page either.

### 4.3 Emergency keys in rudolf-display
- A key combination handled by rudolf-display itself (which reads the keyboard before X, as Ctrl+Alt+Del does) that
  switches the desktop back to the standard engine at once and restarts it: for example Ctrl+Alt+Shift+F12, with an
  on-screen line from rudolf-display saying it did.
- The same keys in Recovery mode do nothing (already safe).

## Priority 5: smoother recovery and fewer surprises

### 5.1 Rescue media that say what they are
- Make a stick writes a full rescue system by default; a boot-files-only stick stays possible but says so on its
  boot menu title and when it starts.
- Start-up messages name the drive they are about ("the USB stick SanDisk 64 GB" / "this computer's drive") and say
  what to do next. The "boot files only (1.2.8)" message drops the version (it is when the feature came, not the
  drive's version) and says whether a rescue stick was found.

### 5.2 Help: starting from a stick
- A Help page: Fast Boot (ASUS and others) can skip USB sticks; use the one-time boot menu (Esc or F8 on ASUS, F12 on
  many others) or turn Fast Boot off; write sticks in DD mode; Windows then calls the stick "not accessible" (do not
  format it); choose Recovery mode in Rudolf OS's menu.

### 5.3 A speaker chip that does not answer
- The self-test checks that every sound card's codec answered at start-up (the kernel's "no codecs initialized" /
  "Codec #N probe error"), and System health says "The sound chip did not answer at start-up: shut down fully (not
  restart), wait 10 seconds and turn the computer on".
- rudolf-init tries once to reset it (unbind and bind the HD Audio device) before the desktop starts.

### 5.4 Test where the risk is
- A rule for the repository: anything that changes how the desktop starts (engine, shell, display) has its fallback
  tested first, on the real graphics path (Xorg with the GPU, in QEMU with virtio-gpu and on the Vivobook), not only on
  the software display; it reaches the Vivobook only after that.
- Found on 2026-11-10 (devlink, Lite run by hand for 15 s): WebKit's web process aborted at once, "Couldn't open
  libGLESv2.so.2": WebKit opens it while running, so addwebkit.sh's ldd pass never saw it. Fixed in 1.5.7
  (addwebkit.sh copies it; the self-test checks it; webkit-check.yml runs Lite from the base layer in a chroot). With
  it, Lite ran, but GTK could not use the GPU ("No available configurations for the given pixel format", Xorg with
  modesetting on AMD): WebKit draws in software. Left for 1.5.8: GPU drawing for Lite (GTK's GL on this X server), and
  its effect on memory and speed.

## Steps
1. 4.1 start logs (initramfs, rudolf-init, session), with a host test of the rotation.
2. 4.2 devlink and remote access at start-up; QEMU check with the desktop disabled.
3. 4.3 emergency keys in rudolf-display; host test of the key handling.
4. 5.3 sound chip check and reset; 5.1 rescue media and messages; 5.2 Help page.
5. 5.4 the testing rule in docs, then the Lite investigation on the Vivobook (devlink plus remote access).
