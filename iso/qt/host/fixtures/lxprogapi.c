/* 1.6.2 (docs/plan-1.6.2.md Part 25b): rudolfd's portal/progress (rudolfd-lxprog.inc), compiled from rudolfd.c itself (main
   renamed), as dlzipapi.c. lxprogapi UID: UID is the signed-in account's user (au_app_user's, set here); prints the body of
   portal/progress's answer. Runs where /run/rudolf may be read (lxprog162_t.sh: a mount namespace of its own). Built with
   -DLXP_EXPORTS='"DIR"' for the exported desktop files. */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
int main(int argc, char **argv)
{
	if (argc < 2) return 2;
	LXP_UID_TEST = atol(argv[1]);
	struct req r; memset(&r, 0, sizeof r); snprintf(r.method, sizeof r.method, "POST"); snprintf(r.path, sizeof r.path, "portal/progress");
	char tf[] = "/tmp/lxprogapi-XXXXXX"; r.fd = mkstemp(tf); unlink(tf);
	api_portal(&r, "progress");
	lseek(r.fd, 0, SEEK_SET); char b[1 << 16]; ssize_t n = read(r.fd, b, sizeof b - 1); close(r.fd); if (n <= 0) return 1; b[n] = 0;
	char *bd = strstr(b, "\r\n\r\n"); if (!bd) return 1;
	fputs(bd + 4, stdout); putchar('\n'); return strncmp(b, "HTTP/1.1 200", 12) ? 1 : 0;
}
