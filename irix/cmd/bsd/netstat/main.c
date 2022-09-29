/*
 * Copyright (c) 1983,1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	5.11 (Berkeley) 2/7/88 plus MULTICAST 1.1";
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <nlist.h>
#include <stdio.h>
#include <stdarg.h>
#include <curses.h>
#include <string.h>
#include <bstring.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sgidefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/protosw.h>
#include <sys/syssgi.h>
#include <sys/mbuf.h>
#include <sys/sysmp.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <invent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/tcpipstats.h>
#include <netinet/in_var.h>

#include "netstat.h"

#if (_MIPS_SZLONG == 64)
#define NLIST   nlist64
#define KERNBASE 0 /* 64-bit kernels use all of /dev/kmem */
#else
#define NLIST   nlist
#define KERNBASE 0x80000000
#endif /* if 64bit */

struct mbufconst mbufconst;
struct pteconst pteconst;
struct pageconst pageconst;

int	quitflag = 0;
int	pagesize = 0;
int	pnumshft = 0;
#undef CLBYTES
#define CLBYTES		pagesize
#undef NBPP
#define NBPP		pagesize

/* curses.h defines nl() */
#define	nl	namelist

/*
 * Values to determine which set of network statistics to obtain
 */
struct NLIST nl[] = {
#define	_KA_TCB		0
	{ "tcb" },
#define	_KA_UDB		1
	{ "udb" },
#define	_KA_RAWCB		2
	{ "rawcb" },
#define	_KA_KPTBL		3
	{ "kptbl" },
#define	_KA_IFNET		4
	{ "ifnet" },
#define	_KA_RTREE		5
	{ "rt_tables" },
#define	_KA_RTSTAT	6
	{ "rtstat" },
#define	_KA_FILE		7
	{ "unpcb_list" },
#define	_KA_UNIXSW	8
	{ "unixsw" },
#define _KA_KERNEL_MAGIC	9
	{ "kernel_magic" },
#define _KA_END		10
	{ "end" },
#define	_KA_V		11
	{ "v" },
#define	_KA_STR_CURPAGES	12		/* also defined in stream.c */
	{ "str_page_cur" },
#define	_KA_STR_MINPAGES	13
	{ "str_min_pages" },
#define	_KA_STR_MAXPAGES	14
	{ "str_page_max" },
#define	_KA_STRINFO	15
	{ "Strinfo" },
#define _KA_MRTPROTO	16
	{ "ip_mrtproto" },
#define _KA_MFCTABLE	17
	{ "mfctable" },
#define _KA_VIFTABLE	18
	{ "viftable" },
#define _KA_PAGECONST	19
	{ "_pageconst" },
#define _KA_MBUFCONST	20
	{ "_mbufconst" },
#define _KA_HASHINFO_INADDR 21
	{ "hashinfo_inaddr" },
#define _KA_HASHTABLE_INADDR 22
	{ "hashtable_inaddr" },
#define _KA_MEMBASE	23
	{ "_physmem_start" },
#define _KA_PTECONST	24
	{ "_pteconst" },
	"",
};
#define _KA_MAX		25

static int syspagesize(void);
static int cpuboard(void);

#define NULLPROTOX	((struct protox *) 0)
struct protox {
	u_char	pr_index;		/* index into nlist of cb head */
	u_char	pr_wanted;		/* 1 if wanted, 0 otherwise */
	void	(*pr_cblocks)();	/* control blocks printing routine */
	void	(*pr_stats)(char *);	/* statistics printing routine */
	char	*pr_name;		/* well-known name */
} protox[] = {
	{ _KA_TCB,	1,	protopr,  tcp_stats, "tcp" },
	{ _KA_UDB,	1,	protopr,  udp_stats, "udp" },
	{ -1,		1,	0,	  ip_stats, "ip" },
	{ -1,		1,	0,	  icmp_stats, "icmp" },
	{ -1,		1,	0,	  igmp_stats, "igmp" },
	{ -1,		0,	0,	  0,		0 }
};

static pde_t *Sysmap;
unsigned long mem_offset;	/* for IP20 and 22, which have a hole in memory */
#define system system_name	/* work around 'system()' in stdio.h */
char	*system = "/unix";
char	*kmemf = "/dev/kmem";

