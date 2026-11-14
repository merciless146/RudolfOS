# 1.5.9 (docs/plan-1.5.9.md 15.5): the hardening flags every C program in sys/ is built with (sourced by
# systools/ci_release.sh, mkrootfs.sh and the ARM64 workflow; systools/hardcheck.py checks the built programs):
# the stack protector, FORTIFY_SOURCE=3, a position-independent program, full RELRO (read-only after start, every
# symbol bound at start), and control-flow protection: CET (IBT and the shadow stack) on x86-64, BTI and PAC on ARM64.
# HARD: dynamically linked programs; HARD_STATIC: static ones (static-pie). rudolf-kexec (no C library, in the
# start-up image) is built without them: there is no library to fortify and no loader to relocate it.
HARD_CF=""
case "$(gcc -dumpmachine 2>/dev/null)" in x86_64*) HARD_CF="-fcf-protection=full";; aarch64*) HARD_CF="-mbranch-protection=standard";; esac
HARD_CC="-fstack-protector-strong -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 $HARD_CF"
HARD="$HARD_CC -fPIE -pie -Wl,-z,relro,-z,now"
HARD_STATIC="$HARD_CC -fPIE -static-pie -Wl,-z,relro,-z,now"
