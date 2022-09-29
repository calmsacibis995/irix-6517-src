#include <sys/types.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/mbuf.h>

/* ARGSUSED */
char*
swipe_getopol(struct in_addr src, struct in_addr dst)
{
    return (char*)0;
}
/* ARGSUSED */
int
sw_output(struct ifnet *ifp, struct mbuf *m, char *opol)
{
    return -1;
}
