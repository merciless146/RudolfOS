// 1.6.2 (docs/plan-1.6.2.md Part 39): what rudolfd answers Task Manager with, made up but as rudolfd writes it (rudolfd.c api_processes,
// api_system, api_memory_breakdown, apps/running, stats, secureboot/status, sys/status, firewall), for qt/native/taskmgr_t.js: the page's
// Task Manager and the native one are given the same. No one's real computer: invented names and numbers.
const MB = 1048576;
function make() {
  const P = (pid, ppid, name, tag, user, rss, cpu, prot, cmd) => ({pid, ppid, user, name, state: 'S', rss: Math.round(rss * MB), cpu, tag, prot: !!prot, cmd: cmd || '/usr/bin/' + name});
  const items = [
    P(1, 0, 'init', 'system', '0', 6.2, 0, 1), P(388, 1, 'Xorg', 'system', '0', 61, 1.2, 1), P(412, 1, 'rudolfd', 'system', '0', 9.8, 0.4, 1),
    P(530, 1, 'rudolf-native', 'desktop', 'rudolf', 96.4, 2.1, 0), P(531, 530, 'WebKitWebProcess', 'web', 'rudolf', 131.5, 0.8, 0, '/usr/lib/x86_64-linux-gnu/webkit2gtk-4.1/WebKitWebProcess 12'),
    P(533, 530, 'WebKitNetworkProcess', 'network', 'rudolf', 22.3, 0.1, 0), P(540, 1, 'pulseaudio', 'audio', 'rudolf', 14.6, 0.3, 0), P(600, 1, 'wpa_supplicant', 'system', '0', 7.1, 0, 0),
    P(612, 1, 'NetworkManager', 'network', '0', 18.4, 0.1, 0), P(700, 530, 'rudolf-taskmgr', 'desktop', 'rudolf', 18.2, 0.5, 0), P(820, 1, 'chrome', 'chromium', 'rudolf', 88.1, 0.6, 0, '/opt/chromium/chrome --type=gpu-process'),
    P(821, 820, 'chrome', 'chromium', 'rudolf', 40.2, 0.2, 0, '/opt/chromium/chrome --type=utility --utility-sub-type=network.mojom.NetworkService'),
    P(900, 1, 'flatpak-session', 'app', 'rudolf', 5.5, 0, 0), P(901, 900, 'discord', 'app', 'rudolf', 210.7, 3.4, 0, '/app/discord/Discord'), P(902, 901, 'discord-helper', 'app', 'rudolf', 64.2, 0.1, 0),
    P(950, 1, 'cupsd', 'system', '0', 4.4, 0, 0), P(2, 0, 'kthreadd', 'kernel', '0', 0, null, 1), P(30, 2, 'kworker/0:1', 'kernel', '0', 0, 0, 1)];
  return {
    stats: {ok: true, cpu: 12.3, load: 0.42, memTotal: 3.4 * 1024 * MB, memFree: 1.2 * 1024 * MB, memAvailable: 2.0 * 1024 * MB, tempC: 47, battery: [{name: 'BAT0', type: 'Battery', capacity: 81, status: 'Discharging'}]},
    processes: {ok: true, threads: 8, items},
    'memory/breakdown': {ok: true, total: 3.4 * 1024 * MB, free: 1.2 * 1024 * MB, available: 2.0 * 1024 * MB, programs: 900 * MB, cache: 600 * MB, shared: 40 * MB, kernel: 120 * MB, graphics: 64 * MB, ssd: 0, compressed: 12 * MB, other: 90 * MB},
    'apps/running': {ok: true, apps: [{id: 'flatpak:com.discordapp.Discord', app: 'com.discordapp.Discord', name: 'Discord', kind: 'flatpak', window: true, rss: 274 * MB}, {id: 'wine:starport', name: 'starport.exe', kind: 'wine', window: false, rss: 120 * MB}]},
    system: {ok: true, machine: 'Example Laptop 14', firmware: 'Example Firmware 1.02', renderer: 'Mesa Intel(R) UHD Graphics 600 (GLK 2)', display: 'gpu|1920x1080 60 Hz|eDP-1', gsp: '',
      kernel: {release: '6.18.44-rudolf', machine: 'x86_64', version: '#1 SMP PREEMPT_DYNAMIC', cmdline: 'quiet rudolf.media=disk lockdown=integrity', boot: 'uefi', media: 'disk', live: false, recovery: false, uptime: 7384, load: [0.42, 0.37, 0.3], procs: '212 processes, 731 threads',
        modules: 'i915\nsnd_hda_intel\niwlwifi\nr8169', messages: 'usb 1-2: device descriptor read/64, error -71\niwlwifi 0000:01:00.0: firmware: failed to load iwl-debug-yoyo.bin'},
      memory: {total: 3.4 * 1024 * MB, available: 2.0 * 1024 * MB, cached: 500 * MB, buffers: 40 * MB, swapTotal: 1700 * MB, swapFree: 1600 * MB, zramStored: 120 * MB, zramUsed: 40 * MB},
      cpu: {model: 'Example CPU N4000 @ 1.10GHz', threads: 2, mhz: 1096, tempC: 47},
      services: {rudolfd: true, display: true, sound: true, wifi: true, x: true, shell: false, webWindow: false, linuxApps: true},
      gpu: 'card0 0x8086:0x3185 i915', storage: 'nvme0n1p3 120034123776 40123456789 /persist', net: 'wlan0 up - 1234567 765432 wifi iwlwifi\neth0 up 100 0 0 wired r8169',
      audio: '0 [PCH            ]: HDA-Intel - HDA Intel PCH', input: 'AT Translated Set 2 keyboard\nExample Touchpad', battery: [{name: 'BAT0', type: 'Battery', capacity: 81, status: 'Discharging'}], session: 'session started\ndesktop: native', storage2: ''},
    'secureboot/status': {ok: true, efi: true, on: true, lockdown: 'none [integrity] confidentiality', enrolled: true},
    'sys/status': {ok: true, current: {version: '1.6.2', serial: '2026112201'}, url: 'https://example.invalid/beta/', booted: 'a'},
    firewall: {ok: true, on: true, blocked: 3},
    info: {ok: true, boot: 'uefi', media: 'disk', live: 0, persistTotal: 100 * 1024 * MB, persistFree: 60 * 1024 * MB},
    'wifi/status': {ok: true, raw: 'ssid=HomeNet-5G\nsaved=1\tHomeNet-5G\n'},
  };
}
module.exports = {make};
