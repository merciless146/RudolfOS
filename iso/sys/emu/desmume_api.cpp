/* Rudolf OS: interface between Emu and DeSmuME (WebAssembly, no Emscripten), adapted from the
   desmume-wasm port's wasm-port/main.cpp by 44670. GPL-2.0-or-later, like DeSmuME. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include "MMU.h"
#include "NDSSystem.h"
#include "SPU.h"
#include "GPU.h"
#include "mc.h"
#include "rasterize.h"
#include "saves.h"
#include "emufile.h"
#include "utils/colorspacehandler/colorspacehandler.h"
#include "mic.h"
#include "utils/decrypt/decrypt.h"
#define EX(n) __attribute__((export_name(#n)))

int emuLastError = 0;
int DESMUME_SAMPLE_RATE = 48000;
extern double samples_per_hline;
EMUFILE_MEMORY *savFile = new EMUFILE_MEMORY();
u8 *romBuffer; static int romBufferCap; int romLen;
volatile bool execute = true; volatile bool paused = false;
static s16 audioBuffer[16384 * 2]; static int samplesRead, samplesDesired;
static u32 frame[2 * 256 * 192];
static u8 *xbuf; static size_t xlen;   /* save states pass through here */

static void SNDWasmUpdateAudio(s16 *buffer, u32 n) { if (n > 16384) n = 16384; samplesRead = n; memcpy(audioBuffer, buffer, sizeof(s16) * n * 2); }
static u32 SNDWasmGetAudioSpace() { return samplesDesired; }
static int SNDWasmInit(int) { return 0; }
static void SNDWasmNop() {}
static void SNDWasmSetVolume(int) {}
SoundInterface_struct SndWasm = {1, "Rudolf OS sound", SNDWasmInit, SNDWasmNop, SNDWasmUpdateAudio, SNDWasmGetAudioSpace, SNDWasmNop, SNDWasmNop, SNDWasmSetVolume, SNDWasmNop, NULL, NULL};
SoundInterface_struct *SNDCoreList[] = {&SNDDummy, &SndWasm, NULL};
GPU3DInterface *core3DList[] = {&gpu3DNull, &gpu3DRasterize, NULL};
/* no microphone: silence */
u8 Mic_ReadSample() { return 64; }
BOOL Mic_Init() { return TRUE; }
void Mic_Reset() {}
void Mic_DeInit() {}
void mic_savestate(EMUFILE &) {}
bool mic_loadstate(EMUFILE &, int) { return true; }
/* encrypted (undumped) game images are not supported: that needs Nintendo's key data */
bool DecryptSecureArea(u8 *, u8 *) { emuLastError = -1001; return false; }

extern "C" {
int bb(int, int, int, int) { return 0; }
EX(ds_init) int ds_init(int rate) {
	srand((unsigned)time(NULL));
	DESMUME_SAMPLE_RATE = rate > 0 ? rate : 48000;
	samples_per_hline = (DESMUME_SAMPLE_RATE / 59.8261f) / 263.0f;
	NDS_Init(); SPU_ChangeSoundCore(1, 16384);
	GPU->Change3DRendererByID(RENDERID_SOFTRASTERIZER);
	return 1;
}
EX(ds_rom_buffer) void *ds_rom_buffer(int n) { romLen = n; if (n > romBufferCap) { romBuffer = (u8 *)realloc(romBuffer, n); romBufferCap = n; } return romBuffer; }
EX(ds_load) int ds_load(void) {
	emuLastError = -2;
	if (!NDS_LoadROM("rom.nds")) return emuLastError ? emuLastError : -2;
	SPU_SetSynchMode(ESynchMode_Synchronous, ESynchMethod_N); SPU_SetVolume(100);
	return 0;
}
EX(ds_reset) void ds_reset(void) { NDS_Reset(); }
/* keys: bit 0 R, 1 L, 2 down, 3 up, 4 left, 5 right, 6 B, 7 A, 8 Y, 9 X, 10 Start, 11 Select, 12 debug, 13 lid */
EX(ds_frame) void ds_frame(int draw, u32 keys, int touched, u32 tx, u32 ty) {
	if (!draw) NDS_SkipNextFrame();
	if (touched) NDS_setTouchPos(tx, ty); else NDS_releaseTouch();
	NDS_setPad(keys & 1, keys & 2, keys & 4, keys & 8, keys & 16, keys & 32, keys & 64, keys & 128, keys & 256, keys & 512, keys & 1024, keys & 2048, keys & 4096, keys & 8192);
	NDS_beginProcessingInput(); NDS_endProcessingInput();
	NDS_exec<false>();
	if (draw) ColorspaceConvertBuffer555To8888Opaque<false, false>((const u16 *)GPU->GetDisplayInfo().masterNativeBuffer, frame, 256 * 192 * 2);
}
EX(ds_frame_ptr) u32 *ds_frame_ptr(void) { return frame; }
EX(ds_audio) int ds_audio(int want) { samplesDesired = want; samplesRead = 0; SPU_Emulate_user(); return samplesRead; }
EX(ds_audio_ptr) s16 *ds_audio_ptr(void) { return audioBuffer; }
EX(ds_sav_size) int ds_sav_size(void) { return (int)savFile->size(); }
EX(ds_sav_ptr) void *ds_sav_ptr(int want) { if (want > 0) { savFile->truncate(want); savFile->fseek(0, SEEK_SET); } return savFile->buf(); }
EX(ds_sav_changed) int ds_sav_changed(void) { int r = savFile->changed ? 1 : 0; savFile->changed = 0; return r; }
EX(ds_state_save) int ds_state_save(void) {
	EMUFILE_MEMORY m; if (!savestate_save(m, -1)) return 0;
	free(xbuf); xlen = m.size(); xbuf = (u8 *)malloc(xlen ? xlen : 1); memcpy(xbuf, m.buf(), xlen); return (int)xlen;
}
EX(ds_state_buf) u8 *ds_state_buf(int n) { if (n > 0) { free(xbuf); xbuf = (u8 *)malloc(n); xlen = n; } return xbuf; }
EX(ds_state_load) int ds_state_load(void) { EMUFILE_MEMORY m(xbuf, (s32)xlen); return savestate_load(m) ? 1 : 0; }
EX(ds_error) int ds_error(void) { return emuLastError; }
}
