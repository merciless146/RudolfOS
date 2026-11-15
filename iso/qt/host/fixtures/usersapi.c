/* 1.5.9 (plan 15.3): rudolfd's accounts' Linux users, compiled from rudolfd.c itself (main renamed), as deskapi.c.
   usersapi LSFILE ACCOUNTS ETC MAP HOMES RUNUSER COMMAND...: sync | start | session-start ID | session-stop ID |
   appuser | users (GET accounts/users) | uid ID | homehas PATH SUB ; each prints one line. USERS_DISPLAY: a stand-in
   for rudolf-display (--xallow/--xdeny at sign-in and sign-out). Run as root in a private mount namespace. */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
int main(int argc, char **argv)
{
	if (argc < 8) return 2;
	snprintf(desk_ls, sizeof desk_ls, "%s", argv[1]); snprintf(home_root, sizeof home_root, "%s", argv[2]);
	snprintf(au_etc, sizeof au_etc, "%s", argv[3]); snprintf(au_map, sizeof au_map, "%s", argv[4]);
	snprintf(au_homes, sizeof au_homes, "%s", argv[5]); snprintf(au_runuser, sizeof au_runuser, "%s", argv[6]);
	au_on = 1; mkdir(RUN, 0755);
	if (getenv("USERS_DISPLAY")) snprintf(au_display, sizeof au_display, "%s", getenv("USERS_DISPLAY"));
	for (int i = 7; i < argc; i++) {
		const char *c = argv[i];
		if (!strcmp(c, "sync")) printf("sync %d\n", au_sync());
		else if (!strcmp(c, "start")) { au_start(); printf("start\n"); }
		else if (!strcmp(c, "session-start") && i + 1 < argc) { write_str(HOME_SESSION, argv[i + 1]); printf("session-start %d\n", au_session_start(argv[++i])); }
		else if (!strcmp(c, "session-stop") && i + 1 < argc) { au_session_stop(argv[++i]); unlink(HOME_SESSION); printf("session-stop\n"); }
		else if (!strcmp(c, "rebind") && i + 1 < argc) { au_keep = 1; au_session_stop(argv[i + 1]); printf("rebind %d\n", au_session_start(argv[++i])); au_keep = 0; }   /* (as home_bind, rudolfd started again) */
		else if (!strcmp(c, "appuser")) { char u[32], e[160]; au_app_user(u, sizeof u, e, sizeof e); printf("%s %s\n", u, e); }
		else if (!strcmp(c, "homehas") && i + 2 < argc) { printf("homehas %d\n", au_home_has(argv[i + 1], argv[i + 2])); i += 2; }
		else if (!strcmp(c, "uid") && i + 1 < argc) printf("%u\n", au_uid(argv[++i]));
		else if ((!strcmp(c, "keyring-list") || !strcmp(c, "keyring-forget")) && (c[8] == 'l' || i + 1 < argc)) {   /* (1.6.0 Part 14) */
			struct req r; memset(&r, 0, sizeof r); char tf[] = "/tmp/usersapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
			if (c[8] == 'f') snprintf(r.query, sizeof r.query, "id=%s", argv[++i]);
			api_keyring(&r, c + 8, c[8] == 'f'); lseek(r.fd, 0, SEEK_SET); char b[65536]; ssize_t n = read(r.fd, b, sizeof b - 1); b[n > 0 ? n : 0] = 0; close(r.fd);
			char *bd = strstr(b, "\r\n\r\n"); printf("%s\n", bd ? bd + 4 : b);
		}
		else if (!strcmp(c, "users")) {
			struct req r; memset(&r, 0, sizeof r); char tf[] = "/tmp/usersapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
			api_account_users(&r); lseek(r.fd, 0, SEEK_SET); char b[65536]; ssize_t n = read(r.fd, b, sizeof b - 1); b[n > 0 ? n : 0] = 0; close(r.fd);
			char *bd = strstr(b, "\r\n\r\n"); printf("%s\n", bd ? bd + 4 : b);
		}
	}
	return 0;
}