int	kmem;
int	kflag;
int	Aflag;
int	Mflag;
int	Cflag;
int     Nflag;
int	aflag;
int	iflag;
#ifdef sgi
int	lflag;
#endif
int	mflag;
int	nflag;
int	pflag;
int	qflag;
int	rflag;
int	sflag;
int	tflag;
int	interval = 0;
char	*interface;
int	unit;
char	usage[] = "[ -AMCailqmnrstu ] [-f family] [-p proto] [-I interface] [ interval ] [ system ] [ core ]";

#ifdef sgi
#ifdef TRIX
mac_label  plabel;
#endif
#endif

int af = AF_UNSPEC;

ns_off_t klseek(int fd, ns_off_t base, int off);

static void chkiface(char *);

static void disp(void);
short havemac = 0;	/* kernel supports Mandatory Access Control */
short havecipso = 0;	/* kernel supports IP security opts */

main(argc, argv)
	int argc;
	char *argv[];
{
	char *cp, *name;
	register struct protoent *p;
	register struct protox *tp;	/* for printing cblocks & stats */
	struct protox *name2protox();	/* for -p */
	ns_off_t kern_end;
	ptrdiff_t ba_addr;
	struct stat sb;

	havemac = (sysconf(_SC_MAC) == 1);
	havecipso = (sysconf(_SC_IP_SECOPTS) == 1);

	syssgi(SGI_CONST, SGICONST_PAGESZ, &pageconst, sizeof(pageconst), 0);
	syssgi(SGI_CONST, SGICONST_MBUF, &mbufconst, sizeof(mbufconst), 0);
	name = argv[0];
	argc--, argv++;
	while (argc > 0 && **argv == '-') {
		for (cp = &argv[0][1]; *cp; cp++)
		switch(*cp) {

		case 'A':
			Aflag++;
			break;

		case 'M':
			Mflag++;
			break;
		case 'C':
			Cflag++;
			break;
		case 'N':
			Nflag++;
			break;
		case 'a':
			aflag++;
			break;

		case 'i':
			iflag++;
			break;

		case 'l':
			if (havecipso)
				lflag++;
			break;

		case 'm':
			mflag++;
			break;

		case 'n':
			nflag++;
			break;
		case 'q':
			qflag++;
			break;
		case 'r':
			rflag++;
			break;

		case 's':
			sflag++;
			break;

		case 't':
			tflag++;
			break;

		case 'u':
			af = AF_UNIX;
			break;

		case 'p':
			argv++;
			argc--;
			if (argc == 0)
				goto use;
			if ((tp = name2protox(*argv)) == NULLPROTOX) {
				fprintf(stderr, "netstat: %s: unknown or uninstrumented protocol\n",
					*argv);
				exit(10);
			}
			pflag++;
			break;

		case 'f':
			argv++;
			argc--;
			if (argc == 0) {
				fprintf(stderr, "netstat: address family missing\n");
				fprintf(stderr, "usage: %s %s\n", name, usage);
				exit(10);
			}
			if (strcmp(*argv, "ns") == 0)
			    {
				fprintf(stderr,
				       "netstat: %s: address family not supported\n",
					*argv);
				exit(10);
			    }
			else if (strcmp(*argv, "inet") == 0)
				af = AF_INET;
			else if (strcmp(*argv, "unix") == 0)
				af = AF_UNIX;
			else {
				fprintf(stderr, "netstat: %s: unknown address family\n",
					*argv);
				exit(10);
			}
			break;

		case 'I':
			iflag++;
			if (*(interface = cp + 1) == 0) {
				if ((interface = argv[1]) == 0)
					break;
				argv++;
				argc--;
			}
			for (cp = interface; isalpha(*cp); cp++)
				;
			unit = atoi(cp);
			chkiface(interface);
			*cp-- = 0;
			break;

		default:
use:
			printf("usage: %s %s\n", name, usage);
			exit(1);
		}
		argv++, argc--;
	}
	if (argc > 0 && isdigit(argv[0][0])) {
		interval = atoi(argv[0]);
		if (interval <= 0) {
			printf("Interval parameter must be positive:\n");
			goto use;
		}
		if (interval > MAX_INTERVAL_PAR) {
		    printf("Interval parameter is too long (max %d seconds)\n",
			   MAX_INTERVAL_PAR);
		    goto use;
		}
		argv++, argc--;
		iflag++;
	}

#ifdef TRIX
	if (havemac
	    && sgi_getplabel(&plabel) < 0) {
		perror("netstat: can't get process label");
		exit(1);
	}
#endif

	if (argc > 0) {
		system = *argv;
		argv++, argc--;
	}
	if (stat(system, &sb) == -1) {
		fprintf(stderr, "netstat: cannot open ");
		perror(system);
		exit(1);
	}

	ba_addr = sysmp(MP_KERNADDR, MPKA_BSD_KERNADDRS);
	if (ba_addr == -1) {
		if (errno != EINVAL) {
			perror("netstat: sysmp");
			exit(1);
		}

		/* kernel doesn't recognize us; use old way (nlist) */
		NLIST(system, nl);
		if (nl[0].n_type == 0) {
			fprintf(stderr, "netstat: %s: no namelist\n", system);
			exit(1);
		}
	}
	if (argc > 0) {
		kmemf = *argv;
		kflag++;
	}
	kmem = open(kmemf, 0);
	if (kmem < 0) {
		fprintf(stderr, "netstat: cannot open ");
		perror(kmemf);
		exit(1);
	}
	if (ba_addr != -1) {
		/* cheese */
		struct bsd_kernaddrs ba;
		int i;

		(void)klseek(kmem, ba_addr, 0);
		read(kmem, (char *)&ba, sizeof(ba));
		for (i = 0; i < _KA_MAX; i++) {
			nl[i].n_value = (__psunsigned_t)ba.bk_addr[i];
		}
	}

	if (kflag) {
		/* really should be done only for cores; if they specify
		 * /dev/kmem explictly, this will break things... */
		mem_offset = nl[_KA_MEMBASE].n_value;
	}

	if (nl[_KA_KERNEL_MAGIC].n_value == 0
	    || nl[_KA_END].n_value == 0
	    || lseek(kmem, ((long)nl[_KA_KERNEL_MAGIC].n_value) & ~KERNBASE -
			mem_offset, 0) == -1
	    || read(kmem, &kern_end, sizeof(kern_end)) != sizeof(kern_end)
	    || kern_end != nl[_KA_END].n_value) {
		fprintf(stderr, "%s: namelist wrong for %s\n",
			system, kmemf);
		exit(1);
	}

	if (kflag) {
		ns_off_t off;
		if (nl[_KA_PAGECONST].n_value == 0 ||
					nl[_KA_PTECONST].n_value == 0) {
			fprintf(stderr, "%s: namelist wrong for %s\n",
				system, kmemf);
			exit(1);
		}
		off = nl[_KA_PAGECONST].n_value & ~KERNBASE;
		lseek(kmem, off-mem_offset, 0);
		read(kmem, (char *)&pageconst, sizeof(pageconst));
		off = nl[_KA_PTECONST].n_value & ~KERNBASE;
		lseek(kmem, off-mem_offset, 0);
		read(kmem, (char *)&pteconst, sizeof(pteconst));
	}

	pagesize = pageconst.p_pagesz;
	pnumshft = pageconst.p_pnumshft;

	if (kflag) {
		ns_off_t off;

		Sysmap = (pde_t *)malloc(NPGPT * pteconst.pt_pdesize);
		if (!Sysmap) {
			fprintf(stderr, "netstat: %s:"
				"unable to alloc %u bytes for Sysmap\n",
				system, (int)(NPGPT * pteconst.pt_pdesize));
			exit(1);
		}

		off = nl[_KA_KPTBL].n_value & ~KERNBASE;
		lseek(kmem, off-mem_offset, 0);
		read(kmem, (char *)Sysmap, NPGPT * sizeof(pde_t));

		/*
		 * Get the mbuf constants.
		 */
		if (nl[_KA_MBUFCONST].n_value == 0) {
			fprintf(stderr, "netstat: %s: namelist wrong for %s\n",
					system, kmemf);
			exit(0);
		}
		(void)klseek(kmem, nl[_KA_MBUFCONST].n_value, 0);
		(void)read(kmem, (char *)&mbufconst, sizeof(struct mbufconst));
	}

	if (Cflag) {
		disp();
	}
	if (mflag) {
		mbpr(0);
		streampr((ns_off_t)nl[_KA_STRINFO].n_value);
		exit(0);
	}
	if (pflag) {
		if (tp->pr_stats)
			(*tp->pr_stats)(tp->pr_name);
		else
			printf("netstat: %s: no stats routine\n", tp->pr_name);
		exit(0);
	}
	/*
	 * Keep file descriptors open to avoid overhead
	 * of open/close on each call to get* routines.
	 */
	sethostent(1);
	setnetent(1);
	if (iflag) {
		intpr(interval,
		      nl[_KA_IFNET].n_value,
		      nl[_KA_HASHINFO_INADDR].n_value);
		exit(0);
	}
	if (rflag) {
		if (sflag)
			rt_stats((ns_off_t)nl[_KA_RTSTAT].n_value);
		else
			routepr((ns_off_t)nl[_KA_RTREE].n_value);
		exit(0);
	}
	if (Mflag) {
		if (sflag)
			mrt_stats((ns_off_t)nl[_KA_MRTPROTO].n_value);
		else
			mroutepr((ns_off_t)nl[_KA_MRTPROTO].n_value,
				 (ns_off_t)nl[_KA_MFCTABLE].n_value,
				 (ns_off_t)nl[_KA_VIFTABLE].n_value);
		exit(0);
	}

    if (af == AF_INET || af == AF_UNSPEC) {
	setprotoent(1);
	setservent(1);
	while (p = getprotoent()) {

		for (tp = protox; tp->pr_name; tp++)
			if (strcmp(tp->pr_name, p->p_name) == 0)
				break;
		if (tp->pr_name == 0 || tp->pr_wanted == 0)
			continue;
		if (sflag) {
			if (tp->pr_stats)
				(*tp->pr_stats)(p->p_name);
		} else
			if (tp->pr_cblocks)
				(*tp->pr_cblocks)(nl[tp->pr_index].n_value,
					p->p_name);
	}
	endprotoent();
    }
    if ((af == AF_UNIX || af == AF_UNSPEC) && !sflag)
	    unixpr((ns_off_t)nl[_KA_V].n_value, (ns_off_t)nl[_KA_FILE].n_value,
		(struct protosw *)nl[_KA_UNIXSW].n_value);
    exit(0);
}

