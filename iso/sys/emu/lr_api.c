/* Rudolf OS: a small libretro frontend for Emu (WebAssembly), linked into a libretro core (1.6.1: ClownMDEmu for the
   Mega Drive / Genesis, PCSX-ReARMed for the PlayStation). It runs the core frame by frame for emu26.js: the picture as
   RGBA (any of libretro's pixel formats), the sound as 16-bit stereo at the core's rate, the RetroPad (16 buttons and
   two analog sticks) and saves/states through one buffer. Games too big for memory (CDs) are files the core opens by
   their path: the WebAssembly host's WASI calls read them from the game library in pieces (emu26.js).
   GPL-2.0-or-later (with the cores: ClownMDEmu is AGPL-3.0-or-later, PCSX-ReARMed GPL-2.0-or-later). */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include "libretro.h"
#define EX(n) __attribute__((export_name(#n)))

static uint8_t *buf; static int bufn;                 /* the exchange buffer: a game, a save, a state */
static uint32_t *pix; static int pw, ph, pcap;        /* the last frame, RGBA */
static int fmt = RETRO_PIXEL_FORMAT_0RGB1555;
/* the pixel formats offered (bits: 1 0RGB1555, 2 XRGB8888, 4 RGB565). ClownMDEmu converts the colours it is given before
   its first frame in its first format (0RGB1555) whatever it asks for later, so for it emu26.js offers 0RGB1555 only */
static int fmt_ok = 7;
EX(lr_formats) void lr_formats(int m) { fmt_ok = m & 7 ? m & 7 : 7; }
static int16_t *snd; static int sn, scap;             /* this frame's sound, stereo, at the core's rate */
static int16_t *out; static int on, ocap;            /* ...and at 48 kHz or less, for the page: a core faster than 48 kHz (ClownMDEmu: ~224 kHz) is averaged down */
static double frac; static int32_t accl, accr, accn;
static uint32_t pad[2]; static int16_t axes[2][4];    /* RetroPad buttons (bit = RETRO_DEVICE_ID_JOYPAD_*), sticks LX LY RX RY */
static struct retro_system_av_info av;
static char sysdir[] = "/bios", savdir[] = "/saves";
static int loaded, rot, geom_changed;
static char opts[64][2][96]; static int nopts;        /* core options: key, value (the first offered, or one set from JS) */
static char log_line[512]; static uint32_t raw0;
EX(lr_raw0) uint32_t lr_raw0(void) { return raw0; }   /* (tests: the middle pixel as the core gave it) */

EX(lr_alloc) uint8_t *lr_alloc(int n) { free(buf); buf = malloc(n > 0 ? n : 1); bufn = n; return buf; }
EX(lr_buf) uint8_t *lr_buf(void) { return buf; }

static void logf_(enum retro_log_level l, const char *f, ...) { va_list a; va_start(a, f); vsnprintf(log_line, sizeof log_line, f, a); va_end(a); fputs(log_line, stderr); }
static const char *opt_get(const char *k) { for (int i = 0; i < nopts; i++) if (!strcmp(opts[i][0], k)) return opts[i][1]; return NULL; }
static void opt_set(const char *k, const char *v) {
	for (int i = 0; i < nopts; i++) if (!strcmp(opts[i][0], k)) { snprintf(opts[i][1], 96, "%s", v); return; }
	if (nopts < 64) { snprintf(opts[nopts][0], 96, "%s", k); snprintf(opts[nopts][1], 96, "%s", v); nopts++; }
}
/* a core option from JS before the game is loaded ("key=value") */
EX(lr_option) void lr_option(int n) { char t[200]; if (n <= 0 || n >= (int)sizeof t || !buf) return; memcpy(t, buf, n); t[n] = 0; char *e = strchr(t, '='); if (!e) return; *e = 0; opt_set(t, e + 1); }

