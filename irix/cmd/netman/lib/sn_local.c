#ifdef __sgi
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Local network interface snooper implementation.
 *
 * Changes History:
 *	Added tokenring.
 */
#include <bstring.h>
#include <ctype.h>
#include <errno.h>
#include <invent.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <net/route.h>
#include <sys/sysctl.h>
#include <net/if_types.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "exception.h"
#include "heap.h"
#include "macros.h"
#include "protocol.h"
#include "protoid.h"
#include "snooper.h"

DefineSnooperOperations(local_snops,lsn)

typedef struct {
	char		interface[RAW_IFNAMSIZ+1];
	struct sfset	sfset;		/* snoopfilter set */
	Snooper		snooper;	/* base class state */
} LocalSnooper;

#define	LSN(sn)	((LocalSnooper *) (sn)->sn_private)

/*
 * Use the kernel's hardware inventory to find an interface's raw protocol.
 */
struct typename {
	char	*ifnam;
	int	namlen;
	int	ctl;			/* controllers for network types */
};
#define	TN(name,ctl)	{ name, constrlen(name), ctl }

static struct typename controllers[] = {
	TN("ec",	INV_ETHER_EC),
	TN("enp",	INV_ETHER_ENP),
	TN("et",	INV_ETHER_ET),
	TN("egl",	INV_ETHER_EGL),
	TN("fxp",	INV_ETHER_FXP),
	TN("cfe",	INV_CRAYIOS_CFEI3),
	TN("imf",	INV_FDDI_IMF),
	TN("ipg",	INV_FDDI_IPG),
	TN("xpi",	INV_FDDI_XPI),
	TN("rns",	INV_FDDI_RNS),
	TN("fv",	INV_TOKEN_FV),
	TN("et",	INV_ETHER_EE),
	TN("ef",	INV_ETHER_EF),
	TN("ep",	INV_ETHER_EP),	/* eplex  */
	TN("gtr",	INV_TOKEN_GTR),	/* token ring - gtr board */
	TN("hip",	INV_HIO_HIPPI),
	TN("atm",	INV_ATM_GIO64),	/* ??? */
	TN("atm",	INV_ATM_QUADOC3),   /* ??? */
	0,
};

struct typeid {
	int	prid;
	int	inv_type;
	int	if_type;
};
#define	TI(prid,inv_type, if_type)	{ prid, inv_type, if_type }

static struct typeid types[] = {
	TI(PRID_ETHER,	    INV_NET_ETHER,  IFT_ETHER),
	TI(PRID_FDDI,	    INV_NET_FDDI,   IFT_FDDI),
	TI(PRID_CRAYIO,	    INV_NET_CRAYIOS, 0),
	TI(PRID_TOKENRING,  INV_NET_TOKEN,  IFT_ISO88025),
	TI(PRID_HIPPI,	    INV_NET_HIPPI,  IFT_HIPPI),
	TI(PRID_ATM,	    INV_NET_ATM,    IFT_ATM),
	TI(PRID_LOOP,	    -1,		    IFT_LOOP),
	TI(PRID_IP,	    -1,		    IFT_SLIP),
	TI(PRID_IP,	    -1,		    IFT_PTPSERIAL),
	PRID_NULL,
};

/*
 * Arguments to the hardware inventory raw protocol scanner.
 */
struct scanargs {
	char	 *interface;
	Protocol *rawproto;
};

/*
 * This function is applied by scaninvent() to all of the hardware
 * inventory items in the system.
 */
