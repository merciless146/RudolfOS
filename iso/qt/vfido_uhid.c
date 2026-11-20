/*
 * vfido_uhid - the guest's half of the simulated FIDO2 security keys of the QEMU test (iso/qt/sb/fido161_t.py; Rudolf OS
 * 1.6.1, docs/plan-1.6.1.md Part 18). Test-only: never part of a release.
 *
 * It makes each software key (iso/qt/host/vfido.py, on the host) a real HID device in the guest with the kernel's uhid,
 * a FIDO HID device as a USB security key is (usage page 0xF1D0, 64-byte input and output reports), so the guest's own
 * libfido2 finds it as /dev/hidrawN and rudolfd talks to it as it would to a key in a USB port. The reports go to the
 * host over a serial port (QEMU's isa-serial on a socket), in frames of 66 bytes: TYPE, KEY, 64 bytes, where TYPE 0 is
 * a report (either way), TYPE 1 from the host plugs KEY in (byte 0 of the data: 1) or takes it out (0), and TYPE 2 to
 * the host says the relay is running.
 *   vfido_uhid /dev/ttyS1        (root: /dev/uhid)        Built static: gcc -O2 -static -o vfido_uhid vfido_uhid.c
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <linux/uhid.h>
#define KEYS 4
static const uint8_t RDESC[] = {
	0x06, 0xD0, 0xF1, 0x09, 0x01, 0xA1, 0x01,                                  /* usage page FIDO, CTAPHID, application */
	0x09, 0x20, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x40, 0x81, 0x02,  /* input: 64 bytes */
	0x09, 0x21, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x40, 0x91, 0x02,  /* output: 64 bytes */
	0xC0 };
static int K[KEYS] = { -1, -1, -1, -1 }, S = -1;
static int full(int f, void *b, size_t n, int wr)
{
	size_t d = 0; while (d < n) { ssize_t k = wr ? write(f, (char *)b + d, n - d) : read(f, (char *)b + d, n - d); if (k < 0 && errno == EINTR) continue; if (k <= 0) return -1; d += k; } return 0;
}
static void send_frame(int type, int key, const uint8_t *data)
{
	uint8_t f[66] = { (uint8_t)type, (uint8_t)key }; if (data) memcpy(f + 2, data, 64); full(S, f, 66, 1);
}
static void plug(int i, int on)
{
	if (i < 0 || i >= KEYS) return;
	if (!on) { if (K[i] >= 0) { struct uhid_event e = { .type = UHID_DESTROY }; full(K[i], &e, sizeof e, 1); close(K[i]); K[i] = -1; } return; }
	if (K[i] >= 0) return;
	int f = open("/dev/uhid", O_RDWR | O_CLOEXEC); if (f < 0) { perror("/dev/uhid"); return; }
	struct uhid_event e; memset(&e, 0, sizeof e); e.type = UHID_CREATE2;
	snprintf((char *)e.u.create2.name, sizeof e.u.create2.name, "Rudolf OS test security key %d", i + 1);
	snprintf((char *)e.u.create2.phys, sizeof e.u.create2.phys, "vfido%d", i);
	snprintf((char *)e.u.create2.uniq, sizeof e.u.create2.uniq, "vfido-%d", i);
	memcpy(e.u.create2.rd_data, RDESC, sizeof RDESC); e.u.create2.rd_size = sizeof RDESC;
	e.u.create2.bus = BUS_USB; e.u.create2.vendor = 0x1209; e.u.create2.product = 0x0001 + i;   /* (pid.codes' test numbers) */
	if (full(f, &e, sizeof e, 1)) { perror("UHID_CREATE2"); close(f); return; }
	K[i] = f;
}
int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "vfido_uhid /dev/ttySN\n"); return 2; }
	S = open(argv[1], O_RDWR | O_NOCTTY | O_CLOEXEC); if (S < 0) { perror(argv[1]); return 1; }
	struct termios t; if (!tcgetattr(S, &t)) { cfmakeraw(&t); cfsetspeed(&t, B115200); tcsetattr(S, TCSANOW, &t); }
	send_frame(2, 0, NULL);
	for (;;) {
		struct pollfd p[KEYS + 1]; int n = 0, map[KEYS + 1];
		p[n].fd = S; p[n].events = POLLIN; map[n++] = -1;
		for (int i = 0; i < KEYS; i++) if (K[i] >= 0) { p[n].fd = K[i]; p[n].events = POLLIN; map[n++] = i; }
		if (poll(p, n, -1) < 0) { if (errno == EINTR) continue; return 1; }
		if (p[0].revents & (POLLHUP | POLLERR)) return 1;
		if (p[0].revents & POLLIN) {
			uint8_t f[66]; if (full(S, f, 66, 0)) return 1;
			if (f[0] == 1) plug(f[1], f[2]);
			else if (f[0] == 0 && f[1] < KEYS && K[f[1]] >= 0) { struct uhid_event e; memset(&e, 0, sizeof e); e.type = UHID_INPUT2; e.u.input2.size = 64; memcpy(e.u.input2.data, f + 2, 64); full(K[f[1]], &e, sizeof e, 1); }
		}
		for (int j = 1; j < n; j++) if (p[j].revents & POLLIN) {
			struct uhid_event e; ssize_t r = read(p[j].fd, &e, sizeof e); if (r <= 0) continue;
			if (e.type == UHID_OUTPUT) {   /* (a hidraw write: the report ID byte 0 first when the device has none) */
				const uint8_t *d = e.u.output.data; size_t sz = e.u.output.size; if (sz == 65 && d[0] == 0) { d++; sz--; }
				if (sz == 64) send_frame(0, map[j], d);
			} else if (e.type == UHID_GET_REPORT) { struct uhid_event a; memset(&a, 0, sizeof a); a.type = UHID_GET_REPORT_REPLY; a.u.get_report_reply.id = e.u.get_report.id; a.u.get_report_reply.err = EIO; full(p[j].fd, &a, sizeof a, 1); }
			else if (e.type == UHID_SET_REPORT) { struct uhid_event a; memset(&a, 0, sizeof a); a.type = UHID_SET_REPORT_REPLY; a.u.set_report_reply.id = e.u.set_report.id; a.u.set_report_reply.err = EIO; full(p[j].fd, &a, sizeof a, 1); }
		}
	}
}
