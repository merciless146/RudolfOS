/* 1.6.0 (plan Part 8): rudolfd's check of a printer's address (print_uri_ok), compiled from rudolfd.c itself (main
   renamed), as deskapi.c.   printuri URI...   one line each: "1 URI" (accepted) or "0 URI" */
#define main rudolfd_main
#include "../../../sys/rudolfd.c"
#undef main
int main(int argc, char **argv) { for (int i = 1; i < argc; i++) printf("%d %s\n", print_uri_ok(argv[i]), argv[i]); return 0; }