static int
cpuboard()
{
	static int board = 0;
	inventory_t *inv;

	if ( board )
		return board;

	setinvent();
	while (inv = getinvent())
		if ( inv->inv_class == INV_PROCESSOR &&
			inv->inv_type  == INV_CPUBOARD ) {
			board = inv->inv_state;
			break;
		}
	endinvent();

	return board;
}

static int
syspagesize()
{
	return pagesize;
}

/*
 * Seek into the kernel for a value.
 */
ns_off_t
klseek(fd, base, off)
	int fd, off;
	ns_off_t base;
{
	if (kflag) { /* reading from a core dump in a file */
			extern ns_off_t ptophys4k(ns_off_t,pde_t []);
			extern ns_off_t ptophys4kip19(ns_off_t,pde_t []);
			extern ns_off_t ptophystfp(ns_off_t,pde_t []);

			/* pte format is CPU board dependent */
			switch ( cpuboard() ) {
			case INV_IP19BOARD:
				base = ptophys4kip19(base,Sysmap);
				break;
			default:
			case INV_IP20BOARD:
			case INV_IP22BOARD:
				base = ptophys4k(base,Sysmap);
				break;
			case INV_IP21BOARD:
				base = ptophystfp(base,Sysmap);
				break;
			case INV_IP25BOARD:
				base = ptophys10k(base,Sysmap);
				break;
			}
		/* allow for hole in IP20 and 22, nop for others */
		/* need this so tests for 0 still work, and so physical
		 * addresses don't get messed up */
		if(base != KERNBASE && base >= mem_offset)
			base -= mem_offset;
	} else {
		base &= ~KERNBASE;
	}
	return (lseek(fd, (off_t)base, off));
}

