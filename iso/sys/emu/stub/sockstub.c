/* Rudolf OS: the socket calls DeSmuME's Wi-Fi link would make; all fail (no real network for the emulated DS). */
#include <errno.h>
#include "sockstub.h"
int socket(int a, int b, int c) { (void)a; (void)b; (void)c; errno = ENOSYS; return -1; }
int bind(int a, const struct sockaddr *b, socklen_t c) { (void)a; (void)b; (void)c; errno = ENOSYS; return -1; }
ssize_t sendto(int a, const void *b, size_t c, int d, const struct sockaddr *e, socklen_t f) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; errno = ENOSYS; return -1; }
ssize_t recvfrom(int a, void *b, size_t c, int d, struct sockaddr *e, socklen_t *f) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; errno = ENOSYS; return -1; }
int setsockopt(int a, int b, int c, const void *d, socklen_t e) { (void)a; (void)b; (void)c; (void)d; (void)e; errno = ENOSYS; return -1; }