static bool env(unsigned cmd, void *d) {
	switch (cmd) {
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: ((struct retro_log_callback *)d)->log = logf_; return true;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: { int f = *(const enum retro_pixel_format *)d; if (f < 0 || f > 2 || !(fmt_ok & (1 << f))) return false; fmt = f; return true; }
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: *(const char **)d = sysdir; return true;
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char **)d = savdir; return true;
	case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool *)d = true; return true;
	case RETRO_ENVIRONMENT_SET_ROTATION: rot = *(const unsigned *)d; return true;
	case RETRO_ENVIRONMENT_SET_GEOMETRY: av.geometry = *(const struct retro_game_geometry *)d; geom_changed = 1; return true;
	case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: av = *(const struct retro_system_av_info *)d; geom_changed = 1; return true;
	case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *(unsigned *)d = 1; return true;
	case RETRO_ENVIRONMENT_SET_VARIABLES: {   /* "Description; first|second|..." : the first is the default */
		for (const struct retro_variable *v = d; v && v->key; v++) if (!opt_get(v->key)) {
			const char *s = strchr(v->value, ';'); if (!s) continue; s++; while (*s == ' ') s++;
			char t[96]; int i = 0; while (s[i] && s[i] != '|' && i < 95) { t[i] = s[i]; i++; } t[i] = 0; if (!opt_get(v->key)) opt_set(v->key, t);
		}
		return true; }
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
		for (const struct retro_core_option_definition *o = d; o && o->key; o++) if (!opt_get(o->key)) opt_set(o->key, o->default_value ? o->default_value : (o->values[0].value ? o->values[0].value : ""));
		return true; }
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
		const struct retro_core_options_intl *in = d;
		for (const struct retro_core_option_definition *o = in->us; o && o->key; o++) if (!opt_get(o->key)) opt_set(o->key, o->default_value ? o->default_value : (o->values[0].value ? o->values[0].value : ""));
		return true; }
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: {
		const struct retro_core_options_v2 *o2 = cmd == RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2 ? d : ((const struct retro_core_options_v2_intl *)d)->us;
		for (const struct retro_core_option_v2_definition *o = o2 ? o2->definitions : NULL; o && o->key; o++) if (!opt_get(o->key)) opt_set(o->key, o->default_value ? o->default_value : (o->values[0].value ? o->values[0].value : ""));
		return true; }
	case RETRO_ENVIRONMENT_GET_VARIABLE: { struct retro_variable *v = d; v->value = opt_get(v->key); return v->value != NULL; }
	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *(bool *)d = false; return true;
	case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: return true;
	case RETRO_ENVIRONMENT_GET_LANGUAGE: *(unsigned *)d = RETRO_LANGUAGE_ENGLISH; return true;
	case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: return true;
	case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: *(int *)d = 3; return true;
	case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
	case RETRO_ENVIRONMENT_SET_MEMORY_MAPS: case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO: case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
	case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE: case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS: case RETRO_ENVIRONMENT_SET_MESSAGE:
	case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK: case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
	case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
	case RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE: case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS: case RETRO_ENVIRONMENT_SET_VARIABLE:
		return true;
	case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION: *(unsigned *)d = 0; return true;
	case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION: *(unsigned *)d = 0; return true;
	case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS: *(unsigned *)d = 2; return true;
	default: return false;
	}
}
static void video(const void *data, unsigned w, unsigned h, size_t pitch) {
	if (!data) return;   /* a duplicate frame: keep the last */
	raw0 = fmt == RETRO_PIXEL_FORMAT_XRGB8888 ? ((const uint32_t *)data)[(h / 2) * (pitch / 4) + w / 2] : ((const uint16_t *)data)[(h / 2) * (pitch / 2) + w / 2];
	if ((int)(w * h) > pcap) { free(pix); pcap = w * h; pix = malloc(pcap * 4); }
	pw = w; ph = h;
	for (unsigned y = 0; y < h; y++) {
		const uint8_t *row = (const uint8_t *)data + y * pitch; uint32_t *o = pix + y * w;
		if (fmt == RETRO_PIXEL_FORMAT_XRGB8888) for (unsigned x = 0; x < w; x++) { uint32_t c = ((const uint32_t *)row)[x]; o[x] = 0xff000000u | ((c & 0xff) << 16) | (c & 0xff00) | ((c >> 16) & 0xff); }
		else if (fmt == RETRO_PIXEL_FORMAT_RGB565) for (unsigned x = 0; x < w; x++) { uint16_t c = ((const uint16_t *)row)[x]; uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
			o[x] = 0xff000000u | (((b << 3) | (b >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((r << 3) | (r >> 2)); }
		else for (unsigned x = 0; x < w; x++) { uint16_t c = ((const uint16_t *)row)[x]; uint32_t r = (c >> 10) & 31, g = (c >> 5) & 31, b = c & 31;
			o[x] = 0xff000000u | (((b << 3) | (b >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((r << 3) | (r >> 2)); }
	}
}
static void snd_need(int frames) { if (sn + frames > scap) { scap = (sn + frames) * 2; snd = realloc(snd, scap * 4); } }
static void audio1(int16_t l, int16_t r) { snd_need(1); snd[2 * sn] = l; snd[2 * sn + 1] = r; sn++; }
static size_t audiob(const int16_t *d, size_t frames) { snd_need(frames); memcpy(snd + 2 * sn, d, frames * 4); sn += frames; return frames; }
static void poll_(void) {}
static int16_t state(unsigned port, unsigned dev, unsigned idx, unsigned id) {
	if (port > 1) return 0;
	if (dev == RETRO_DEVICE_JOYPAD) return id == RETRO_DEVICE_ID_JOYPAD_MASK ? (int16_t)pad[port] : (pad[port] >> id) & 1;
	if (dev == RETRO_DEVICE_ANALOG) { if (idx == RETRO_DEVICE_INDEX_ANALOG_BUTTON) return ((pad[port] >> id) & 1) ? 0x7fff : 0; return idx < 2 && id < 2 ? axes[port][idx * 2 + id] : 0; }
	return 0;
}

/* start: the core, then a game, from buf (data) or by its path (path: "/game/..." read through WASI by emu26.js) */
EX(lr_init) void lr_init(void) {
	retro_set_environment(env); retro_set_video_refresh(video); retro_set_audio_sample(audio1); retro_set_audio_sample_batch(audiob);
	retro_set_input_poll(poll_); retro_set_input_state(state); retro_init();
}
EX(lr_need_path) int lr_need_path(void) { struct retro_system_info si; memset(&si, 0, sizeof si); retro_get_system_info(&si); return si.need_fullpath; }
EX(lr_load) int lr_load(int len, int bypath) {
	struct retro_game_info gi; memset(&gi, 0, sizeof gi);
	char path[600] = "/game/game.bin";
	if (bypath) { if (len <= 0 || len >= (int)sizeof path) return 0; memcpy(path, buf, len); path[len] = 0; }
	else { gi.data = buf; gi.size = len; }
	gi.path = path;
	if (!retro_load_game(&gi)) return 0;
	retro_get_system_av_info(&av); loaded = 1;
	retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD); retro_set_controller_port_device(1, RETRO_DEVICE_JOYPAD);
	return 1;
}
/* the controller in a port, a RETRO_DEVICE_* (PlayStation: the DualShock, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1)) */
EX(lr_device) void lr_device(int port, int dev) { if (loaded && port >= 0 && port < 2) retro_set_controller_port_device(port, dev); }
EX(lr_fps) double lr_fps(void) { return av.timing.fps; }
EX(lr_rate) double lr_rate(void) { return av.timing.sample_rate > 48000 ? 48000 : av.timing.sample_rate; }
/* the frame's sound at lr_rate(): as it came, or each output sample the average of the input samples it covers */
static void resample(void) {
	double r = av.timing.sample_rate;
	if (r <= 48000) { if (sn > ocap) { ocap = sn * 2; out = realloc(out, ocap * 4); } memcpy(out, snd, sn * 4); on = sn; return; }
	double step = r / 48000.0; int need = (int)(sn / step) + 4; if (need > ocap) { ocap = need * 2; out = realloc(out, ocap * 4); }
	on = 0;
	for (int i = 0; i < sn; i++) {
		accl += snd[2 * i]; accr += snd[2 * i + 1]; accn++; frac += 1.0;
		if (frac >= step) { frac -= step; out[2 * on] = (int16_t)(accl / accn); out[2 * on + 1] = (int16_t)(accr / accn); on++; accl = accr = accn = 0; }
	}
}
EX(lr_aspect) double lr_aspect(void) { return av.geometry.aspect_ratio > 0 ? av.geometry.aspect_ratio : (av.geometry.base_height ? (double)av.geometry.base_width / av.geometry.base_height : 4.0 / 3); }
/* a frame: buttons (bits), sticks (-32768..32767), then the picture (lr_w x lr_h at lr_pixels) and lr_samples of sound */
EX(lr_input) void lr_input(int port, int buttons, int lx, int ly, int rx, int ry) { if (port < 0 || port > 1) return; pad[port] = buttons; axes[port][0] = lx; axes[port][1] = ly; axes[port][2] = rx; axes[port][3] = ry; }
EX(lr_frame) void lr_frame(void) { if (!loaded) return; sn = 0; retro_run(); resample(); }
EX(lr_pixels) uint32_t *lr_pixels(void) { return pix; }
EX(lr_w) int lr_w(void) { return pw; }
EX(lr_h) int lr_h(void) { return ph; }
EX(lr_sound) int16_t *lr_sound(void) { return out; }
EX(lr_samples) int lr_samples(void) { return on; }
EX(lr_reset) void lr_reset(void) { if (loaded) retro_reset(); }
/* the battery save (SRAM): its size; read or written at lr_save_ptr */
EX(lr_save_size) int lr_save_size(void) { return loaded ? (int)retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) : 0; }
EX(lr_save_ptr) uint8_t *lr_save_ptr(void) { return loaded ? retro_get_memory_data(RETRO_MEMORY_SAVE_RAM) : NULL; }
/* states through buf */
EX(lr_state_save) int lr_state_save(void) { if (!loaded) return 0; size_t n = retro_serialize_size(); if (!n) return 0; lr_alloc(n); return retro_serialize(buf, n) ? (int)n : 0; }
EX(lr_state_load) int lr_state_load(int n) { return loaded && retro_unserialize(buf, n); }
EX(lr_log) const char *lr_log(void) { return log_line; }
EX(lr_fmt) int lr_fmt(void) { return fmt; }