static int				/* 1=stop scanning on this item */
rawprotoscan(inventory_t *ie,		/* check this inventory item */
	     void *arg)			/* for this interface name */
{
	struct scanargs *sa;
	struct typename *tn;
	struct typeid *ti;

	if (ie->inv_class != INV_NETWORK)
		return 0;

	/*
	 * Look for a hardware inventory controller type that
	 * we know about, and that has the same interface name
	 * as the interface that we are looking for.
	 */
	sa = arg;
	for (tn = controllers; ; tn++) {
		if (tn->ifnam == 0)
			return 0;
		if (tn->ctl == ie->inv_controller)
			break;
	}
	/*
	 * We found this type of item in our tables.
	 * Is its name in our tables the same as the name of the interface
	 * we are looking for?
	 */
	if (strncmp(tn->ifnam, sa->interface, tn->namlen)
	    || !isdigit(sa->interface[tn->namlen])) {
		return 0;
	}

	/*
	 * Having found the right kind of controller with the right kind
	 * of name, map its network class type to a protocol ID.
	 * This cannot fail unless our static tables are broken.
	 */
	for (ti = types; ; ti++) {
		if (ti->prid == PRID_NULL)
			return 0;
		if (ti->inv_type == ie->inv_type)
			break;
	}

	sa->rawproto = findprotobyid(ti->prid);
	return sa->rawproto != 0;
}

/*
 * Given a network interface name, return a pointer to its raw protocol.
 */
Protocol *
getrawprotobyif(char *interface)
{
	struct scanargs sa;
	char *buf;
	int mib[6];
	size_t needed;
	struct sockaddr_dl *sdl;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam, *ifam_lim, *ifam2;
	struct typeid *ti;
	int serrno;

	sa.interface = interface;
	sa.rawproto = 0;
		/*
		 * If the hardware inventory system recognizes the device
		 * name, then take its answer.
		 */
	if (scaninvent(rawprotoscan, &sa))
		return sa.rawproto;

	/*
	 * Ask the kernel about the device.
	 */
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;
	if (sysctl(mib, 6, 0,&needed, 0, 0) < 0) {
		serrno = errno;
		exc_raise(errno, "size probe sysctl() failed on %s",
			  interface);
		errno = serrno;
		return 0;
	}
	buf = (char *)malloc(needed);
	if (buf == 0) {
		exc_raise(0, "malloc() for sysctl() failed on %s",
			  interface);
		return 0;
	}
	if (sysctl(mib, 6, buf,&needed, 0, 0) < 0) {
		serrno = errno;
		free(buf);
		exc_raise(serrno, "sysctl() failed on %s", interface);
		errno = serrno;
		return 0;
	}

	ifam_lim = (struct ifa_msghdr *)(buf + needed);
	for (ifam = (struct ifa_msghdr *)buf;
	     ifam < ifam_lim;
	     ifam = ifam2) {
		ifam2 = (struct ifa_msghdr*)((char*)ifam
					     + ifam->ifam_msglen);
		if (ifam->ifam_type != RTM_IFINFO)
			continue;
		ifm = (struct if_msghdr *)ifam;
		sdl = (struct sockaddr_dl *)(ifm + 1);
		if (!strncmp(interface, sdl->sdl_data,sdl->sdl_nlen)) {
			for (ti = types; ti->prid != PRID_NULL; ti++) {
				if (ti->if_type == ifm->ifm_data.ifi_type) {
					sa.rawproto=findprotobyid(ti->prid);
					break;
				}
			}
			break;
		}
	}
	free(buf);

	return sa.rawproto;
}

/*
 * Approximate upper bound on socket buffer space reservation.
 */
#define	MAXBUFSPACE	60000

