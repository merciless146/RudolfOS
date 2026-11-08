/* 1.5.5: rudolfd's checks for the per-account Downloads folders, compiled from rudolfd.c itself (main renamed) */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strncmp(a, "acct:", 5)) printf("%s %d\n", a, dl_acct_ok(a + 5));
		else printf("%s %d\n", a, dl_path_ok(a));
	}
	return 0;
}
