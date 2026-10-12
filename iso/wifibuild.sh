#!/bin/bash
# Build the Wi-Fi tools for Rudolf OS into src/wifi: wpa_supplicant and wpa_cli (WPA2, WPA3/SAE, OWE,
# enterprise EAP; nl80211 driver; OpenSSL), and hostapd for the QEMU tests (simulated access point).
# Sources: hostap (src/hostap: upstream hostap 2.12-devel as kept by LineageOS, github.com/LineageOS/android_external_wpa_supplicant_8, branch lineage-24.0) and
# libnl 3.9.0 (src/libnl, github.com/thom311/libnl), linked statically.   GPL-2.0-or-later.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/src; OUT=$S/wifi; NL=$S/libnl; NLB=$S/libnl-build
mkdir -p "$OUT" "$NLB"
# ---- libnl (core + generic netlink), without autotools
sed -e 's/@PACKAGE_STRING@/libnl 3.9.0/;s/@PACKAGE_VERSION@/3.9.0/;s/@MAJ_VERSION@/3/;s/@MIN_VERSION@/9/;s/@MIC_VERSION@/0/;s/@LT_CURRENT@/209/;s/@LT_REVISION@/0/;s/@LT_AGE@/9/' \
	$NL/include/netlink/version.h.in > $NL/include/netlink/version.h
printf '#define HAVE_LITTLE_ENDIAN 1\n#define HAVE_STRERROR_L 1\n#define HAVE_DLFCN_H 1\n#define HAVE_DECL_GETPROTOBYNAME_R 1\n#define HAVE_DECL_GETPROTOBYNUMBER_R 1\n' > $NL/config.h
rm -f $NLB/*.o $NLB/*.a
for f in $NL/lib/*.c $NL/lib/genl/*.c; do gcc -O2 -fPIC -D_GNU_SOURCE -DSYSCONFDIR=\"/etc/libnl\" -I$NL/include -I$NL/lib -I$NL -c $f -o $NLB/$(basename $(dirname $f))_$(basename $f .c).o; done
ar rcs $NLB/libnl-3.a $NLB/lib_*.o; ar rcs $NLB/libnl-genl-3.a $NLB/genl_*.o
# ---- hostap: the fork's test-only PMKID tampering must compile out without CONFIG_TESTING_OPTIONS
cd $S/hostap
python3 - <<'P'
p='src/rsn_supp/pmksa_cache.c';s=open(p).read()
a='\t\t/* Override PMKID */\n'
if a in s and '#ifdef CONFIG_TESTING_OPTIONS /* rudolf */' not in s:
    i=s.index(a);j=s.index('\t\tos_memcpy(entry->orig_pmkid, pmkid, PMKID_LEN);',i)
    s=s[:i]+'#ifdef CONFIG_TESTING_OPTIONS /* rudolf */\n'+s[i:j]+'#endif /* CONFIG_TESTING_OPTIONS */\n'+s[j:]
    open(p,'w').write(s)
P
# Android's copy includes one Bionic-only header; glibc has the same definitions in netinet/in.h
sed -i '/#include <netinet\/in6.h>/d' wpa_supplicant/wpa_supplicant_i.h
# Android's nl80211 driver picks Broadcom/Intel vendor helpers that exist only in its Android build:
# outside Android there are none (as in upstream hostap)
python3 - <<'P'
p='src/drivers/driver_nl80211.c';s=open(p).read()
a="\textern const struct wpa_driver_nl80211_vendor_ops brcm_vendor_ops, intel_vendor_ops;\n"
if a in s and "/* rudolf: no vendor helpers */" not in s:
    i=s.index(a);j=s.index("\t} else {\n\t\tdrv->vendor_ops = NULL;\n\t}\n",i)
    s=s[:i]+"\t/* rudolf: no vendor helpers */\n\tdrv->vendor_ops = NULL;\n#if 0\n"+s[i:j]+"\t}\n#endif\n"+s[j+len("\t} else {\n\t\tdrv->vendor_ops = NULL;\n\t}\n"):]
    open(p,'w').write(s)
P
export EXTRA_CFLAGS="-I$NL/include -I$S/hostap/src/common -I$S/hostap/src -I$S/hostap/src/utils -I$S/hostap/wpa_supplicant"
# Android's tree calls the background-scan module unconditionally; its make build must compile it
grep -q '^OBJS += bgscan.o' wpa_supplicant/Makefile || sed -i 's/^OBJS += $(SHA1OBJS) $(DESOBJS)$/&\nOBJS += bgscan.o/' wpa_supplicant/Makefile
grep -q '^OBJS += bgscan_simple.o' wpa_supplicant/Makefile || sed -i 's/^OBJS += bgscan.o$/&\nOBJS += bgscan_simple.o\nCFLAGS += -DCONFIG_BGSCAN_SIMPLE/' wpa_supplicant/Makefile
# two stand-ins in the NAN header (used when NAN is off) are missing "static inline"
sed -i 's/^int wpas_nan_usd_\(publish\|subscribe\)_stop_listen(/static inline &/' wpa_supplicant/nan_supplicant.h
MK="make -s -j$(nproc) LIBNL_INC=-I$NL/include"
cd $S/hostap/wpa_supplicant
cp defconfig .config
sed -i 's/^\(CONFIG_CTRL_IFACE_DBUS_NEW\|CONFIG_CTRL_IFACE_DBUS_INTRO\|CONFIG_READLINE\|CONFIG_WPS\|CONFIG_P2P\|CONFIG_AP\|CONFIG_DRIVER_WEXT\|CONFIG_DRIVER_WIRED\|CONFIG_MACSEC\|CONFIG_DRIVER_MACSEC_LINUX\|CONFIG_DRIVER_MACSEC_QCA\)=y/#&/' .config
printf 'CONFIG_DRIVER_NL80211=y\nCONFIG_LIBNL32=y\nCONFIG_TLS=openssl\nCONFIG_SAE=y\nCONFIG_OWE=y\nCONFIG_IEEE80211W=y\nCONFIG_CTRL_IFACE=y\nCONFIG_HT_OVERRIDES=y\nCONFIG_VHT_OVERRIDES=y\nCONFIG_HE_OVERRIDES=y\nCONFIG_WNM=y\n' >> .config
make -s clean >/dev/null 2>&1 || true
LDFLAGS="-L$NLB" $MK wpa_supplicant wpa_cli
strip wpa_supplicant wpa_cli; cp wpa_supplicant wpa_cli $OUT/
cd $S/hostap/hostapd
cp defconfig .config
sed -i 's/^\(CONFIG_DRIVER_WIRED\|CONFIG_MACSEC\|CONFIG_DRIVER_MACSEC_LINUX\|CONFIG_WPS\)=y/#&/' .config
printf 'CONFIG_DRIVER_NL80211=y\nCONFIG_LIBNL32=y\nCONFIG_TLS=openssl\nCONFIG_SAE=y\nCONFIG_IEEE80211W=y\n' >> .config
make -s clean >/dev/null 2>&1 || true
LDFLAGS="-L$NLB" $MK hostapd
strip hostapd; cp hostapd $OUT/
ls -la $OUT
# sources as built (with the changes above), for the disc's SOURCE folder
tar -C $S -cJf $S/wifi-src.tar.xz --owner=0 --group=0 --exclude=.git --exclude='*.o' --exclude='*.d' --exclude=build \
	--exclude=wpa_supplicant/wpa_supplicant --exclude=wpa_supplicant/wpa_cli --exclude=wpa_supplicant/wpa_passphrase --exclude=hostapd/hostapd --exclude=hostapd/hostapd_cli hostap libnl