char *
plural(n)
	int n;
{

	return (n != 1 ? "s" : "");
}

char *
pluraly(n)
	int n;
{
	return (n != 1 ? "ies" : "y");
}

char *
plurales(n)
	int n;
{
	return (n != 1 ? "es" : "");
}

/*
 * Find the protox for the given "well-known" name.
 */
struct protox *
knownname(name)
	char *name;
{
	struct protox *tp;

	for (tp = protox; tp->pr_name; tp++)
		if (strcmp(tp->pr_name, name) == 0)
			return(tp);
#ifndef sgi
	for (tp = nsprotox; tp->pr_name; tp++)
		if (strcmp(tp->pr_name, name) == 0)
			return(tp);
#endif
	return(NULLPROTOX);
}

/*
 * Find the protox corresponding to name.
 */
struct protox *
name2protox(name)
	char *name;
{
	struct protox *tp;
	char **alias;			/* alias from p->aliases */
	struct protoent *p;

	/*
	 * Try to find the name in the list of "well-known" names. If that
	 * fails, check if name is an alias for an Internet protocol.
	 */
	if (tp = knownname(name))
		return(tp);

	setprotoent(1);			/* make protocol lookup cheaper */
	while (p = getprotoent()) {
		/* assert: name not same as p->name */
		for (alias = p->p_aliases; *alias; alias++)
			if (strcmp(name, *alias) == 0) {
				endprotoent();
				return(knownname(p->p_name));
			}
	}
	endprotoent();
	return(NULLPROTOX);
}

