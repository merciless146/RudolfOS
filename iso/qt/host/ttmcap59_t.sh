#!/bin/sh
# 1.5.9 (plan 10): the graphics page pool is capped at 64 MiB: BusyBox's modprobe reads /etc/modules/ttm when ttm
# loads, and the memory script sets it when ttm was already loaded.   sh ttmcap59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); R=$HERE/../../sys/root; W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1"; fi; }
[ "$(cat "$R/etc/modules/ttm")" = "page_pool_size=16384" ]; ok "1. /etc/modules/ttm: page_pool_size=16384 (64 MiB of 4 KiB pages)" $?
# the memory script's lines for it, run against a stand-in /sys
L=$(grep -A2 'graphics pool capped' "$R/usr/lib/rudolf/memory" | tail -1 | sed "s#/sys/module/ttm#$W/ttm#g")
mkdir -p "$W/ttm/parameters"; echo 437188 > "$W/ttm/parameters/page_pool_size"; log() { echo "$*" > "$W/log"; }
eval "$L"; [ "$(cat "$W/ttm/parameters/page_pool_size")" = 16384 ] && grep -q "at most 64 MiB" "$W/log"; ok "2. ttm already loaded: the memory script caps it (logged)" $?
rm -rf "$W/ttm" "$W/log"; eval "$L"; [ ! -e "$W/log" ]; ok "3. ttm not loaded yet: nothing to do (the module file applies when it loads)" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
