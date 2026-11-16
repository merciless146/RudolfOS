/* Rudolf OS 1.6.0 (docs/plan-1.6.0.md, "Cellular and ARM64", stage A): a stand-in ModemManager for the tests. It owns
   org.freedesktop.ModemManager1 on the bus DBUS_SYSTEM_BUS_ADDRESS names (a private dbus-daemon: never the computer's
   own system bus) and implements, with ModemManager's own generated D-Bus code (libmm-glib's skeletons), the parts of
   its API Rudolf OS uses: the manager (with its object manager), modems with a SIM (locked with a PIN, the PUK after
   three wrong PINs, blocked after ten wrong PUKs), the 3GPP registration (home, roaming, searching, denied), the
   signal (quality, and LTE/5G details), the network types (allowed and preferred modes), bearers (Simple.Connect and
   Disconnect: the APN checked letter for letter when the modem is given one, roaming refused unless allowed), the
   messages (incoming, created and sent, multi-part counted, delivery reports), an FCC-locked radio, and a modem that
   disappears. Real mmcli and Rudolf's scripts talk to it as to ModemManager.
     mmstub --ctl FIFO [--log FILE] [--init FILE]
   FIFO (and FILE given with --init): one command per line
     add ID [key=value ...]    a modem (keys below)        remove ID   it disappears (unplugged)
     set ID key=value ...      change it                   sms ID NUMBER TEXT...   an incoming message
     quit
   (a value can carry %20 for a space, as in a URL)
   keys: maker model rev device (its sysfs path) drivers plugin imei port (its primary port) iface (the bearer's network
   interface) sim=0|1 pin puk (the SIM's codes; with a PIN the SIM starts locked) mccmnc opname spn iccid imsi own
   reg=home|roaming|searching|denied|idle tech=lte|5gnr|umts|hspa|edge signal=0-100 rsrp rsrq sinr rssi (LTE's, in dB)
   nr=0|1 (5G details too) modes=2g,3g,4g,5g (supported) apn (the only APN the network accepts) ip4 gw4 dns ip6 gw6
   dns6 (the bearer's addresses: ip6 only for an IPv6-only network) fcc=1|0 (the radio is FCC-locked: enabling fails
   until it is unlocked) failed=sim-missing|sim-error|unknown report=0|1 (delivery reports come back) rx tx (the
   bearer's byte counters)
   The log (--log): each call Rudolf made, one per line: "ID Enable 1", "ID SendPin 1234", "ID Connect apn=... ip-type=...
   allow-roaming=0", "ID Disconnect", "ID SetCurrentModes 12 8", "ID Create number=... text-length=N", "ID Send parts=N",
   "ID Delete N", "ID SignalSetup 5", "ID SetPowerState 1", "ID SetInitialEpsBearerSettings apn=...", "ScanDevices".
   GPL-2.0-or-later. Build: gcc -O2 -o mmstub mmstub.c $(pkg-config --cflags --libs mm-glib gio-unix-2.0) */
#include <libmm-glib.h>
#include <gio/gio.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAXM 8
#define MAXS 64
typedef struct {
	int on, id;
	MmGdbusObjectSkeleton *obj;
	MmGdbusModem *m; MmGdbusModem3gpp *g; MmGdbusModemSimple *s; MmGdbusModemSignal *sig; MmGdbusModemMessaging *msg;
	MmGdbusSim *sim; MmGdbusBearer *bearer;
	char path[96], simpath[96], bpath[96];
	char pin[16], puk[16], apn[96], iface[32], ip4[64], gw4[64], dns[96], ip6[96], gw6[96], dns6[128], mccmnc[8], opname[64];
	int haspin, pinleft, pukleft, locked, simon, enabled, fcc, report, reg, tech, signal, connected, nr;
	double rsrp, rsrq, sinr, rssi; guint64 rx, tx;
	guint32 allowed, preferred, supported;
} Modem;
typedef struct { int on, modem; MmGdbusSms *s; char path[96]; } Sms;
static Modem M[MAXM]; static Sms S[MAXS]; static int nsms, nbear;
static GDBusConnection *BUS; static GDBusObjectManagerServer *OM; static MmGdbusOrgFreedesktopModemManager1 *MGR;
static FILE *LOG; static const char *CTL; static GMainLoop *LOOP;