Snooper *
localsnooper(char *interface, int port, int bufspace)
{
	LocalSnooper *lsn;
	int s, n;
	struct sockaddr_raw sr;

	/*
	 * Allocate a local snooper.
	 */
	lsn = new(LocalSnooper);

	/*
	 * Open a snoop socket.
	 */
	s = socket(PF_RAW, SOCK_RAW, RAWPROTO_SNOOP);
	if (s < 0)
		goto sysfail;

	/*
	 * Bind socket to interface.
	 */
	sr.sr_family = AF_RAW;
	sr.sr_port = port;
	if (interface == 0)
		bzero(sr.sr_ifname, sizeof sr.sr_ifname);
	else
		(void) strncpy(sr.sr_ifname, interface, sizeof sr.sr_ifname);
	if (bind(s, &sr, sizeof sr) < 0 && interface) {
		int binderrno;
		struct hostent *hp;
		u_long addr;
		char buf[8192];
		struct ifconf ifc;
		struct ifreq *ifr;
#define	ifr_sin(ifr)	((struct sockaddr_in *) &(ifr)->ifr_addr)

		/*
		 * Check whether interface is an IP hostname or number,
		 * which are often more convenient than an ifname.
		 */
		binderrno = errno;
		hp = gethostbyname(interface);
		if (hp && hp->h_addrtype == AF_INET)
			bcopy(hp->h_addr, &addr, sizeof addr);
		else
			addr = inet_addr(interface);
		if (addr == INADDR_NONE) {
			errno = binderrno;
			goto sysfail;
		}
		ifc.ifc_len = sizeof buf;
		ifc.ifc_buf = buf;
		if (ioctl(s, SIOCGIFCONF, (caddr_t) &ifc) < 0)
			goto sysfail;
		n = ifc.ifc_len / sizeof *ifr;
		for (ifr = ifc.ifc_req; --n >= 0; ifr++) {
			if (ifr->ifr_addr.sa_family == AF_INET
			    && ifr_sin(ifr)->sin_addr.s_addr == addr) {
				interface = ifr->ifr_name;
				break;
			}
		}
		(void) strncpy(sr.sr_ifname, interface, sizeof sr.sr_ifname);
		if (bind(s, &sr, sizeof sr) < 0)
			goto sysfail;
#undef	ifr_sin
	}

	/*
	 * Get interface name if default.
	 */
	if (interface == 0) {
		n = sizeof sr;
		if (getsockname(s, &sr, &n) < 0)
			goto sysfail;
		interface = sr.sr_ifname;
	}
	(void) strncpy(lsn->interface, interface, RAW_IFNAMSIZ);
	lsn->interface[RAW_IFNAMSIZ] = '\0';

	/*
	 * Open socket receive buffer to a generous limit.
	 */
	if (bufspace > MAXBUFSPACE)
		bufspace = MAXBUFSPACE;
	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (caddr_t) &bufspace,
		       sizeof bufspace) < 0) {
		goto sysfail;
	}

	/*
	 * Initialize common state.  NB: sn_init calls exc_raise on error.
	 */
	if (!sn_init(&lsn->snooper, lsn, &local_snops, lsn->interface, s,
		     getrawprotobyif(lsn->interface))) {
		n = lsn->snooper.sn_error;
		goto fail;
	}
	sfs_init(&lsn->sfset);
	return &lsn->snooper;

sysfail:
	n = errno;
	exc_raise((n == EADDRNOTAVAIL) ? 0 : n, "cannot snoop on %s",
		  interface ? interface : "default interface");
fail:
	if (s >= 0)
		(void) close(s);
	delete(lsn);
	errno = n;
	return 0;
}

/*
 * Add a filter for ex to this localsnooper.
 */
static int
lsn_add(Snooper *sn, struct expr **exp, struct exprerror *err)
{
	struct expr *ex;
	struct sfset tmp;
	int i, n, tries;
	SnoopFilter *sf;

	ex = (exp == 0) ? 0 : *exp;
	if (ex && sn->sn_rawproto == 0) {
		sn->sn_error = EINVAL;
		return 0;
	}

	/*
	 * Compile ex into a temporary filter set.  If sfs_compile fails,
	 * sn->sn_error will be zero and err will be set.
	 */
	sfs_init(&tmp);
	if (!sfs_compile(&tmp, ex, sn->sn_rawproto, sn->sn_rawhdrpad, err)) {
		sn->sn_error = 0;
		return 0;
	}

	/*
	 * Delete all kernel filters.  The ioctl will fail with EACCES if
	 * passed the index of a filter not owned by sn.
	 */
	for (i = SNOOP_MAXFILTERS; --i >= 0; )
		(void) lsn_ioctl(sn, SIOCDELSNOOP, &i);
	sn->sn_error = 0;

	/*
	 * Try to add all allocated filters in tmp.  If we run out of room
	 * in the kernel's filter vector, unify tmp and retry once.  If we
	 * can't add even one filter, fail with ENOMEM.
	 */
	tries = 0;
retry:
	n = SNOOP_MAXFILTERS;
	for (sf = &tmp.sfs_vec[0]; --n >= 0; sf++) {
		if (sf->sf_allocated && !lsn_ioctl(sn, SIOCADDSNOOP, sf)) {
			if (sn->sn_error != ENOMEM || ++tries == 2)
				return 0;
			sfs_unifyfilters(&tmp);
			while (--sf >= &tmp.sfs_vec[0]) {
				if (!sf->sf_allocated)
					continue;
				i = sf->sf_index;
				(void) lsn_ioctl(sn, SIOCDELSNOOP, &i);
			}
			goto retry;
		}
	}

	/*
	 * Snoop for error flags, if any.
	 * XXX  No attempt is made to preserve flags set by an uncoordinated
	 *	sn_seterrflags.
	 */
	i = tmp.sfs_errflags;
	if (i != 0 && !lsn_ioctl(sn, SIOCERRSNOOP, &i))
		return 0;

	/*
	 * Now that all kernel filters have been set, update lsn->sfset.
	 */
	LSN(sn)->sfset = tmp;
	return 1;
}

