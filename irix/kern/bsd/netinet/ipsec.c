#ifdef INET6
#include <stdio.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/radix.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in6_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ipsec.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip6_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6_icmp.h>
#include <netinet/igmp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp6_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/udp6_var.h>

/* These are place holders until we decide what to do. */

/*ARGSUSED*/
struct ipsec_entry_header *
ipsec_lookup(addr, isl)
        register struct sockaddr_ipsec *addr;
        register struct ip_seclist *isl;
{
	return (0);
}

/*ARGSUSED*/
int    
ipsec_create(struct ipsec_req *a)
{
	return (0);
}

/*ARGSUSED*/
int    
ipsec_change(struct ipsec_req *a)
{
	return (0);
}

/*ARGSUSED*/
int    
ipsec_delete(struct sockaddr_ipsec *a)
{
	return (0);
}

/*ARGSUSED*/
int     
ipsec_attach(struct inpcb *a, struct sockaddr_ipsec *b, int c)
{
	return (0);
}

/*ARGSUSED*/
int     
ipsec_detach(struct inpcb *a, struct sockaddr_ipsec *b, int c)
{
	return (0);
}

/*ARGSUSED*/
int
ipsec_match(struct inpcb *a, struct mbuf *b, struct mbuf *c, int d)
{
	return (0);
}

/*ARGSUSED*/
struct  mbuf *ah6_build(struct mbuf *a, struct mbuf *b, int c)
{
	return (0);
}

/*ARGSUSED*/
struct  mbuf *esp6_build(struct mbuf *a, int b, int c, int d)
{
	return (0);
}

/*ARGSUSED*/
void
ipsec_init(void)
{
}

/*ARGSUSED*/
void
ipsec_duplist(list0, list)
        struct ip_soptions *list0, *list;
{
}
#endif /* INET6 */