static void logf_(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
static void logf_(const char *fmt, ...) { if (!LOG) return; va_list a; va_start(a, fmt); vfprintf(LOG, fmt, a); va_end(a); fputc('\n', LOG); fflush(LOG); }
static Modem *by_obj(gpointer o) { for (int i = 0; i < MAXM; i++) if (M[i].on && ((gpointer)M[i].m == o || (gpointer)M[i].g == o || (gpointer)M[i].s == o || (gpointer)M[i].sig == o || (gpointer)M[i].msg == o || (gpointer)M[i].sim == o || (gpointer)M[i].bearer == o)) return &M[i]; return NULL; }
static Modem *by_id(int id) { for (int i = 0; i < MAXM; i++) if (M[i].on && M[i].id == id) return &M[i]; return NULL; }

static guint32 tech_bits(int t) { switch (t) { case 1: return MM_MODEM_ACCESS_TECHNOLOGY_LTE; case 2: return MM_MODEM_ACCESS_TECHNOLOGY_5GNR | MM_MODEM_ACCESS_TECHNOLOGY_LTE; case 3: return MM_MODEM_ACCESS_TECHNOLOGY_UMTS; case 4: return MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS; case 5: return MM_MODEM_ACCESS_TECHNOLOGY_EDGE; } return 0; }
static int tech_of(const char *v) { return !strcmp(v, "lte") ? 1 : !strcmp(v, "5gnr") ? 2 : !strcmp(v, "umts") ? 3 : !strcmp(v, "hspa") ? 4 : !strcmp(v, "edge") ? 5 : 0; }
static int reg_of(const char *v) { return !strcmp(v, "home") ? MM_MODEM_3GPP_REGISTRATION_STATE_HOME : !strcmp(v, "roaming") ? MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING : !strcmp(v, "searching") ? MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING : !strcmp(v, "denied") ? MM_MODEM_3GPP_REGISTRATION_STATE_DENIED : MM_MODEM_3GPP_REGISTRATION_STATE_IDLE; }
static int registered(Modem *x) { return x->reg == MM_MODEM_3GPP_REGISTRATION_STATE_HOME || x->reg == MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING; }
static guint32 modes_of(const char *v) { guint32 r = 0; if (strstr(v, "2g")) r |= MM_MODEM_MODE_2G; if (strstr(v, "3g")) r |= MM_MODEM_MODE_3G; if (strstr(v, "4g")) r |= MM_MODEM_MODE_4G; if (strstr(v, "5g")) r |= MM_MODEM_MODE_5G; return r; }

/* the modem's properties from its state (as ModemManager keeps them) */
static void sync_unlock(Modem *x)
{
	GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a{uu}"));
	if (x->simon && x->haspin) { g_variant_builder_add(&b, "{uu}", MM_MODEM_LOCK_SIM_PIN, x->pinleft); g_variant_builder_add(&b, "{uu}", MM_MODEM_LOCK_SIM_PUK, x->pukleft); }
	mm_gdbus_modem_set_unlock_retries(x->m, g_variant_builder_end(&b));
	mm_gdbus_modem_set_unlock_required(x->m, !x->simon ? MM_MODEM_LOCK_UNKNOWN : x->locked == 1 ? MM_MODEM_LOCK_SIM_PIN : x->locked == 2 ? MM_MODEM_LOCK_SIM_PUK : MM_MODEM_LOCK_NONE);
}
static void sync_state(Modem *x)
{
	int st; guint fr = MM_MODEM_STATE_FAILED_REASON_NONE;
	if (!x->simon) { st = MM_MODEM_STATE_FAILED; fr = MM_MODEM_STATE_FAILED_REASON_SIM_MISSING; }
	else if (x->locked == 3) { st = MM_MODEM_STATE_FAILED; fr = MM_MODEM_STATE_FAILED_REASON_SIM_ERROR; }
	else if (x->locked) st = MM_MODEM_STATE_LOCKED;
	else if (!x->enabled) st = MM_MODEM_STATE_DISABLED;
	else if (x->connected) st = MM_MODEM_STATE_CONNECTED;
	else if (registered(x)) st = MM_MODEM_STATE_REGISTERED;
	else if (x->reg == MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING) st = MM_MODEM_STATE_SEARCHING;
	else st = MM_MODEM_STATE_ENABLED;
	if ((int)mm_gdbus_modem_get_state(x->m) != st) {
		int old = mm_gdbus_modem_get_state(x->m); mm_gdbus_modem_set_state(x->m, st);
		mm_gdbus_modem_emit_state_changed(x->m, old, st, MM_MODEM_STATE_CHANGE_REASON_UNKNOWN);
	}
	mm_gdbus_modem_set_state_failed_reason(x->m, fr);
	int on = x->enabled && x->simon && !x->locked;
	mm_gdbus_modem_set_access_technologies(x->m, on && registered(x) ? tech_bits(x->tech) : 0);
	mm_gdbus_modem_set_signal_quality(x->m, g_variant_new("(ub)", on && registered(x) ? (guint)x->signal : 0, TRUE));
	mm_gdbus_modem_set_power_state(x->m, x->enabled ? MM_MODEM_POWER_STATE_ON : MM_MODEM_POWER_STATE_LOW);
	mm_gdbus_modem3gpp_set_registration_state(x->g, on ? x->reg : MM_MODEM_3GPP_REGISTRATION_STATE_IDLE);
	mm_gdbus_modem3gpp_set_operator_code(x->g, on && registered(x) ? x->mccmnc : "");
	mm_gdbus_modem3gpp_set_operator_name(x->g, on && registered(x) ? x->opname : "");
	mm_gdbus_modem3gpp_set_enabled_facility_locks(x->g, x->haspin ? MM_MODEM_3GPP_FACILITY_SIM : MM_MODEM_3GPP_FACILITY_NONE);
	mm_gdbus_modem_set_sim(x->m, x->simon ? x->simpath : "/");
	const char *slots[] = {x->simon ? x->simpath : "/", NULL}; mm_gdbus_modem_set_sim_slots(x->m, slots);
	mm_gdbus_modem_set_current_modes(x->m, g_variant_new("(uu)", x->allowed, x->preferred));
	sync_unlock(x);
	/* the signal's details (Signal.Setup asked for them) */
	if (on && registered(x) && mm_gdbus_modem_signal_get_rate(x->sig)) {
		GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
		g_variant_builder_add(&b, "{sv}", "rssi", g_variant_new_double(x->rssi)); g_variant_builder_add(&b, "{sv}", "rsrp", g_variant_new_double(x->rsrp));
		g_variant_builder_add(&b, "{sv}", "rsrq", g_variant_new_double(x->rsrq)); g_variant_builder_add(&b, "{sv}", "snr", g_variant_new_double(x->sinr));
		mm_gdbus_modem_signal_set_lte(x->sig, g_variant_builder_end(&b));
		g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
		if (x->nr) { g_variant_builder_add(&b, "{sv}", "rsrp", g_variant_new_double(x->rsrp + 3)); g_variant_builder_add(&b, "{sv}", "rsrq", g_variant_new_double(x->rsrq)); g_variant_builder_add(&b, "{sv}", "snr", g_variant_new_double(x->sinr + 2)); }
		mm_gdbus_modem_signal_set_nr5g(x->sig, g_variant_builder_end(&b));
	} else {
		mm_gdbus_modem_signal_set_lte(x->sig, g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0));
		mm_gdbus_modem_signal_set_nr5g(x->sig, g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0));
	}
	if (x->bearer) {
		mm_gdbus_bearer_set_connected(x->bearer, x->connected);
		GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
		g_variant_builder_add(&b, "{sv}", "rx-bytes", g_variant_new_uint64(x->rx)); g_variant_builder_add(&b, "{sv}", "tx-bytes", g_variant_new_uint64(x->tx));
		mm_gdbus_bearer_set_stats(x->bearer, g_variant_builder_end(&b));
	}
	g_dbus_interface_skeleton_flush(G_DBUS_INTERFACE_SKELETON(x->m));
}