/*
 * Compute the number of available stream related data structures.
 * This must be computed as this value is dynamic.
 * NOTE: The absolute value must be returned as in DELTA mode the
 * values could become negative since the 'cstrst' structure is replaced
 * by the beginning values at the start of the timing interval.
 */
void
get_strpagesct(int *str_page_curX, int *str_min_pagesX, int *str_max_pagesX)
{
	(void)klseek(kmem, nl[_KA_STR_CURPAGES].n_value, 0);
	read(kmem, (char *)str_page_curX, sizeof(int));

	(void)klseek(kmem, nl[_KA_STR_MINPAGES].n_value, 0);
	read(kmem, (char *)str_min_pagesX, sizeof(int));

	(void)klseek(kmem, nl[_KA_STR_MAXPAGES].n_value, 0);
	read(kmem, (char *)str_max_pagesX, sizeof(int));
}


/* Procedure to convert interface names to interface addresses;
   This routine should only be used for short lived netstat processes
   since the ifnet list is nominally read once and cached thereafter. */

struct sockaddr_in*
ifnametoaddr(const char* ifname, int size)
{
	struct myif {
		char if_name[IFNAMSIZ+10];
		struct sockaddr_in if_addr;
	};
	static struct myif *ifb;
	static int num_if = 128;  /* num initial if entries */

	struct ifnet ifnet;
	struct in_ifaddr myifaddr;
	int i = 0;

	/* see if we have to (re)build ifnet list */

	if (!ifb || size > num_if) {
		char *cp;
		struct myif *ifptr;
		ns_off_t ifnetaddr = 0;
		if (ifb) {
			free(ifb);
			ifb = NULL;
			num_if = size + 32;
		}
		if (!(ifb=(struct myif *)calloc(num_if,sizeof(struct myif))))
			return NULL;

		/* get the address of the first ifnet struct in the kernel */
		klseek(kmem, nl[_KA_IFNET].n_value, 0);
		read(kmem, (char *)&ifnetaddr, sizeof ifnetaddr);

		for (ifptr=ifb; ifnetaddr != 0 && i < num_if; ifptr=&ifb[i]) {
			/* read ifnet struct from kernel */
			klseek(kmem, ifnetaddr, 0),
			read(kmem, &ifnet, sizeof(ifnet));
			ifnetaddr = (ns_off_t)ifnet.if_next;
			/* this shouldn't happen */
			if (ifnet.if_name == NULL) continue;

			/* get the interface name and append unit #  */
			klseek(kmem, (ns_off_t)ifnet.if_name, 0);
			read(kmem, ifptr->if_name, IFNAMSIZ);
			ifptr->if_name[IFNAMSIZ] = '\0';
			cp = &ifptr->if_name[strlen(ifptr->if_name)];
			cp = cp + sprintf(cp, "%d", ifnet.if_unit);
			*cp = '\0';

			/* in_ifaddr == 0 when interface isn't configured;
			   otherwise, read interface address               */
			if (ifnet.in_ifaddr) {
				klseek(kmem, (ns_off_t)ifnet.in_ifaddr, 0);
				read(kmem, &myifaddr, sizeof(myifaddr));
				memcpy(&ifptr->if_addr, &myifaddr.ia_addr,
				       sizeof(struct sockaddr_in));
			}
			i++;
		}
	}

	/* search cached if list for if_name and return associated address */
	for (i=0; i<num_if && *(ifb[i].if_name); i++)
		if (!strcmp(ifb[i].if_name,ifname))
			return (ifb[i].if_addr.sin_addr.s_addr != 0 ? 
				(struct sockaddr_in*) &ifb[i].if_addr : NULL);
	
	return NULL;
}


