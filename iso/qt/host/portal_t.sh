#!/bin/sh
# rudolf-portal (1.2.9) on the host: the interfaces GLib inside a Flatpak sandbox asks for (ProxyResolver: Sober's
# sign-in page; NetworkMonitor), on a private session bus. sh portal_t.sh   (needs gcc, glib, dbus-run-session)
D=$(cd "$(dirname "$0")/../../sys" && pwd); T=$(mktemp -d)
gcc -O2 -w -o "$T/rudolf-portal" "$D/rudolf-portal.c" $(pkg-config --cflags --libs gio-2.0 gio-unix-2.0) || exit 1
cat > "$T/t.sh" <<'EOS'
P=0; N=0; ok() { N=$((N + 1)); if [ "$1" = 0 ]; then P=$((P + 1)); echo "PASS $2"; else echo "FAIL $2 - $3"; fi; }
c() { gdbus call --session -d org.freedesktop.portal.Desktop -o /org/freedesktop/portal/desktop -m "$@" 2>&1; }
mkdir -p /run/rudolf/portal; rm -f /run/rudolf/proxy
"$T/rudolf-portal" 2>/dev/null & PID=$!; sleep 1
r=$(c org.freedesktop.portal.ProxyResolver.Lookup https://www.roblox.com/); [ "$r" = "(['direct://'],)" ]; ok $? "ProxyResolver.Lookup: a direct connection when no proxy is set" "$r"
echo "proxy.example:3128" > /run/rudolf/proxy
r=$(c org.freedesktop.portal.ProxyResolver.Lookup https://www.roblox.com/); [ "$r" = "(['http://proxy.example:3128'],)" ]; ok $? "...Rudolf OS's proxy (Settings > Network) when one is set" "$r"
echo "socks5://10.0.0.1:1080" > /run/rudolf/proxy
r=$(c org.freedesktop.portal.ProxyResolver.Lookup https://www.roblox.com/); [ "$r" = "(['socks5://10.0.0.1:1080'],)" ]; ok $? "...with its own scheme as it is" "$r"
rm -f /run/rudolf/proxy
r=$(c org.freedesktop.portal.NetworkMonitor.GetStatus); echo "$r" | grep -q "'available': <true>" && echo "$r" | grep -q "'connectivity': <uint32 4>"; ok $? "NetworkMonitor.GetStatus: online, full connectivity" "$r"
r=$(c org.freedesktop.portal.NetworkMonitor.CanReach roblox.com 443); [ "$r" = "(true,)" ]; ok $? "NetworkMonitor.CanReach" "$r"
r=$(c org.freedesktop.DBus.Properties.Get org.freedesktop.portal.ProxyResolver version); [ "$r" = "(<uint32 1>,)" ]; ok $? "versions: ProxyResolver 1" "$r"
r=$(c org.freedesktop.DBus.Properties.Get org.freedesktop.portal.NetworkMonitor version); [ "$r" = "(<uint32 3>,)" ]; ok $? "...NetworkMonitor 3" "$r"
r=$(c org.freedesktop.portal.Settings.ReadOne org.freedesktop.appearance color-scheme); echo "$r" | grep -q uint32; ok $? "Settings still answer (colour scheme)" "$r"
kill $PID; echo "$P/$N passed"; [ $P = $N ]
EOS
T="$T" dbus-run-session -- sh "$T/t.sh" 2>&1 | grep -v "dbus-daemon"; r=$?; rm -rf "$T"; exit $r