/* ---- the SIM ---- */
static gboolean sim_send_pin(MmGdbusSim *o, GDBusMethodInvocation *inv, const gchar *pin, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; logf_("%d SendPin %s", x->id, pin);
	if (x->locked == 2) { g_dbus_method_invocation_return_error(inv, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_PUK, "SIM PUK required"); return TRUE; }
	if (x->locked != 1) { g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE, "Cannot send PIN: device is not SIM-PIN locked"); return TRUE; }
	if (strcmp(pin, x->pin)) {
		if (--x->pinleft <= 0) { x->pinleft = 0; x->locked = 2; }
		sync_state(x); g_dbus_method_invocation_return_error(inv, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_INCORRECT_PASSWORD, "Incorrect password"); return TRUE;
	}
	x->pinleft = 3; x->locked = 0; sync_state(x); mm_gdbus_sim_complete_send_pin(o, inv); return TRUE;
}
static gboolean sim_send_puk(MmGdbusSim *o, GDBusMethodInvocation *inv, const gchar *puk, const gchar *pin, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; logf_("%d SendPuk %s %s", x->id, puk, pin);
	if (x->locked != 2) { g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE, "Cannot send PUK: device is not SIM-PUK locked"); return TRUE; }
	if (strcmp(puk, x->puk)) {
		if (--x->pukleft <= 0) { x->pukleft = 0; x->locked = 3; }
		sync_state(x); g_dbus_method_invocation_return_error(inv, MM_MOBILE_EQUIPMENT_ERROR, x->locked == 3 ? MM_MOBILE_EQUIPMENT_ERROR_SIM_FAILURE : MM_MOBILE_EQUIPMENT_ERROR_INCORRECT_PASSWORD, x->locked == 3 ? "SIM failure" : "Incorrect password"); return TRUE;
	}
	snprintf(x->pin, sizeof x->pin, "%s", pin); x->pinleft = 3; x->pukleft = 10; x->locked = 0; sync_state(x); mm_gdbus_sim_complete_send_puk(o, inv); return TRUE;
}
static gboolean sim_enable_pin(MmGdbusSim *o, GDBusMethodInvocation *inv, const gchar *pin, gboolean en, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; logf_("%d EnablePin %s %d", x->id, pin, en);
	if (x->haspin && strcmp(pin, x->pin)) { if (--x->pinleft <= 0) { x->pinleft = 0; x->locked = 2; } sync_state(x);
		g_dbus_method_invocation_return_error(inv, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_INCORRECT_PASSWORD, "Incorrect password"); return TRUE; }
	x->haspin = en; if (en) snprintf(x->pin, sizeof x->pin, "%s", pin); x->pinleft = 3; sync_state(x); mm_gdbus_sim_complete_enable_pin(o, inv); return TRUE;
}
static gboolean sim_change_pin(MmGdbusSim *o, GDBusMethodInvocation *inv, const gchar *old, const gchar *nw, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; logf_("%d ChangePin %s %s", x->id, old, nw);
	if (strcmp(old, x->pin)) { if (--x->pinleft <= 0) { x->pinleft = 0; x->locked = 2; } sync_state(x);
		g_dbus_method_invocation_return_error(inv, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_INCORRECT_PASSWORD, "Incorrect password"); return TRUE; }
	snprintf(x->pin, sizeof x->pin, "%s", nw); x->pinleft = 3; sync_state(x); mm_gdbus_sim_complete_change_pin(o, inv); return TRUE;
}

/* ---- the modem ---- */
static gboolean reg_later(gpointer p) { Modem *x = p; if (x->on) sync_state(x); return G_SOURCE_REMOVE; }
static gboolean m_enable(MmGdbusModem *o, GDBusMethodInvocation *inv, gboolean en, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; logf_("%d Enable %d", x->id, en);
	if (!x->simon) { g_dbus_method_invocation_return_error(inv, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_NOT_INSERTED, "SIM not inserted"); return TRUE; }
	if (x->locked) { g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE, "modem in initialization state 'locked': SIM PIN or PUK required"); return TRUE; }
	if (en && x->fcc) { g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_RETRY, "Invalid transition: FCC unlock required (radio is off)"); return TRUE; }
	x->enabled = en; if (!en) x->connected = 0;
	sync_state(x); mm_gdbus_modem_complete_enable(o, inv); return TRUE;
}
static gboolean m_modes(MmGdbusModem *o, GDBusMethodInvocation *inv, GVariant *v, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; guint32 a, p; g_variant_get(v, "(uu)", &a, &p); logf_("%d SetCurrentModes %u %u", x->id, a, p);
	if (!a || (a & ~x->supported)) { g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_UNSUPPORTED, "Requested mode not supported"); return TRUE; }
	x->allowed = a; x->preferred = p; sync_state(x); mm_gdbus_modem_complete_set_current_modes(o, inv); return TRUE;
}
static gboolean m_power(MmGdbusModem *o, GDBusMethodInvocation *inv, guint st, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; logf_("%d SetPowerState %u", x->id, st);
	if (st != MM_MODEM_POWER_STATE_ON) { x->enabled = 0; x->connected = 0; } sync_state(x); mm_gdbus_modem_complete_set_power_state(o, inv); return TRUE;
}
static gboolean m_list_bearers(MmGdbusModem *o, GDBusMethodInvocation *inv, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; const char *b[] = {x->bearer ? x->bpath : NULL, NULL};
	mm_gdbus_modem_complete_list_bearers(o, inv, b); return TRUE;
}
static void ipcfg(Modem *x)
{
	GVariantBuilder b; int v6only = !x->ip4[0] && x->ip6[0];
	g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
	if (x->ip4[0]) { char a[64]; snprintf(a, sizeof a, "%s", x->ip4); char *sl = strchr(a, '/'); int pf = 30; if (sl) { *sl = 0; pf = atoi(sl + 1); }
		g_variant_builder_add(&b, "{sv}", "method", g_variant_new_uint32(MM_BEARER_IP_METHOD_STATIC));
		g_variant_builder_add(&b, "{sv}", "address", g_variant_new_string(a)); g_variant_builder_add(&b, "{sv}", "prefix", g_variant_new_uint32(pf));
		if (x->gw4[0]) g_variant_builder_add(&b, "{sv}", "gateway", g_variant_new_string(x->gw4));
		char d[96]; snprintf(d, sizeof d, "%s", x->dns); char *c = strchr(d, ','); if (c) *c++ = 0;
		if (d[0]) g_variant_builder_add(&b, "{sv}", "dns1", g_variant_new_string(d)); if (c && *c) g_variant_builder_add(&b, "{sv}", "dns2", g_variant_new_string(c));
		g_variant_builder_add(&b, "{sv}", "mtu", g_variant_new_uint32(1500)); }
	else g_variant_builder_add(&b, "{sv}", "method", g_variant_new_uint32(MM_BEARER_IP_METHOD_UNKNOWN));
	mm_gdbus_bearer_set_ip4_config(x->bearer, g_variant_builder_end(&b));
	g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
	if (x->ip6[0]) { char a[96]; snprintf(a, sizeof a, "%s", x->ip6); char *sl = strchr(a, '/'); int pf = 64; if (sl) { *sl = 0; pf = atoi(sl + 1); }
		g_variant_builder_add(&b, "{sv}", "method", g_variant_new_uint32(MM_BEARER_IP_METHOD_STATIC));
		g_variant_builder_add(&b, "{sv}", "address", g_variant_new_string(a)); g_variant_builder_add(&b, "{sv}", "prefix", g_variant_new_uint32(pf));
		if (x->gw6[0]) g_variant_builder_add(&b, "{sv}", "gateway", g_variant_new_string(x->gw6));
		if (x->dns6[0]) { char d[128]; snprintf(d, sizeof d, "%s", x->dns6); char *c = strchr(d, ','); if (c) *c++ = 0; g_variant_builder_add(&b, "{sv}", "dns1", g_variant_new_string(d)); if (c && *c) g_variant_builder_add(&b, "{sv}", "dns2", g_variant_new_string(c)); } }
	else g_variant_builder_add(&b, "{sv}", "method", g_variant_new_uint32(MM_BEARER_IP_METHOD_UNKNOWN));
	mm_gdbus_bearer_set_ip6_config(x->bearer, g_variant_builder_end(&b));
	(void)v6only;
}
static gboolean s_connect(MmGdbusModemSimple *o, GDBusMethodInvocation *inv, GVariant *props, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE;
	const char *apn = "", *ipt = ""; gboolean roam = TRUE; GVariant *v;
	if ((v = g_variant_lookup_value(props, "apn", G_VARIANT_TYPE_STRING))) { apn = g_variant_get_string(v, NULL); }
	GVariant *it = g_variant_lookup_value(props, "ip-type", G_VARIANT_TYPE_UINT32); guint ipv = it ? g_variant_get_uint32(it) : 0;
	ipt = ipv == MM_BEARER_IP_FAMILY_IPV4 ? "ipv4" : ipv == MM_BEARER_IP_FAMILY_IPV6 ? "ipv6" : ipv == MM_BEARER_IP_FAMILY_IPV4V6 ? "ipv4v6" : "";
	GVariant *ar = g_variant_lookup_value(props, "allow-roaming", G_VARIANT_TYPE_BOOLEAN); if (ar) roam = g_variant_get_boolean(ar);
	logf_("%d Connect apn=%s ip-type=%s allow-roaming=%d", x->id, apn, ipt, roam);
	if (!x->simon) { g_dbus_method_invocation_return_error(inv, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_NOT_INSERTED, "SIM not inserted"); goto out; }
	if (x->locked) { g_dbus_method_invocation_return_error(inv, MM_MOBILE_EQUIPMENT_ERROR, x->locked == 2 ? MM_MOBILE_EQUIPMENT_ERROR_SIM_PUK : MM_MOBILE_EQUIPMENT_ERROR_SIM_PIN, x->locked == 2 ? "SIM PUK required" : "SIM PIN required"); goto out; }
	if (x->fcc) { g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_RETRY, "Invalid transition: FCC unlock required (radio is off)"); goto out; }
	x->enabled = 1;
	if (!registered(x)) { sync_state(x); g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_RETRY, "Network timeout: not registered"); goto out; }
	if (x->reg == MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING && !roam) { sync_state(x); g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_UNAUTHORIZED, "Roaming is not allowed"); goto out; }
	if (x->apn[0] && strcmp(apn, x->apn)) { sync_state(x); g_dbus_method_invocation_return_error(inv, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_MISSING_OR_UNKNOWN_APN, "Missing or unknown APN"); goto out; }
	if (!x->bearer) {
		x->bearer = mm_gdbus_bearer_skeleton_new(); snprintf(x->bpath, sizeof x->bpath, "/org/freedesktop/ModemManager1/Bearer/%d", nbear++);
		g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(x->bearer), BUS, x->bpath, NULL);
		const char *bl[] = {x->bpath, NULL}; mm_gdbus_modem_set_bearers(x->m, bl);
	}
	GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(&b, "{sv}", "apn", g_variant_new_string(apn)); g_variant_builder_add(&b, "{sv}", "allow-roaming", g_variant_new_boolean(roam));
	if (ipv) g_variant_builder_add(&b, "{sv}", "ip-type", g_variant_new_uint32(ipv));
	mm_gdbus_bearer_set_properties(x->bearer, g_variant_builder_end(&b));
	mm_gdbus_bearer_set_interface(x->bearer, x->iface); mm_gdbus_bearer_set_bearer_type(x->bearer, MM_BEARER_TYPE_DEFAULT);
	ipcfg(x); x->connected = 1; sync_state(x);
	mm_gdbus_modem_simple_complete_connect(o, inv, x->bpath);
