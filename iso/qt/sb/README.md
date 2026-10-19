# Secure Boot tests (Rudolf OS 1.2.2)

`sb_t.py` boots a test disc in QEMU on OVMF with Secure Boot and Microsoft's keys (as PCs ship) and checks the
whole chain: shim refuses the boot menu until the key is enrolled; enrolment from the disc in MokManager; the
system then starts locked down with every layer through dm-verity; Settings, Task Manager and the report say
so; the UEFI boot menu's Recovery mode; an update slot with a changed layer is dropped and one with an
unsigned kernel does not start; Prepare Secure Boot (MokManager enrolls a requested certificate after its
one-time password); and a UEFI computer with Secure Boot off.

```sh
S=/some/scratch
# the parts: the files of the draft release sb-parts (workflow "Secure Boot parts")
# a test Secure Boot key:  (cd $S/sbtest && sh ../../iso/systools/mksbkey.sh)
# a test release with that key, then a test disc whose kernel bundle opens a serial shell (tests only):
RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1 SB_PARTS=$S/sbparts SB_KEY=$S/sbtest/rudolf-sb.key \
  SB_CERT=$S/sbtest/rudolf-sb.crt SBSIGN=$S/sbparts/sbsign bash ../../systools/mkdisc.sh OLD.iso RELEASE-DIR 1.2.2 $S/disc
# TESTDATA: an ext4 image labelled TESTDATA with unsigned-vmlinuz and other.crt
SB_PARTS=$S/sbparts QEMU=path/to/qemu python3 sb_t.py $S/disc/rudolf-os-1.2.2.iso $S/testdata.img
```

`sbvm.py` starts such a computer (`VM(name, parts, drives, secure=True)`); its screenshots are in the folder
named after the VM.
