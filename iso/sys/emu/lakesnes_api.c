/* Rudolf OS: a small interface to LakeSnes for Emu (WebAssembly). MIT, like LakeSnes. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../src/lakesnes/snes/snes.h"
#define EX(n) __attribute__((export_name(#n)))
static Snes *S; static uint8_t *rom; static uint8_t pix[512 * 480 * 4]; static int16_t snd[2 * 2048]; static uint8_t *buf; static int bufn;
EX(ls_alloc) uint8_t *ls_alloc(int n) { free(buf); buf = malloc(n > 0 ? n : 1); bufn = n; return buf; }
EX(ls_load) int ls_load(int len) {
	if (!S) S = snes_init();
	/* a 512-byte copier header in front of the game is removed */
	uint8_t *d = buf; if (len % 1024 == 512) { d += 512; len -= 512; }
	free(rom); rom = malloc(len); memcpy(rom, d, len);
	return snes_loadRom(S, rom, len) ? 1 : 0;
}
EX(ls_pal) int ls_pal(void) { return S && S->palTiming; }
EX(ls_reset) void ls_reset(int hard) { if (S) snes_reset(S, hard); }
EX(ls_button) void ls_button(int player, int button, int down) { if (S) snes_setButtonState(S, player, button, down); }
EX(ls_frame) void ls_frame(int samples) { if (!S) return; snes_runFrame(S); snes_setPixels(S, pix); if (samples > 0 && samples <= 2048) snes_setSamples(S, snd, samples); }
EX(ls_pixels) uint8_t *ls_pixels(void) { return pix; }
EX(ls_sound) int16_t *ls_sound(void) { return snd; }
/* battery saves and states go through buf: *_size, then read buf / fill buf and *_load */
EX(ls_battery_save) int ls_battery_save(void) { if (!S) return 0; int n = snes_saveBattery(S, NULL); if (n <= 0) return 0; ls_alloc(n); snes_saveBattery(S, buf); return n; }
EX(ls_battery_load) int ls_battery_load(int n) { return S && snes_loadBattery(S, buf, n); }
EX(ls_state_save) int ls_state_save(void) { if (!S) return 0; int n = snes_saveState(S, NULL); ls_alloc(n); snes_saveState(S, buf); return n; }
EX(ls_state_load) int ls_state_load(int n) { return S && snes_loadState(S, buf, n); }
EX(ls_buf) uint8_t *ls_buf(void) { return buf; }