/*
 * Delete the current filter from this localsnooper's interface.
 */
static int
lsn_delete(Snooper *sn)
{
	LocalSnooper *lsn;
	int n, i;
	SnoopFilter *sf;

	lsn = LSN(sn);
	n = SNOOP_MAXFILTERS;
	for (sf = &lsn->sfset.sfs_vec[0]; --n >= 0; sf++) {
		if (!sfs_freefilter(&lsn->sfset, sf))
			continue;
		i = sf->sf_index;
		if (!lsn_ioctl(sn, SIOCDELSNOOP, &i))
			return 0;
	}
	if (lsn->sfset.sfs_errflags) {
		lsn->sfset.sfs_errflags = 0;
		if (!lsn_ioctl(sn, SIOCERRSNOOP, &lsn->sfset.sfs_errflags))
			return 0;
	}
	return 1;
}

static int
lsn_read(Snooper *sn, SnoopPacket *sp, int len)
{
	int cc;

	if (len > sn->sn_packetsize)
		len = sn->sn_packetsize;
	cc = read(sn->sn_file, (char *)sp, sizeof sp->sp_hdr + len);
	if (cc < 0) {
		sn->sn_error = errno;
		return cc;
	}
	cc -= sizeof sp->sp_hdr;
	return (cc < 0) ? 0 : cc;
}

static int
lsn_write(Snooper *sn, SnoopPacket *sp, int len)
{
	int cc;

	cc = write(sn->sn_file, sp->sp_data + sn->sn_rawhdrpad, len);
	if (cc < 0)
		sn->sn_error = errno;
	return cc;
}

static int
lsn_ioctl(Snooper *sn, int cmd, void *data)
{
	if (ioctl(sn->sn_file, cmd, data) < 0) {
		sn->sn_error = errno;
		return 0;
	}
	return 1;
}

static int
lsn_getaddr(Snooper *sn, int cmd, struct sockaddr *sa)
{
	LocalSnooper *lsn;
	int sock, ok;
	struct ifreq ifreq;

	lsn = LSN(sn);
	if (sa->sa_family == AF_RAW)
		sock = sn->sn_file;
	else {
		sock = socket(sa->sa_family, SOCK_DGRAM, 0);
		if (sock < 0) {
			sn->sn_error = errno;
			return 0;
		}
	}
	(void) strncpy(ifreq.ifr_name, lsn->interface, sizeof ifreq.ifr_name);
	if (ioctl(sock, cmd, (caddr_t) &ifreq) < 0) {
		sn->sn_error = errno;
		ok = 0;
	} else {
		*sa = ifreq.ifr_addr;
		ok = 1;
	}
	if (sock != sn->sn_file)
		(void) close(sock);
	return ok;
}

static int
lsn_shutdown(Snooper *sn, enum snshutdownhow how)
{
	if (shutdown(sn->sn_file, (int) how) < 0) {
		sn->sn_error = errno;
		return 0;
	}
	return 1;
}

static void
lsn_destroy(Snooper *sn)
{
	(void) close(sn->sn_file);
	delete(LSN(sn));
}
#endif