out:
	if (v) g_variant_unref(v); if (it) g_variant_unref(it); if (ar) g_variant_unref(ar); return TRUE;
}
static gboolean s_disconnect(MmGdbusModemSimple *o, GDBusMethodInvocation *inv, const gchar *path, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; logf_("%d Disconnect", x->id); (void)path;
	x->connected = 0; sync_state(x); mm_gdbus_modem_simple_complete_disconnect(o, inv); return TRUE;
}
static gboolean g_eps(MmGdbusModem3gpp *o, GDBusMethodInvocation *inv, GVariant *st, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; const char *apn = ""; GVariant *v = g_variant_lookup_value(st, "apn", G_VARIANT_TYPE_STRING); if (v) apn = g_variant_get_string(v, NULL);
	logf_("%d SetInitialEpsBearerSettings apn=%s", x->id, apn); mm_gdbus_modem3gpp_set_initial_eps_bearer_settings(o, st);
	if (v) g_variant_unref(v); mm_gdbus_modem3gpp_complete_set_initial_eps_bearer_settings(o, inv); return TRUE;
}
static gboolean sig_setup(MmGdbusModemSignal *o, GDBusMethodInvocation *inv, guint rate, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; logf_("%d SignalSetup %u", x->id, rate); mm_gdbus_modem_signal_set_rate(o, rate); sync_state(x);
	mm_gdbus_modem_signal_complete_setup(o, inv); return TRUE;
}

