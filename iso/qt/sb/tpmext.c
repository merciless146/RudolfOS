/* 1.6.1 (docs/plan-1.6.1.md Part 20), tests only (wk161_t.py): TPM2_PCR_Extend of one PCR's SHA-256 bank through
 * /dev/tpmrm0, standing in for a change of what the firmware measured (Secure Boot's state, its keys). Never in a
 * release.   tpmext PCR BYTE */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char **argv)
{
	if (argc != 3) { fprintf(stderr, "usage: tpmext PCR BYTE\n"); return 2; }
	unsigned pcr = (unsigned)atoi(argv[1]); unsigned char c[96] = {0x80, 0x02, 0, 0, 0, 0, 0, 0, 0x01, 0x82}, *p = c + 10;
	p[0] = 0; p[1] = 0; p[2] = 0; p[3] = (unsigned char)pcr; p += 4;                          /* pcrHandle */
	p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 9; p += 4;                                           /* authorizationSize */
	p[0] = 0x40; p[1] = 0; p[2] = 0; p[3] = 9; p += 4; p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0; p[4] = 0; p += 5;   /* TPM_RS_PW, empty */
	p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 1; p[4] = 0; p[5] = 0x0B; p += 6;                    /* one digest, SHA-256 */
	memset(p, 0, 32); p[31] = (unsigned char)atoi(argv[2]); p += 32;
	unsigned n = (unsigned)(p - c); c[2] = n >> 24; c[3] = n >> 16; c[4] = n >> 8; c[5] = n;
	int f = open(getenv("TPM") ? getenv("TPM") : "/dev/tpmrm0", O_RDWR); if (f < 0) { perror("tpm"); return 1; }
	unsigned char a[96]; if (write(f, c, n) != (ssize_t)n || read(f, a, sizeof a) < 10) { perror("tpm"); return 1; }
	unsigned rc = (unsigned)a[6] << 24 | a[7] << 16 | a[8] << 8 | a[9];
	if (rc) { fprintf(stderr, "PCR_Extend: 0x%x\n", rc); return 1; }
	printf("PCR %u extended\n", pcr); return 0;
}
