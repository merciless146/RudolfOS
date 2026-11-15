/* qt/host/inhibit60_t.sh: rudolfd's inhibit_scan, compiled from rudolfd.c (main renamed), on -DINHIBIT_DIR: prints
   "idle N sleep N by [..]" */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
int main(void) { int i, s; str a = {0}; sadd(&a, "%s", ""); inhibit_scan(&i, &s, &a); printf("idle %d sleep %d by [%s]\n", i, s, a.b); return 0; }
