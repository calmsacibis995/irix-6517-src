#ifndef lint
static char RCSid[] = "$Id: audit.c,v 1.5 1997/01/27 00:01:32 chatz Exp $";
#endif

/*
 * Used to check the size and type of kernel data structures used in the
 * "cluster methods".
 *
 * If you need to add checking for "struct foo", ...
 * (a) check all the include files are being #included (beware of nested
 *     structs in particular)
 * (b) add the declaration for ... the naming convention and syntax is
 *     known to ./chktype
 *	static struct foo	_foo;
 * (c) make audit
 * (d) check all fields are well defined
 *     % dbx audit
 *     dbx> whatis _foo
 * (e) and make sure chktype is doing the right thing
 *     % ./chktype | grep foo
 */

#define _KMEMUSER

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <invent.h>
#include <sys/ksa.h>
#include <net/if.h>
#include <sys/mbuf.h>
#include <sys/fs/nfs_stat.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/tcpipstats.h>
#include <sys/ipc.h>
#define _KERNEL
#include <sys/msg.h>
#undef _KERNEL
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/region.h>
#include <sys/swap.h>
#include <sys/var.h>

static struct statfs	_statfs;
static struct getblkstats	_getblkstats;
static inventory_t	_hinv;
static struct ifnet	_ifnet;
static struct igetstats	_igetstats;
static struct minfo	_minfo;
static struct kna	_kna;
static struct msqid_ds	_msqid_ds;
static struct semid_ds	_semid_ds;
static struct shmid_ds	_shmid_ds;
static struct ncstats	_ncstats;
static struct clstat	_clstat;
static struct svstat	_svstat;
static struct rcstat	_rcstat;
static struct rsstat	_rsstat;
static swapent_t	_swapent_t;
static struct syserr	_syserr;
static struct sysinfo	_sysinfo;
static struct var	_var;
static struct vnodestats	_vnodestats;

#define BITSPERBYTE	8

main()
{
    printf("s/unsigned /U/\n");
    printf("s/__uint32_t /U32/\n");
    printf("s/__uint64_t /U64/\n");
    printf("s/long long /%d/\n", BITSPERBYTE * sizeof(long long));
    printf("s/u_long /U%d/\n", BITSPERBYTE * sizeof(u_long));
    printf("s/ulong_t /U%d/\n", BITSPERBYTE * sizeof(ulong_t));
    printf("s/long /%d/\n", BITSPERBYTE * sizeof(long));
    printf("s/u_int /U%d/\n", BITSPERBYTE * sizeof(u_int));
    printf("s/uint_t /U%d/\n", BITSPERBYTE * sizeof(uint_t));
    printf("s/int /%d/\n", BITSPERBYTE * sizeof(int));
    printf("s/u_short /U%d/\n", BITSPERBYTE * sizeof(u_short));
    printf("s/ushort_t /U%d/\n", BITSPERBYTE * sizeof(ushort_t));
    printf("s/short /%d/\n", BITSPERBYTE * sizeof(short));
    printf("s/u_char /U%d/\n", BITSPERBYTE * sizeof(u_char));
    printf("s/uchar_t /U%d/\n", BITSPERBYTE * sizeof(uchar_t));
    printf("s/char /%d/\n", BITSPERBYTE * sizeof(char));
}