#ifdef sgi
static void
chkiface(char *iface)
{
	int s;
	static struct	ifreq ifr;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("netstat: socket");
		exit(1);
	}
	strncpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		if (errno == ENXIO)
			fprintf(stderr, "netstat: %s: unknown interface\n", iface);
		else
			perror("netstat: ioctl (SIOCGIFFLAGS)");
		exit(1);
	}
	close(s);
}

/*----------------------------------------------------------------------*/
#include "cdisplay.h"

int Cfirst = 1;
enum cmode cmode = DELTA;

WINDOW *win;

#define NUMFUNCS 9
static int spr_top_offset[NUMFUNCS];

/* ARGSUSED */
void
quit1(int sig)
{
	quitflag++;
}

void
quit(int stat)
{
	if (Cflag) {
		move(BOS, 0);
		clrtoeol();
		refresh();
		(void)endwin();
	}
	exit(stat);
}

void
sig_winch(int sig)
{
	if (Cflag) {
		(void)endwin();
		win = initscr();
		Cfirst = 1;
	}

	/* re-arm in case we have System V signals */
	(void)signal(sig, sig_winch);
}

void
fail(char *fmt, ...)
{
	va_list args;

	if (Cflag) {
		move(BOS, 0);
		clrtoeol();
		refresh();
		endwin();
	}
	va_start(args, fmt);
	fprintf(stderr, "netstat: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(1);
}


#define LABEL_LEN 14
#define NUM_LEN	  12
#define P(lab,fmt,val) \
	{ \
	    if (Cfirst) { move(y,x); printw("%-.*s", LABEL_LEN, lab); } \
	    move(y,x); \
	    printw(fmt,(val)); \
	    y++; \
	}
#define PL(lab) \
	{ \
	    if (Cfirst) { move(y,x); printw("%-s",lab); } \
	    y++; \
	}


static void
spr_if(int cmd, char *tmb)
{
	move(0,0);
	printw("  %d: Interface Statistics -- %s", cmd+1, tmb);
	sprif(2, &spr_top_offset[cmd], nl[_KA_IFNET].n_value);
}

static void
spr_mb(int cmd, char *tmb)
{
	move(0,0);
	printw("  %d: Memory Usage -- %s", cmd + 1, tmb);
	sprmb(2, &spr_top_offset[cmd]);
}


static void
spr_stream(int cmd, char *tmb)
{
	move(0,0);
	printw("  %d: Stream Buffer Usage -- %s", cmd+1, tmb);
	sprstrm(2, &spr_top_offset[cmd], nl[_KA_STRINFO].n_value);
}

static void
spr_ip(int cmd, char *tmb)
{
	move(0,0);
	printw("  %d: IP Statistics -- %s", cmd+1, tmb);
	sprip(2, &spr_top_offset[cmd]);
}

static void
spr_icmp(int cmd, char *tmb)
{
	move(0,0);
	printw("  %d: ICMP Statistics -- %s", cmd+1, tmb);
	spricmp(2, &spr_top_offset[cmd]);
}

static void
spr_udp(int cmd, char *tmb)
{
	move(0,0);
	printw("  %d: UDP Statistics -- %s", cmd+1, tmb);
	sprudp(2, &spr_top_offset[cmd]);
}

static void
spr_tcp(int cmd, char *tmb)
{
	move(0,0);
	printw("  %d: TCP Statistics -- %s", cmd+1, tmb);
	sprtcp(2, &spr_top_offset[cmd]);
}

static void
spr_multi(int cmd, char *tmb)
{
	move(0,0);
	printw("  %d: Multicast Forwarding -- %s", cmd+1, tmb);
	sprmulti(2, nl[_KA_VIFTABLE].n_value, nl[_KA_MFCTABLE].n_value);
}

static void
spr_sock(int cmd, char *tmb)
{
	move(0,0);
	printw("  %d: Socket Statistics -- %s", cmd+1, tmb);
	sprsock(2, &spr_top_offset[cmd]);
}


static void
zero_if(int cmd)
{
	sprif(2, &spr_top_offset[cmd], nl[_KA_IFNET].n_value);
	zeroif();
}

/* ARGSUSED */
static void
zero_mb(int cmd)
{
	sprmb(1, &spr_top_offset[cmd]);
	zeromb();
}

/* ARGSUSED */
static void
zero_stream(int cmd)
{
	sprstrm(1, &spr_top_offset[cmd], nl[_KA_STRINFO].n_value);
	zerostream();
}

/* ARGSUSED */
static void
zero_ip(int cmd)
{
	sprip(1, &spr_top_offset[cmd]);
	zeroip();
}

/* ARGSUSED */
static void
zero_icmp(int cmd)
{
	spricmp(1, &spr_top_offset[cmd]);
	zeroicmp();
}

/* ARGSUSED */
static void
zero_udp(int cmd)
{
	sprudp(1, &spr_top_offset[cmd]);
	zeroudp();
}

/* ARGSUSED */
static void
zero_tcp(int cmd)
{
	sprtcp(1, &spr_top_offset[cmd]);
	zerotcp();
}

/* ARGSUSED */
static void
zero_multi(int cmd)
{
	sprmulti(1, nl[_KA_VIFTABLE].n_value, nl[_KA_MFCTABLE].n_value);
	zeromulti();
}

/* ARGSUSED */
static void
zero_sock(int cmd)
{
	sprsock(1, &spr_top_offset[cmd]);
	zerosock();
}


static void (*sprfuncs[NUMFUNCS])(int, char *) = {
    spr_if,
    spr_mb,
    spr_stream,
    spr_ip,
    spr_icmp,
    spr_tcp,
    spr_udp,
    spr_multi,
    spr_sock
};
static void (*sprinitfuncs[NUMFUNCS])(void) = {
    initif,
    initmb,
    initstream,
    initip,
    initicmp,
    inittcp,
    initudp,
    initmulti,
    initsock
};
static void (*sprzerofuncs[NUMFUNCS])(int) = {
    zero_if,
    zero_mb,
    zero_stream,
    zero_ip,
    zero_icmp,
    zero_tcp,
    zero_udp,
    zero_multi,
    zero_sock
};


const char min_cmd_num = '1';
const char max_cmd_num = '1' + NUMFUNCS - 1;
const char ctrl_l = 'L' & 037;

/*
 * Repeated periodic Curses display
 */
static void
disp(void)
{
	time_t now;
	char tmb[80];
	struct timeval wait;
	fd_set rmask;
	struct termio  tb;
	int c, n, top_offset = 1;
	int intrchar;			/* user's interrupt character */
	int suspchar;			/* job control character */
	int scmd = 0;			/* default scmd = MAC */
	struct tms tm;
	static char dtitle[] = "D: Delta/second";
	static char rtitle0[] = "R: Normal -- %b %d %T";
	static char rtitle[] = "R: Normal -- MMM DD HH:MM:SS";
	static char ztitle0[] = "Z: Delta -- %b %d %T";
	static char ztitle[]  = "Z: Delta -- MMM DD HH:MM:SS";

	if (interval == 0)
		interval = 1;		/* default to 1 sec interval */

	(void) ioctl(0, TCGETA, &tb);
	intrchar = tb.c_cc[VINTR];
	suspchar = tb.c_cc[VSUSP];
	FD_ZERO(&rmask);

	initmb();
	initstream();

	/* keep those open to avoid overhead */
	sethostent(1);
	setnetent(1);
	setprotoent(1);
	setservent(1);

	now = time(0) - times(&tm)/HZ;
	cftime(rtitle,rtitle0,&now);

	/* now that everything else is ready, leave cooked mode
	 */
	win = initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	leaveok(stdscr, FALSE);
	nonl();
	intrflush(stdscr,FALSE);
	move(0, 0);

	(void)signal(SIGINT, quit1);
	(void)signal(SIGTERM, quit1);
	(void)signal(SIGHUP, quit1);
	(void)signal(SIGWINCH, sig_winch);
	(void)signal(SIGCONT, sig_winch);

	for (;;) {
		if (Cfirst) {
			clear();
			standout();
			move(BOS, 0);
			printw(
"1:Iface 2:Mem 3:Streams 4:IP 5:ICMP 6:TCP 7:UDP 8:MCast 9:Sock   +/- DZRN:mode");
			standend();
			switch (cmode) {
			    case DELTA:
				move(0, 80-sizeof(dtitle));
				printw(dtitle);
				break;
			    case ZERO:
				move(0, 80-sizeof(ztitle));
				printw(ztitle);
				break;
			    case NORM:
				move(0, 80-sizeof(rtitle));
				printw(rtitle);
				break;
			}
		}
		now = time(0);
		cftime(tmb,"%b %e %T", &now);

		sprfuncs[scmd](scmd, tmb);

		Cfirst = 0;
		move(0, 0);
		refresh();

		(void)gettimeofday(&wait, 0);
		wait.tv_sec = interval-1;
		wait.tv_usec = 1000000 - wait.tv_usec;
		if (wait.tv_usec >= 1000000) {
			wait.tv_sec++;
			wait.tv_usec -= 1000000;
		}
		if (wait.tv_sec < interval
		    && wait.tv_usec < 1000000/10)
			wait.tv_sec++;
		FD_SET(0, &rmask);
		n = select(1, &rmask, NULL, NULL, &wait);
		if (n < 0) {
			if (errno == EINTR && quitflag) {
				quit(0);
			}
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fail("select: %s\n", strerror(errno));
			break;
		} else if (n == 1) {
			c = getch();
			if (c == intrchar || c == 'q' || c == 'Q') {
				quit(0);
				break;
			} else if (c == suspchar) {
				reset_shell_mode();
				kill(getpid(), SIGTSTP);
				reset_prog_mode();
			} else if (c == 'z' || c == 'Z') {
				now = time(0);
				cftime(ztitle,ztitle0,&now);
				for (n = 0; n < NUMFUNCS; n++)
					sprzerofuncs[n](scmd);
				cmode = ZERO;
			} else if (c == 'r' || c == 'R') {
				for (n = 0; n < NUMFUNCS; n++)
					sprinitfuncs[n]();
				cmode = NORM;
			} else if (c == 'd' || c == 'D') {
				cmode = DELTA;
			} else if (c == ctrl_l) {
				;
			} else if (c == 'n' || c == 'N') {
				nflag = !nflag;
			} else if (c == '-' || c == '_') {
				spr_top_offset[scmd]--;
				if (spr_top_offset[scmd] < 0)
					spr_top_offset[scmd] = 0;
			} else if (c == '+' || c == '=') {
				spr_top_offset[scmd]++;
			} else if (c == 'b' || c == 'B' || c == ('B'&037)) {
				spr_top_offset[scmd] -= 5;
				if (spr_top_offset[scmd] < 0)
					spr_top_offset[scmd] = 0;
			} else if (c == 'f' || c == 'F' || c == ('F'&037)) {
				spr_top_offset[scmd] += 5;
			} else if ((c >= min_cmd_num) && (c <= max_cmd_num)) {
				scmd = c - '1';
			}
			Cfirst = 1;
		}
	}
}
#endif
