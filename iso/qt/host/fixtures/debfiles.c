/* 1.5.7: rudolf-debian's pieces for programs that need no installing and for devices, compiled from rudolf-debian.c
   (main renamed): which device names go into Debian, which files may be run, and a launcher's Exec quoting. */
#define main rudolf_debian_main
#include "../../../sys/rudolf-debian.c"
#undef main
int main(int argc, char **argv)
{
	if (argc > 2 && !strcmp(argv[1], "dev")) { for (int i = 2; i < argc; i++) printf("%s=%d\n", argv[i], dev_wanted(argv[i])); return 0; }
	if (argc > 2 && !strcmp(argv[1], "file")) { printf("%d\n", file_ok(argv[2])); return 0; }
	if (argc > 2 && !strcmp(argv[1], "quote")) { char q[2000]; exec_quote(argv[2], q, sizeof q); puts(q); return 0; }
	return 2;
}
