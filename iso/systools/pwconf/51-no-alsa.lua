-- Rudolf OS 1.6.3 (docs/plan-1.6.3.md Part 9): PipeWire runs beside PulseAudio, and PulseAudio keeps the sound cards.
-- WirePlumber's ALSA monitor is switched off here, so it publishes no audio device and opens no card; the v4l2 and
-- libcamera monitors stay on, which is what WebKitGTK 2.52's camera captures through. (WirePlumber 0.4's Lua.)
alsa_monitor.enabled = false