/* ---- messages ---- */
static int gsm7(const char *t) { for (const unsigned char *p = (const unsigned char *)t; *p; p++) if (*p >= 0x80 || *p == '`') return 0; return 1; }
static long nchars(const char *t) { long n = 0; for (const unsigned char *p = (const unsigned char *)t; *p; p++) if ((*p & 0xc0) != 0x80) n += (*p >= 0xf0) ? 2 : 1; return n; }
static void sms_list_sync(Modem *x)
{
	const char *l[MAXS + 1]; int n = 0; for (int i = 0; i < nsms; i++) if (S[i].on && S[i].modem == x->id) l[n++] = S[i].path; l[n] = NULL;
	mm_gdbus_modem_messaging_set_messages(x->msg, l);
}
static gboolean sms_report(gpointer p)
{
	Sms *s = p; if (!s->on) return G_SOURCE_REMOVE;
	mm_gdbus_sms_set_delivery_state(s->s, MM_SMS_DELIVERY_STATE_COMPLETED_RECEIVED);
	GDateTime *now = g_date_time_new_now_utc(); char *ts = g_date_time_format(now, "%Y-%m-%dT%H:%M:%S+00:00"); mm_gdbus_sms_set_discharge_timestamp(s->s, ts); g_free(ts); g_date_time_unref(now);
	g_dbus_interface_skeleton_flush(G_DBUS_INTERFACE_SKELETON(s->s)); return G_SOURCE_REMOVE;
}
static gboolean sms_send(MmGdbusSms *o, GDBusMethodInvocation *inv, gpointer u)
{
	(void)u; Sms *s = NULL; for (int i = 0; i < nsms; i++) if (S[i].on && S[i].s == o) s = &S[i]; if (!s) return FALSE;
	Modem *x = by_id(s->modem); const char *t = mm_gdbus_sms_get_text(o); if (!t) t = "";
	long n = nchars(t); int g = gsm7(t); int parts = g ? (n <= 160 ? 1 : (int)((n + 152) / 153)) : (n <= 70 ? 1 : (int)((n + 66) / 67));
	logf_("%d Send parts=%d", s->modem, parts);
	if (!x || !x->enabled || !registered(x)) { g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE, "Cannot send SMS: not registered"); return TRUE; }
	mm_gdbus_sms_set_state(o, MM_SMS_STATE_SENT); mm_gdbus_sms_set_message_reference(o, 40 + (int)(s - S));
	if (mm_gdbus_sms_get_delivery_report_request(o) && x->report) g_timeout_add(600, sms_report, s);
	mm_gdbus_sms_complete_send(o, inv); return TRUE;
}
static gboolean sms_store(MmGdbusSms *o, GDBusMethodInvocation *inv, guint st, gpointer u) { (void)u; (void)st; mm_gdbus_sms_set_state(o, MM_SMS_STATE_STORED); mm_gdbus_sms_complete_store(o, inv); return TRUE; }
static Sms *sms_new(Modem *x, const char *num, const char *text, int incoming, gboolean rep)
{
	if (nsms >= MAXS) return NULL; Sms *s = &S[nsms]; s->on = 1; s->modem = x->id; s->s = mm_gdbus_sms_skeleton_new();
	snprintf(s->path, sizeof s->path, "/org/freedesktop/ModemManager1/SMS/%d", nsms); nsms++;
	mm_gdbus_sms_set_number(s->s, num); mm_gdbus_sms_set_text(s->s, text);
	mm_gdbus_sms_set_pdu_type(s->s, incoming ? MM_SMS_PDU_TYPE_DELIVER : MM_SMS_PDU_TYPE_SUBMIT);
	mm_gdbus_sms_set_state(s->s, incoming ? MM_SMS_STATE_RECEIVED : MM_SMS_STATE_UNKNOWN); mm_gdbus_sms_set_storage(s->s, MM_SMS_STORAGE_ME);
	mm_gdbus_sms_set_delivery_report_request(s->s, rep); mm_gdbus_sms_set_delivery_state(s->s, MM_SMS_DELIVERY_STATE_UNKNOWN);
	mm_gdbus_sms_set_smsc(s->s, "+12063130004");
	mm_gdbus_sms_set_validity(s->s, g_variant_new("(uv)", MM_SMS_VALIDITY_TYPE_UNKNOWN, g_variant_new_uint32(0)));
	mm_gdbus_sms_set_data(s->s, g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, "", 0, 1));
	GDateTime *now = g_date_time_new_now_utc(); char *ts = g_date_time_format(now, "%Y-%m-%dT%H:%M:%S+00:00"); mm_gdbus_sms_set_timestamp(s->s, incoming ? ts : ""); g_free(ts); g_date_time_unref(now);
	g_signal_connect(s->s, "handle-send", G_CALLBACK(sms_send), NULL); g_signal_connect(s->s, "handle-store", G_CALLBACK(sms_store), NULL);
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(s->s), BUS, s->path, NULL);
	sms_list_sync(x); mm_gdbus_modem_messaging_emit_added(x->msg, s->path, incoming);
	return s;
}
static gboolean msg_create(MmGdbusModemMessaging *o, GDBusMethodInvocation *inv, GVariant *props, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; const char *num = "", *text = ""; gboolean rep = FALSE;
	GVariant *a = g_variant_lookup_value(props, "number", G_VARIANT_TYPE_STRING), *b = g_variant_lookup_value(props, "text", G_VARIANT_TYPE_STRING), *c = g_variant_lookup_value(props, "delivery-report-request", G_VARIANT_TYPE_BOOLEAN);
	if (a) num = g_variant_get_string(a, NULL); if (b) text = g_variant_get_string(b, NULL); if (c) rep = g_variant_get_boolean(c);
	logf_("%d Create number=%s text-length=%ld report=%d", x->id, num, nchars(text), rep);
	if (!*num || !b) { g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_INVALID_ARGS, "Cannot create SMS: mandatory parameter missing"); }
	else { Sms *s = sms_new(x, num, text, 0, rep); if (s) mm_gdbus_modem_messaging_complete_create(o, inv, s->path); else g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_TOO_MANY, "storage full"); }
	if (a) g_variant_unref(a); if (b) g_variant_unref(b); if (c) g_variant_unref(c); return TRUE;
}
static gboolean msg_list(MmGdbusModemMessaging *o, GDBusMethodInvocation *inv, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; const char *l[MAXS + 1]; int n = 0; for (int i = 0; i < nsms; i++) if (S[i].on && S[i].modem == x->id) l[n++] = S[i].path; l[n] = NULL;
	mm_gdbus_modem_messaging_complete_list(o, inv, l); return TRUE;
}
static gboolean msg_delete(MmGdbusModemMessaging *o, GDBusMethodInvocation *inv, const gchar *path, gpointer u)
{
	Modem *x = by_obj(o); (void)u; if (!x) return FALSE; const char *n = strrchr(path, '/'); logf_("%d Delete %s", x->id, n ? n + 1 : path);
	for (int i = 0; i < nsms; i++) if (S[i].on && !strcmp(S[i].path, path)) { S[i].on = 0; g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(S[i].s)); g_object_unref(S[i].s); S[i].s = NULL;
		sms_list_sync(x); mm_gdbus_modem_messaging_emit_deleted(o, path); mm_gdbus_modem_messaging_complete_delete(o, inv); return TRUE; }
	g_dbus_method_invocation_return_error(inv, MM_CORE_ERROR, MM_CORE_ERROR_NOT_FOUND, "No SMS found with path '%s'", path); return TRUE;
}

/* ---- the manager ---- */
static gboolean mgr_scan(MmGdbusOrgFreedesktopModemManager1 *o, GDBusMethodInvocation *inv, gpointer u) { (void)u; logf_("ScanDevices"); mm_gdbus_org_freedesktop_modem_manager1_complete_scan_devices(o, inv); return TRUE; }
static gboolean mgr_log(MmGdbusOrgFreedesktopModemManager1 *o, GDBusMethodInvocation *inv, const gchar *l, gpointer u) { (void)u; logf_("SetLogging %s", l); mm_gdbus_org_freedesktop_modem_manager1_complete_set_logging(o, inv); return TRUE; }
static gboolean mgr_kev(MmGdbusOrgFreedesktopModemManager1 *o, GDBusMethodInvocation *inv, GVariant *p, gpointer u) { (void)u; (void)p; logf_("ReportKernelEvent"); mm_gdbus_org_freedesktop_modem_manager1_complete_report_kernel_event(o, inv); return TRUE; }

