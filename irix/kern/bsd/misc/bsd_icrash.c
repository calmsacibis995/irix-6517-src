#ident "$Revision: 1.7 $"

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/kmem.h"
#include "sys/debug.h"
#include "sys/systm.h"
#include "sys/mbuf.h"
#include "sys/domain.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/tcpipstats.h"
#include "sys/protosw.h"
#include "sys/mbuf_private.h"
#include "net/if_dl.h"
#include "netinet/if_ether.h"
#include <sys/vsocket.h>

/* Structure that contains fields that are pointers to key kernel
 * structures). This forces the type information to be sucked into
 * kernel the symbol table.
 */
typedef struct bsd_icrash_s {
	struct kna  		*bsd_icrash1;
	struct domain		*bas_icrash3;
	struct mbufconst	*bas_icrash4;
	struct vsocket  	*bsd_icrash5;
	vsocket_ops_t   	*bsd_icrash6;
	struct ether_arp   	*bsd_icrash7;
	struct sockaddr_dl  *bsd_icrash8;
	struct mb  			*bsd_icrash9;
} bsd_icrash_t;

bsd_icrash_t *bsd_icrash_struct;

extern struct inpcb tcb, udb;
extern struct rawcb rawcb;
extern struct ifnet *ifnet;
extern struct mbufconst _mbufconst;
extern struct rtstat rtstat;
extern struct radix_node_head *rt_tables[];
extern struct protosw unixsw[];
extern struct unpcb unpcb_list;
extern struct hashtable hashtable_inaddr[];
extern struct hashinfo hashinfo_inaddr;

extern struct strinfo Strinfo[];
extern int str_min_pages, str_page_max, str_page_cur;

extern long *kernel_magic;
extern struct pageconst _pageconst;
extern struct pteconst _pteconst;
extern char *kend;
extern char _physmem_start[];
extern struct var v;

struct bsd_kernaddrs bsd_kernaddrs = {
	(caddr_t)&tcb,
	(caddr_t)&udb,
	(caddr_t)&rawcb,
	(caddr_t)&kptbl,
	(caddr_t)&ifnet,
	(caddr_t)&rt_tables,
	(caddr_t)&rtstat,
	(caddr_t)&unpcb_list,
	(caddr_t)&unixsw,
	(caddr_t)&kernel_magic,
	(caddr_t)0,		/* end; filled in by bsd_init() */
	(caddr_t)&v,
	(caddr_t)&str_page_cur,
	(caddr_t)&str_min_pages,
	(caddr_t)&str_page_max,
	(caddr_t)&Strinfo[0],
	(caddr_t)0,		/* ip_mrtproto; filled in by ip_mroute */
	(caddr_t)0,		/* mfctable; filled in by ip_mroute */
	(caddr_t)0,		/* viftable; filled in by ip_mroute */
	(caddr_t)&_pageconst,
	(caddr_t)&_mbufconst,
	(caddr_t)&hashinfo_inaddr,
	(caddr_t)&hashtable_inaddr,
	(caddr_t)&_physmem_start,
	(caddr_t)&_pteconst,
};

/* Dummy function called by icrash_init() to ensure that symbol
 * information from this module gets included in the kernel
 * symbol table.
 */
void bsd_icrash(void)
{
}
