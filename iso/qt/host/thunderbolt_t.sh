#!/bin/sh
# 1.5.4: /usr/lib/rudolf/thunderbolt against a stand-in /sys/bus/thunderbolt: a dock waits for an administrator's
# answer; approved, it is kept and approved by itself when plugged in again; "no" holds until it is unplugged; with DMA
# protection (iommu_dma_protection) devices are approved by themselves; Forget; the JSON rudolfd passes on.
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/thunderbolt
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
export RUDOLF_TEST_SYS=$T/sys RUDOLF_TEST_PERSIST=$T/persist RUDOLF_TEST_RUN=$T/run; mkdir -p $T/run $T/persist/etc
D=$T/sys/bus/thunderbolt/devices; mkdir -p $D/domain0 $D/0-0 $D/0-0:1.1
echo user > $D/domain0/security; echo 0 > $D/domain0/iommu_dma_protection; echo 1 > $D/0-0/authorized
plug() {   # plug ID UID VENDOR NAME
	mkdir -p $D/$1; echo 0 > $D/$1/authorized; echo "$2" > $D/$1/unique_id; echo "$3" > $D/$1/vendor_name; echo "$4" > $D/$1/device_name; echo 1 > $D/$1/deauthorization
	ACTION=add DEVPATH=/devices/pci0000:00/0000:00:0d.2/domain0/0-0/$1 sh $S plug; }
unplug() { rm -rf $D/$1; ACTION=remove DEVPATH=/devices/pci0000:00/0000:00:0d.2/domain0/0-0/$1 sh $S plug; }
A() { tr -d '\n' < $D/$1/authorized; }
plug 0-1 d5010000-0060-6508-2304-61066cc4211e CalDigit "TS4"
ok "a new dock without DMA protection waits" "$([ "$(A 0-1)" = 0 ] && echo 1)"
ok "...and the desktop is asked about it" "$(has "$(cat $T/run/tb-ask 2>/dev/null)" '"id":"0-1"')" "$(cat $T/run/tb-ask 2>/dev/null)"
ok "...with its name" "$(has "$(cat $T/run/tb-ask)" '"name":"CalDigit TS4"')"
ok "...and an event" "$(has "$(cat $T/run/events)" 'tb ask 0-1')"
sh $S deny 0-1 >/dev/null
ok "no: not approved, and not asked again" "$([ "$(A 0-1)" = 0 ] && [ ! -s $T/run/tb-ask ] && echo 1)"
unplug 0-1; plug 0-1 d5010000-0060-6508-2304-61066cc4211e CalDigit "TS4"
ok "plugged in again after a no: asked again" "$(has "$(cat $T/run/tb-ask 2>/dev/null)" '"id":"0-1"')"
O=$(sh $S allow 0-1)
ok "yes: approved" "$([ "$(A 0-1)" = 1 ] && [ "$O" = approved ] && echo 1)" "$O"
ok "...kept by its unique ID" "$(grep -q '^d5010000-0060-6508-2304-61066cc4211e CalDigit TS4$' $T/persist/etc/thunderbolt-allowed && echo 1)" "$(cat $T/persist/etc/thunderbolt-allowed)"
ok "...and the question is gone" "$([ ! -s $T/run/tb-ask ] && echo 1)"
unplug 0-1; plug 0-3 d5010000-0060-6508-2304-61066cc4211e CalDigit "TS4"
ok "the kept dock in another port: approved by itself" "$([ "$(A 0-3)" = 1 ] && [ ! -s $T/run/tb-ask ] && echo 1)"
J=$(sh $S json)
ok "json: valid" "$(echo "$J" | python3 -c 'import json,sys; json.load(sys.stdin)' && echo 1)" "$J"
ok "json: the dock, approved and kept; the host router and ports are not devices" "$(echo "$J" | python3 -c 'import json,sys; j=json.load(sys.stdin); d=j["devices"]; print(1 if len(d)==1 and d[0]["id"]=="0-3" and d[0]["approved"] and d[0]["kept"] and d[0]["canDisconnect"] and not j["dma"] and j["security"]=="user" and j["present"] else 0)')" "$J"
sh $S forget d5010000-0060-6508-2304-61066cc4211e >/dev/null
ok "forget: no longer kept, and disconnected" "$([ ! -s $T/persist/etc/thunderbolt-allowed ] && [ "$(A 0-3)" = 0 ] && echo 1)"
plug 0-301 0b0b0000-1111-2222-3333-444455556666 'Bad "name\' 'Drive'
ok "names cannot break the JSON" "$(sh $S json | python3 -c 'import json,sys; json.load(sys.stdin)' >/dev/null && echo 1)" "$(sh $S json)"
echo 1 > $D/domain0/iommu_dma_protection
plug 0-5 aaaa0000-0000-0000-0000-000000000001 OWC "Envoy Express"
ok "with DMA protection: approved by itself, and kept" "$([ "$(A 0-5)" = 1 ] && grep -q '^aaaa0000-0000-0000-0000-000000000001 ' $T/persist/etc/thunderbolt-allowed && echo 1)"
echo 0 > $D/domain0/iommu_dma_protection; echo 0 > $D/0-3/authorized; echo 0 > $D/0-301/authorized; rm -f $T/run/tb-ask
sh $S boot
ok "start-up: a device that was not approved is asked about" "$(has "$(cat $T/run/tb-ask 2>/dev/null)" '"id":"0-3"')" "$(cat $T/run/tb-ask 2>/dev/null)"
O=$(sh $S allow 'x;reboot'); ok "a bad ID is refused" "$([ "$O" = "no such device" ] && echo 1)" "$O"
O=$(sh $S forget '../x'); ok "a bad unique ID is refused" "$([ "$O" = "bad device" ] && echo 1)" "$O"
rm -rf $T/sys/bus/thunderbolt; J=$(sh $S json)
ok "no Thunderbolt or USB4 here: present false" "$(echo "$J" | python3 -c 'import json,sys; j=json.load(sys.stdin); print(1 if not j["present"] and j["devices"]==[] else 0)')" "$J"
echo "$((N-F))/$N passed"; [ $F = 0 ]