/* ---- the commands ---- */
static void setkv(Modem *x, const char *k, const char *v)
{
#define S_(f) snprintf(x->f, sizeof x->f, "%s", v)
	if (!strcmp(k, "maker")) mm_gdbus_modem_set_manufacturer(x->m, v);
	else if (!strcmp(k, "model")) mm_gdbus_modem_set_model(x->m, v);
	else if (!strcmp(k, "rev")) mm_gdbus_modem_set_revision(x->m, v);
	else if (!strcmp(k, "device")) mm_gdbus_modem_set_device(x->m, v);
	else if (!strcmp(k, "plugin")) mm_gdbus_modem_set_plugin(x->m, v);
	else if (!strcmp(k, "drivers")) { gchar **d = g_strsplit(v, ",", 0); mm_gdbus_modem_set_drivers(x->m, (const gchar *const *)d); g_strfreev(d); }
	else if (!strcmp(k, "imei")) { mm_gdbus_modem_set_equipment_identifier(x->m, v); mm_gdbus_modem3gpp_set_imei(x->g, v); }
	else if (!strcmp(k, "port")) { mm_gdbus_modem_set_primary_port(x->m, v);
		GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a(su)")); g_variant_builder_add(&b, "(su)", v, !strncmp(v, "cdc-wdm", 7) ? MM_MODEM_PORT_TYPE_QMI : !strncmp(v, "wwan", 4) ? MM_MODEM_PORT_TYPE_MBIM : MM_MODEM_PORT_TYPE_AT);
		g_variant_builder_add(&b, "(su)", x->iface, MM_MODEM_PORT_TYPE_NET); mm_gdbus_modem_set_ports(x->m, g_variant_builder_end(&b)); }
	else if (!strcmp(k, "iface")) { S_(iface); if (x->bearer) mm_gdbus_bearer_set_interface(x->bearer, v); }
	else if (!strcmp(k, "sim")) { int on = atoi(v); if (on && !x->simon && x->haspin) { x->locked = 1; x->pinleft = 3; } if (!on) { x->connected = 0; x->enabled = 0; } x->simon = on; }
	else if (!strcmp(k, "pin")) { S_(pin); x->haspin = !!*v; x->locked = x->haspin; x->pinleft = 3; }
	else if (!strcmp(k, "puk")) S_(puk);
	else if (!strcmp(k, "mccmnc")) { S_(mccmnc); mm_gdbus_sim_set_operator_identifier(x->sim, v); }
	else if (!strcmp(k, "opname")) { S_(opname); }
	else if (!strcmp(k, "spn")) mm_gdbus_sim_set_operator_name(x->sim, v);
	else if (!strcmp(k, "iccid")) mm_gdbus_sim_set_sim_identifier(x->sim, v);
	else if (!strcmp(k, "imsi")) mm_gdbus_sim_set_imsi(x->sim, v);
	else if (!strcmp(k, "own")) { const char *o[] = {v, NULL}; mm_gdbus_modem_set_own_numbers(x->m, o); }
	else if (!strcmp(k, "reg")) { x->reg = reg_of(v); if (!registered(x)) x->connected = 0; }
	else if (!strcmp(k, "tech")) x->tech = tech_of(v);
	else if (!strcmp(k, "signal")) x->signal = atoi(v);
	else if (!strcmp(k, "rsrp")) x->rsrp = atof(v); else if (!strcmp(k, "rsrq")) x->rsrq = atof(v); else if (!strcmp(k, "sinr")) x->sinr = atof(v); else if (!strcmp(k, "rssi")) x->rssi = atof(v);
	else if (!strcmp(k, "nr")) x->nr = atoi(v);
	else if (!strcmp(k, "modes")) { x->supported = modes_of(v); x->allowed = x->supported; x->preferred = MM_MODEM_MODE_NONE;
		GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a(uu)")); guint32 s = x->supported;
		g_variant_builder_add(&b, "(uu)", s, (guint32)MM_MODEM_MODE_NONE);
		if (s & MM_MODEM_MODE_4G) g_variant_builder_add(&b, "(uu)", (guint32)MM_MODEM_MODE_4G, (guint32)MM_MODEM_MODE_NONE);
		if ((s & MM_MODEM_MODE_5G) && (s & MM_MODEM_MODE_4G)) { g_variant_builder_add(&b, "(uu)", (guint32)(MM_MODEM_MODE_4G | MM_MODEM_MODE_5G), (guint32)MM_MODEM_MODE_5G); g_variant_builder_add(&b, "(uu)", (guint32)MM_MODEM_MODE_5G, (guint32)MM_MODEM_MODE_NONE); }
		if ((s & MM_MODEM_MODE_3G) && (s & MM_MODEM_MODE_4G)) g_variant_builder_add(&b, "(uu)", (guint32)(MM_MODEM_MODE_3G | MM_MODEM_MODE_4G), (guint32)MM_MODEM_MODE_4G);
		if (s & MM_MODEM_MODE_3G) g_variant_builder_add(&b, "(uu)", (guint32)MM_MODEM_MODE_3G, (guint32)MM_MODEM_MODE_NONE);
		mm_gdbus_modem_set_supported_modes(x->m, g_variant_builder_end(&b)); }
	else if (!strcmp(k, "apn")) S_(apn);
	else if (!strcmp(k, "ip4")) S_(ip4); else if (!strcmp(k, "gw4")) S_(gw4); else if (!strcmp(k, "dns")) S_(dns);
	else if (!strcmp(k, "ip6")) S_(ip6); else if (!strcmp(k, "gw6")) S_(gw6); else if (!strcmp(k, "dns6")) S_(dns6);
	else if (!strcmp(k, "fcc")) x->fcc = atoi(v);
	else if (!strcmp(k, "report")) x->report = atoi(v);
	else if (!strcmp(k, "rx")) x->rx = g_ascii_strtoull(v, NULL, 10); else if (!strcmp(k, "tx")) x->tx = g_ascii_strtoull(v, NULL, 10);
	else if (!strcmp(k, "enabled")) x->enabled = atoi(v);
	else fprintf(stderr, "mmstub: unknown key %s\n", k);
#undef S_
}
static void apply(Modem *x, char **w) { for (int i = 0; w[i]; i++) { char *e = strchr(w[i], '='); if (!e) continue; *e = 0; char *v = g_uri_unescape_string(e + 1, NULL); setkv(x, w[i], v ? v : e + 1); g_free(v); *e = '='; } }
static void add_modem(int id, char **w)
{
	if (by_id(id)) { fprintf(stderr, "mmstub: modem %d is there already\n", id); return; }
	Modem *x = NULL; for (int i = 0; i < MAXM; i++) if (!M[i].on) { x = &M[i]; break; } if (!x) return;
	memset(x, 0, sizeof *x); x->on = 1; x->id = id; x->simon = 1; x->pinleft = 3; x->pukleft = 10; x->reg = MM_MODEM_3GPP_REGISTRATION_STATE_HOME; x->tech = 1; x->signal = 70;
	x->rsrp = -95; x->rsrq = -10; x->sinr = 12; x->rssi = -65; x->report = 1;
	snprintf(x->iface, sizeof x->iface, "wwan0"); snprintf(x->ip4, sizeof x->ip4, "10.64.12.34/30"); snprintf(x->gw4, sizeof x->gw4, "10.64.12.33"); snprintf(x->dns, sizeof x->dns, "10.177.0.34,10.177.0.210");
	snprintf(x->mccmnc, sizeof x->mccmnc, "310260"); snprintf(x->opname, sizeof x->opname, "T-Mobile");
	snprintf(x->path, sizeof x->path, "/org/freedesktop/ModemManager1/Modem/%d", id); snprintf(x->simpath, sizeof x->simpath, "/org/freedesktop/ModemManager1/SIM/%d", id);
	x->obj = mm_gdbus_object_skeleton_new(x->path);
	x->m = mm_gdbus_modem_skeleton_new(); x->g = mm_gdbus_modem3gpp_skeleton_new(); x->s = mm_gdbus_modem_simple_skeleton_new();
	x->sig = mm_gdbus_modem_signal_skeleton_new(); x->msg = mm_gdbus_modem_messaging_skeleton_new(); x->sim = mm_gdbus_sim_skeleton_new();
	mm_gdbus_modem_set_manufacturer(x->m, "Quectel"); mm_gdbus_modem_set_model(x->m, "EG25-G"); mm_gdbus_modem_set_revision(x->m, "EG25GGBR07A08M2G");
	mm_gdbus_modem_set_hardware_revision(x->m, "10000"); mm_gdbus_modem_set_plugin(x->m, "quectel");
	const char *drv[] = {"qmi_wwan", "option", NULL}; mm_gdbus_modem_set_drivers(x->m, drv);
	mm_gdbus_modem_set_device(x->m, "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-2"); mm_gdbus_modem_set_physdev(x->m, "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-2");
	mm_gdbus_modem_set_device_identifier(x->m, "7d8e2f1c0a9b3e4d5f6a7b8c9d0e1f2a3b4c5d6e");
	mm_gdbus_modem_set_equipment_identifier(x->m, "861234567890123"); mm_gdbus_modem3gpp_set_imei(x->g, "861234567890123");
	mm_gdbus_modem_set_supported_capabilities(x->m, g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32, (guint32[]){MM_MODEM_CAPABILITY_GSM_UMTS | MM_MODEM_CAPABILITY_LTE}, 1, sizeof(guint32)));
	mm_gdbus_modem_set_current_capabilities(x->m, MM_MODEM_CAPABILITY_GSM_UMTS | MM_MODEM_CAPABILITY_LTE);
	mm_gdbus_modem_set_max_bearers(x->m, 1); mm_gdbus_modem_set_max_active_bearers(x->m, 1);
	mm_gdbus_modem_set_supported_ip_families(x->m, MM_BEARER_IP_FAMILY_IPV4 | MM_BEARER_IP_FAMILY_IPV6 | MM_BEARER_IP_FAMILY_IPV4V6);
	mm_gdbus_modem_set_supported_bands(x->m, g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32, (guint32[]){MM_MODEM_BAND_EUTRAN_2, MM_MODEM_BAND_EUTRAN_4, MM_MODEM_BAND_EUTRAN_12, MM_MODEM_BAND_EUTRAN_66, MM_MODEM_BAND_EUTRAN_71}, 5, sizeof(guint32)));
	mm_gdbus_modem_set_current_bands(x->m, mm_gdbus_modem_get_supported_bands(x->m));
	const char *none[] = {NULL}; mm_gdbus_modem_set_bearers(x->m, none); mm_gdbus_modem_set_own_numbers(x->m, none);
	mm_gdbus_modem_set_primary_sim_slot(x->m, 1);
	mm_gdbus_modem_set_carrier_configuration(x->m, "ROW_Generic_3GPP"); mm_gdbus_modem_set_carrier_configuration_revision(x->m, "05010821");
	mm_gdbus_modem_messaging_set_messages(x->msg, none); mm_gdbus_modem_messaging_set_default_storage(x->msg, MM_SMS_STORAGE_ME);
	mm_gdbus_modem_messaging_set_supported_storages(x->msg, g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32, (guint32[]){MM_SMS_STORAGE_SM, MM_SMS_STORAGE_ME}, 2, sizeof(guint32)));
	mm_gdbus_sim_set_active(x->sim, TRUE); mm_gdbus_sim_set_sim_identifier(x->sim, "8901260123456789012"); mm_gdbus_sim_set_imsi(x->sim, "310260123456789");
	mm_gdbus_sim_set_operator_identifier(x->sim, "310260"); mm_gdbus_sim_set_operator_name(x->sim, "T-Mobile");
	mm_gdbus_sim_set_sim_type(x->sim, MM_SIM_TYPE_PHYSICAL); mm_gdbus_sim_set_removability(x->sim, MM_SIM_REMOVABILITY_REMOVABLE);
	const char *em[] = {"911", "112", NULL}; mm_gdbus_sim_set_emergency_numbers(x->sim, em);
	mm_gdbus_modem3gpp_set_initial_eps_bearer_settings(x->g, g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0));
	char p1[] = "modes=3g,4g", p2[] = "port=cdc-wdm0"; char *pm[] = {p1, p2, NULL}; apply(x, pm);
	apply(x, w);
	g_signal_connect(x->m, "handle-enable", G_CALLBACK(m_enable), NULL); g_signal_connect(x->m, "handle-set-current-modes", G_CALLBACK(m_modes), NULL);
	g_signal_connect(x->m, "handle-set-power-state", G_CALLBACK(m_power), NULL); g_signal_connect(x->m, "handle-list-bearers", G_CALLBACK(m_list_bearers), NULL);
	g_signal_connect(x->s, "handle-connect", G_CALLBACK(s_connect), NULL); g_signal_connect(x->s, "handle-disconnect", G_CALLBACK(s_disconnect), NULL);
	g_signal_connect(x->g, "handle-set-initial-eps-bearer-settings", G_CALLBACK(g_eps), NULL);
	g_signal_connect(x->sig, "handle-setup", G_CALLBACK(sig_setup), NULL);
	g_signal_connect(x->msg, "handle-create", G_CALLBACK(msg_create), NULL); g_signal_connect(x->msg, "handle-list", G_CALLBACK(msg_list), NULL); g_signal_connect(x->msg, "handle-delete", G_CALLBACK(msg_delete), NULL);
	g_signal_connect(x->sim, "handle-send-pin", G_CALLBACK(sim_send_pin), NULL); g_signal_connect(x->sim, "handle-send-puk", G_CALLBACK(sim_send_puk), NULL);
	g_signal_connect(x->sim, "handle-enable-pin", G_CALLBACK(sim_enable_pin), NULL); g_signal_connect(x->sim, "handle-change-pin", G_CALLBACK(sim_change_pin), NULL);
	sync_state(x);
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(x->sim), BUS, x->simpath, NULL);
	mm_gdbus_object_skeleton_set_modem(x->obj, x->m); mm_gdbus_object_skeleton_set_modem3gpp(x->obj, x->g); mm_gdbus_object_skeleton_set_modem_simple(x->obj, x->s);
	mm_gdbus_object_skeleton_set_modem_signal(x->obj, x->sig); mm_gdbus_object_skeleton_set_modem_messaging(x->obj, x->msg);
	g_dbus_object_manager_server_export(OM, G_DBUS_OBJECT_SKELETON(x->obj));
}
static void remove_modem(Modem *x)
{
	g_dbus_object_manager_server_unexport(OM, x->path);
	g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(x->sim));
	if (x->bearer) { g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(x->bearer)); g_object_unref(x->bearer); }
	for (int i = 0; i < nsms; i++) if (S[i].on && S[i].modem == x->id) { S[i].on = 0; g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(S[i].s)); g_object_unref(S[i].s); }
	g_object_unref(x->m); g_object_unref(x->g); g_object_unref(x->s); g_object_unref(x->sig); g_object_unref(x->msg); g_object_unref(x->sim); g_object_unref(x->obj);
	x->on = 0;
}
static void command(char *line)
{
	g_strstrip(line); if (!*line || *line == '#') return;
	if (!strncmp(line, "sms ", 4)) {   /* sms ID NUMBER TEXT (the text: the rest of the line; \n for a new line) */
		int id = atoi(line + 4); char *p = strchr(line + 4, ' '); if (!p) return; p++; char *t = strchr(p, ' '); if (!t) return; *t++ = 0;
		Modem *x = by_id(id); if (!x) { fprintf(stderr, "mmstub: no modem %d\n", id); return; }
		GString *s = g_string_new(NULL); for (const char *c = t; *c; c++) { if (c[0] == '\\' && c[1] == 'n') { g_string_append_c(s, '\n'); c++; } else g_string_append_c(s, *c); }
		sms_new(x, p, s->str, 1, FALSE); g_string_free(s, TRUE); return;
	}
	gchar **w = g_strsplit_set(line, " \t", 0); int n = g_strv_length(w);
	if (!strcmp(w[0], "quit")) g_main_loop_quit(LOOP);
	else if (n >= 2 && !strcmp(w[0], "add")) add_modem(atoi(w[1]), w + 2);
	else if (n >= 2 && !strcmp(w[0], "remove")) { Modem *x = by_id(atoi(w[1])); if (x) remove_modem(x); }
	else if (n >= 3 && !strcmp(w[0], "set")) { Modem *x = by_id(atoi(w[1])); if (x) { apply(x, w + 2); sync_state(x); g_timeout_add(10, reg_later, x); } }
	else fprintf(stderr, "mmstub: what is \"%s\"?\n", line);
	g_strfreev(w);
}
static gboolean on_ctl(GIOChannel *ch, GIOCondition c, gpointer u)
{
	(void)c; (void)u; gchar *line = NULL; gsize n;
	while (g_io_channel_read_line(ch, &line, &n, NULL, NULL) == G_IO_STATUS_NORMAL && line) { command(line); g_free(line); line = NULL; }
	return TRUE;
}
static const char *INIT;
static void on_name(GDBusConnection *c, const gchar *name, gpointer u)
{
	(void)c; (void)name; (void)u;
	if (INIT) { FILE *f = fopen(INIT, "r"); char b[2048]; if (f) { while (fgets(b, sizeof b, f)) command(b); fclose(f); } }
	if (CTL) { int fd = open(CTL, O_RDWR | O_NONBLOCK); if (fd < 0) { perror(CTL); exit(1); }
		GIOChannel *ch = g_io_channel_unix_new(fd); g_io_channel_set_flags(ch, G_IO_FLAG_NONBLOCK, NULL); g_io_add_watch(ch, G_IO_IN, on_ctl, NULL); }
	printf("mmstub: ready\n"); fflush(stdout);
}
static void on_lost(GDBusConnection *c, const gchar *name, gpointer u) { (void)c; (void)u; fprintf(stderr, "mmstub: could not own %s\n", name); exit(1); }
int main(int argc, char **argv)
{
	for (int i = 1; i + 1 < argc; i++) { if (!strcmp(argv[i], "--ctl")) CTL = argv[++i]; else if (!strcmp(argv[i], "--log")) LOG = fopen(argv[++i], "a"); else if (!strcmp(argv[i], "--init")) INIT = argv[++i]; }
	const char *a = getenv("DBUS_SYSTEM_BUS_ADDRESS");
	/* (MMSTUB_TEST_MACHINE=1: inside a throwaway QEMU guest, qt/sb/cell60_q.py, the guest's own system bus is the one) */
	if ((!a || !*a || strstr(a, "/run/dbus/system_bus_socket") || strstr(a, "/var/run/dbus/")) && !getenv("MMSTUB_TEST_MACHINE")) { fprintf(stderr, "mmstub: give it a private bus (DBUS_SYSTEM_BUS_ADDRESS), never the computer's own\n"); return 2; }
	if (CTL) { struct stat st; if (stat(CTL, &st) || !S_ISFIFO(st.st_mode)) { fprintf(stderr, "mmstub: %s is not a FIFO\n", CTL); return 2; } }
	GError *e = NULL; BUS = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &e); if (!BUS) { fprintf(stderr, "mmstub: %s\n", e->message); return 1; }
	OM = g_dbus_object_manager_server_new(MM_DBUS_PATH);
	MGR = mm_gdbus_org_freedesktop_modem_manager1_skeleton_new(); mm_gdbus_org_freedesktop_modem_manager1_set_version(MGR, "1.23.4");
	g_signal_connect(MGR, "handle-scan-devices", G_CALLBACK(mgr_scan), NULL); g_signal_connect(MGR, "handle-set-logging", G_CALLBACK(mgr_log), NULL);
	g_signal_connect(MGR, "handle-report-kernel-event", G_CALLBACK(mgr_kev), NULL);
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(MGR), BUS, MM_DBUS_PATH, NULL);
	g_dbus_object_manager_server_set_connection(OM, BUS);
	g_bus_own_name_on_connection(BUS, MM_DBUS_SERVICE, G_BUS_NAME_OWNER_FLAGS_NONE, on_name, on_lost, NULL, NULL);
	LOOP = g_main_loop_new(NULL, FALSE); g_main_loop_run(LOOP); return 0;
}
