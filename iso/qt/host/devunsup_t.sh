#!/bin/sh
# 1.5.3: devplug's list of devices without a driver (PCI, USB, known ACPI parts), USB speeds and the port's speed,
# and the health-log line on a computer's first start. A stand-in /sys like the Vivobook's: the RT5370 dongle in a
# USB 2.0 port, a USB 2 drive in a USB 3 port, the Realtek Bluetooth with its driver, plus an AMD sound processor,
# an NPU and a Cirrus amplifier without one.   sh devunsup_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); D=$HERE/../../sys/root/usr/lib/rudolf/devplug
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
S=$T/sys; mkdir -p $S/bus/pci/devices $S/bus/usb/devices $S/bus/acpi/devices $S/drivers/x $S/class/dmi/id
pci() { local d=$S/devices/pci0000:00/$1; mkdir -p $d; echo 0x$2 > $d/class; echo 0x$3 > $d/vendor; echo 0x$4 > $d/device; [ -n "$5" ] && ln -s $S/drivers/x $d/driver; ln -s $d $S/bus/pci/devices/$1; }
pci 0000:00:00.0 060000 1022 15e8                 # host bridge: needs no driver
pci 0000:02:00.0 028000 10ec c821 drv             # the Wi-Fi card, with its driver
pci 0000:03:00.5 048000 1022 15e2                 # AMD audio processor (ACP), no driver
pci 0000:04:00.1 118000 1022 15e4                 # a sensor hub: needs no driver here
pci 0000:05:00.0 120000 1022 1502                 # an NPU, no driver
C=$S/devices/pci0000:00/0000:03:00.4
hub() { mkdir -p $C/$1/${1#usb}-0:1.0; echo $2 > $C/$1/speed; echo 1d6b > $C/$1/idVendor; echo 09 > $C/$1/bDeviceClass; ln -s $C/$1 $S/bus/usb/devices/$1; }
hub usb3 480; hub usb4 10000
port() { mkdir -p $C/$1/${1#usb}-0:1.0/$1-port$2; }
port usb3 1; port usb3 3; port usb4 1; ln -s ../../../usb4/4-0:1.0/usb4-port1 $C/usb3/3-0:1.0/usb3-port1/peer
usbdev() {  # name hub port vid pid speed ifclass driver product manufacturer
	local d=$C/$2/$1; mkdir -p $d/$1:1.0; echo $4 > $d/idVendor; echo $5 > $d/idProduct; echo 00 > $d/bDeviceClass; echo $6 > $d/speed
	echo $7 > $d/$1:1.0/bInterfaceClass; echo 00 > $d/$1:1.0/bInterfaceSubClass; echo 00 > $d/$1:1.0/bInterfaceProtocol
	[ "$8" = drv ] && ln -s $S/drivers/x $d/$1:1.0/driver; echo "$9" > $d/product; echo "${10}" > $d/manufacturer
	ln -s ../${2#usb}-0:1.0/$2-port$3 $d/port; ln -s $d $S/bus/usb/devices/$1; }
usbdev 3-3 usb3 3 148f 5370 480 ff no "802.11 n WLAN" Ralink
usbdev 3-1 usb3 1 0781 5567 480 08 drv "Cruzer Blade" SanDisk
usbdev 3-2 usb3 3 13d3 3556 12 e0 drv "Bluetooth Radio" Realtek
acpi() { local d=$S/bus/acpi/devices/$1; mkdir -p $d; [ -n "$2" ] && echo $2 > $d/status; [ "$3" = drv ] && { mkdir -p $T/i2c/$1; ln -s $S/drivers/x $T/i2c/$1/driver; ln -s $T/i2c/$1 $d/physical_node; }; }
acpi CSC3551:00 15          # Cirrus amplifier, present, no driver
acpi PNP0C50:00 15 drv      # the touchpad, with its driver
acpi INT3472:01 0           # described, but not there
acpi PNP0A08:00 15          # not a kind it lists
echo ASUSTeK > $S/class/dmi/id/sys_vendor; echo "Vivobook Go E1404FA_E1404FA" > $S/class/dmi/id/product_name
export RUDOLF_SYS=$S RUDOLF_RUN=$T/run
o=$(sh $D unsupported)
ok "four devices without a driver, nothing else" $([ "$(echo "$o" | wc -l)" = 4 ] && echo 1 || echo 0) "$o"
ok "the RT5370 dongle: a Wi-Fi adapter, by its name" $(has "$o" "usb|148f:5370|Wi-Fi adapter|Ralink 802.11 n WLAN") "$o"
ok "the AMD audio processor" $(has "$o" "pci|1022:15e2|Sound processor|AMD Sound processor") "$o"
ok "the NPU" $(has "$o" "pci|1022:1502|AI processor (NPU)|AMD AI processor (NPU)") "$o"
ok "the Cirrus amplifier (ACPI)" $(has "$o" "acpi|CSC3551|Speaker amplifier|Cirrus Speaker amplifier") "$o"
ok "not: the bridge, the sensor hub, the touchpad, an absent part, devices with drivers" $([ "$(echo "$o" | grep -cE '15e8|15e4|PNP0C50|INT3472|PNP0A08|c821|0781|13d3')" = 0 ] && echo 1 || echo 0) "$o"
j=$(sh $D unsupported json)
ok "the JSON is valid and has the four" $(node -e "const j=JSON.parse(process.argv[1]);process.stdout.write(j.ok&&j.devices.length==4&&j.devices.some(d=>d.kind=='Wi-Fi adapter'&&d.id=='148f:5370')?'1':'0')" "$j") "$j"
l=$(sh $D list)
ok "the device list is valid JSON" $(node -e "JSON.parse(process.argv[1]);process.stdout.write('1')" "$l" 2>/dev/null || echo 0) "$l"
ok "the dongle: USB 2.0 (480) in a USB 2.0 port (480), no driver" $(has "$l" '"id":"148f:5370","driver":0,"speed":480,"port":480') "$l"
ok "the drive: USB 2.0 (480) in a USB 3 port (its partner's 10000)" $(has "$l" '"id":"0781:5567","driver":1,"speed":480,"port":10000') "$l"
ok "the Bluetooth adapter: USB 1.1 (12)" $(has "$l" '"id":"13d3:3556","driver":1,"speed":12') "$l"
mkdir -p $T/run; cat > $T/health <<'X'
#!/bin/sh
echo "$*" >> $HLOG
X
chmod +x $T/health; export RUDOLF_HEALTH=$T/health HLOG=$T/hlog
sh $D hwlog; h=$(cat $T/hlog 2>/dev/null)
ok "first start on this computer: one hardware line with the model and what is missing" $(has "$h" "hardware ASUSTeK Vivobook Go E1404FA_E1404FA: no driver yet for 4 device(s): AMD Sound processor (pci 1022:15e2), AMD AI processor (NPU) (pci 1022:1502), Ralink 802.11 n WLAN, Wi-Fi adapter (usb 148f:5370), Cirrus Speaker amplifier (acpi CSC3551)") "$h"
sh $D hwlog
ok "...and not again on the next start" $([ "$(wc -l < $T/hlog)" = 1 ] && echo 1 || echo 0) "$(cat $T/hlog)"
echo "Other Laptop" > $S/class/dmi/id/product_name; rm -rf $S/bus/pci/devices/0000:03:00.5 $S/bus/pci/devices/0000:05:00.0 $S/bus/usb/devices/3-3 $S/bus/acpi/devices/CSC3551:00
sh $D hwlog
ok "the disk in another computer: a new line, and it says everything has a driver" $([ "$(wc -l < $T/hlog)" = 2 ] && [ "$(has "$(tail -1 $T/hlog)" "ASUSTeK Other Laptop: every device has a driver")" = 1 ] && echo 1 || echo 0) "$(cat $T/hlog)"
# 1.5.4: Intel's camera processor with its driver (IPU6) is still listed: apps cannot use the camera until libcamera
mkdir -p $S/drivers/intel-ipu6; d=$S/devices/pci0000:00/0000:00:05.0; mkdir -p $d; echo 0x048000 > $d/class; echo 0x8086 > $d/vendor; echo 0x7d19 > $d/device
ln -s $S/drivers/intel-ipu6 $d/driver; ln -s $d $S/bus/pci/devices/0000:00:05.0
o=$(sh $D unsupported)
ok "the IPU6 camera with its driver: listed, saying apps cannot use it yet" $(has "$o" "pci|8086:7d19|Camera|Intel built-in camera (IPU): driver ready, apps cannot use it yet") "$o"
ok "...once" $([ "$(echo "$o" | grep -c 8086:7d19)" = 1 ] && echo 1 || echo 0) "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
