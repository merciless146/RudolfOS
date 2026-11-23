/* 1.6.2 (docs/plan-1.6.2.md Part 40): rudolf-debian's message of a program that ended at once (launch_report, as cmd_launch
   calls it once the program has ended), compiled from rudolf-debian.c itself (main renamed), its folder chosen by -DLR_DIR.
   debreport ID NAME EXEC PATH STATUS SECONDS LOG */
#define main rudolf_debian_main
#include "../../../sys/rudolf-debian.c"
#undef main
int main(int argc, char **argv)
{
	if (argc < 8) return 2;
	desk k; memset(&k, 0, sizeof k); snprintf(k.name, sizeof k.name, "%s", argv[2]); snprintf(k.exec, sizeof k.exec, "%s", argv[3]); snprintf(k.path, sizeof k.path, "%s", argv[4]);
	launch_report(argv[1], &k, atoi(argv[5]), atof(argv[6]), argv[7]);
	return 0;
}
