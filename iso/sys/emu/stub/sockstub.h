/* Rudolf OS: DeSmuME's Wi-Fi link to the real network is not used in Emu; these make it compile and
   fail cleanly (the emulated DS sees no access point). */
#include <sys/socket.h>
#include <netinet/in.h>
#ifndef SO_REUSEADDR
#define SO_REUSEADDR 2
#endif
#ifndef SO_BROADCAST
#define SO_BROADCAST 6
#endif
#ifdef __cplusplus
extern "C" {
#endif
int socket(int, int, int);
int bind(int, const struct sockaddr *, socklen_t);
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
ssize_t recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
int setsockopt(int, int, int, const void *, socklen_t);
#ifdef __cplusplus
}
#endif
