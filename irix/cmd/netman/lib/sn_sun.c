#ifdef sun
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Sun network interface snooper implementation.
 */
#include <bstring.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stropts.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/nit_if.h>
#include <net/nit_buf.h>
#include <netinet/in.h>
#include "exception.h"
#include "heap.h"
#include "macros.h"
#include "protocol.h"
#include "protoid.h"
#include "snooper.h"

#ifndef INADDR_NONE
#define INADDR_NONE	0xffffffff	/* -1 return */
#endif

/* Define this missing variable used in _rpc_errorhandler() */
int _using_syslog = 0;

DefineSnooperOperations(sun_snops,ssn)

typedef struct {
	char		interface[RAW_IFNAMSIZ+1];
	unsigned int	started;	/* State of snooper */
	unsigned int	seq;		/* Sequence number */
	unsigned int	startDropCounter; /* Start recording number of drops */
	unsigned long	dropCount;	/* Number of drops at interface */
	unsigned long	startDrop;	/* Starting number of drops */
	unsigned long	lastDrop;	/* Latest number of drops */
	Snooper		snooper;	/* base class state */
} SunSnooper;

#define	SSN(sn)	((SunSnooper *) (sn)->sn_private)


/*
 * Given a network interface name, return a pointer to its raw protocol.
 */
Protocol *
getrawprotobyif(char *interface)
{
	if (!strcmp(interface, "lo0"))
		return findprotobyid(PRID_LOOP);
	else if (!strncmp(interface, "sl", 2) && isdigit(interface[2]))
		return findprotobyid(PRID_IP);
	else if (!strncmp(interface, "le", 2) && isdigit(interface[2]))
		return findprotobyid(PRID_ETHER);

	return 0;
}

/*
 * Approximate upper bound on socket buffer space reservation.
 */
#define	MAXBUFSPACE	60000

Snooper *
sunsnooper(char *interface, int bufspace)
{
	SunSnooper *ssn;
	struct strioctl si;
	struct ifreq ifr;
	struct timeval timeout;
	int s, n;

	/*
	 * Allocate a local snooper.
	 */
	ssn = new(SunSnooper);
	bzero(ssn, sizeof *ssn);

	/*
	 * Open the NIT device.
	 */
	s = open("/dev/nit", O_RDONLY);
	if (s < 0)
		goto sysfail;

	/* Arrange to get discrete messages from the stream. */
	if (ioctl(s, I_SRDOPT, (char *) RMSGD) < 0)
		goto sysfail;

#ifdef USE_NBUF_MODULE
	/* Push and configure the buffering module. */
	if (ioctl(s, I_PUSH, "nbuf") < 0)
		goto sysfail;

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	si.ic_cmd = NIOCSTIME;
	si.ic_timout = INFTIM;
	si.ic_len = sizeof timeout;
	si.ic_dp = (char *) &timeout;
	si.ic_timout = INFTIM;
	if (ioctl(s, I_STR, (char *) &si) < 0)
		goto sysfail;

	/*
	 * Set receive buffer to a generous limit.
	 */
	si.ic_cmd = NIOCSCHUNK;
	si.ic_timout = INFTIM;
	si.ic_len = sizeof bufspace;
	si.ic_dp = (char *) &bufspace;
	if (ioctl(s, I_STR, (char *) &si) < 0)
		goto sysfail;
#endif

	/*
	 * Get the interface name.
	 */
	{
		struct ifconf ifc;
		struct ifreq *ifrp;
		char buf[8192];
		int sock;
#define	ifr_sin(ifr)	((struct sockaddr_in *) &(ifr)->ifr_addr)

		sock = socket(PF_INET, SOCK_DGRAM, 0);
		if (sock < 0)
			goto sysfail;

		ifc.ifc_len = sizeof buf;
		ifc.ifc_buf = buf;
		if (ioctl(sock, SIOCGIFCONF, (caddr_t) &ifc) < 0) {
			close(sock);
			goto sysfail;
		}

		if (interface == 0) {
			ifrp = ifc.ifc_req;
			interface = ifrp->ifr_name;
		} else {
			struct hostent *hp;
			unsigned long addr;
			int n;

			hp = gethostbyname(interface);
			if (hp && hp->h_addrtype == AF_INET)
				bcopy(hp->h_addr, &addr, sizeof addr);
			else
				addr = inet_addr(interface);

			n = ifc.ifc_len / sizeof *ifrp;
			for (ifrp = ifc.ifc_req; --n >= 0; ifrp++) {
				if (strcmp(ifrp->ifr_name, interface) == 0)
					break;
				if (ifrp->ifr_addr.sa_family == AF_INET
				    && ifr_sin(ifrp)->sin_addr.s_addr == addr) {
					interface = ifrp->ifr_name;
					break;
				}
			}
			if (n < 0)
				goto sysfail;
		}

		(void) strncpy(ifr.ifr_name, interface, sizeof ifr.ifr_name);
		ifr.ifr_name[sizeof ifr.ifr_name - 1] = '\0';
		close(sock);
#undef  ifr_sin
        }

	/*
	 * Bind to interface.
	 */
	si.ic_cmd = NIOCBIND;
	si.ic_timout = INFTIM;
	si.ic_len = sizeof ifr;
	si.ic_dp = (char *)&ifr;
	if (ioctl(s, I_STR, (char *) &si) < 0)
		goto sysfail;

	/* Flush the read queue. */
	if (ioctl(s, I_FLUSH, (char *)FLUSHR) < 0)
		goto sysfail;

	(void) strncpy(ssn->interface, interface, RAW_IFNAMSIZ);
	ssn->interface[RAW_IFNAMSIZ] = '\0';

	/*
	 * Initialize common state.  NB: sn_init calls exc_raise on error.
	 */
	if (!sn_init(&ssn->snooper, ssn, &sun_snops, ssn->interface, s,
		     getrawprotobyif(ssn->interface))) {
		n = ssn->snooper.sn_error;
		goto fail;
	}
#ifdef FILTER_SETS
	sfs_init(&ssn->sfset);
#endif
	return &ssn->snooper;

sysfail:
	n = errno;
	exc_raise((n == EADDRNOTAVAIL) ? 0 : n, "cannot snoop on %s",
		  interface ? interface : "default interface");
fail:
	if (s >= 0)
		(void) close(s);
	delete(ssn);
	errno = n;
	return 0;
}

/*
 * Add a filter for ex to this sunsnooper.
 */
static int
ssn_add(Snooper *sn, struct expr **exp, struct exprerror *err)
{
#ifdef FILTER_SETS
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
#endif
	return 1;
}

/*
 * Delete the current filter from this localsnooper's interface.
 */
static int
ssn_delete(Snooper *sn)
{
#ifdef FILTER_SETS
	SunSnooper *ssn;
	int n, i;
	SnoopFilter *sf;

	ssn = SSN(sn);
	n = SNOOP_MAXFILTERS;
	for (sf = &ssn->sfset.sfs_vec[0]; --n >= 0; sf++) {
		if (!sfs_freefilter(&ssn->sfset, sf))
			continue;
		i = sf->sf_index;
		if (!ssn_ioctl(sn, SIOCDELSNOOP, &i))
			return 0;
	}
	if (ssn->sfset.sfs_errflags) {
		ssn->sfset.sfs_errflags = 0;
		if (!ssn_ioctl(sn, SIOCERRSNOOP, &ssn->sfset.sfs_errflags))
			return 0;
	}
#endif
	return 1;
}

static int
ssn_read(Snooper *sn, SnoopPacket *sp, int len)
{
	SunSnooper *ssn;
	struct phdr {
		struct nit_iftime time;
		struct nit_ifdrops drop;
		struct nit_iflen len;
	} *ph;
	int cc, flag;
	unsigned short plen;
	struct strbuf cbuf, dbuf;

	ssn = SSN(sn);
	if (ssn->started == 0)
		return 0;

	if (len > sn->sn_packetsize)
		len = sn->sn_packetsize;

	/*
	 * Convert from NIT header to SGI snoop header.  The NIT flags set
	 * create a header that is the same length as the snoop header,
	 * so don't change them!
	 */
	ph = (struct phdr *) sp;
	cbuf.maxlen = sizeof *ph;
	cbuf.buf = (char *) ph;
	dbuf.maxlen = len;
	dbuf.buf = (char *) sp + sizeof *ph + sn->sn_rawhdrpad;
	flag = 0;

	cc = getmsg(sn->sn_file, &cbuf, &dbuf, &flag);
	if (cc < 0) {
		sn->sn_error = errno;
		return cc;
	}

	/* Maintain drop count */
	if (ssn->startDropCounter != 0) {
		ssn->startDropCounter = 0;
		ssn->startDrop = ph->drop.nh_drops;
	}
	ssn->lastDrop = ph->drop.nh_drops;

	/* Create snoop header */
	plen = ph->len.nh_pktlen;
	sp->sp_hdr.snoop_timestamp = ph->time.nh_timestamp;
	sp->sp_hdr.snoop_flags = SN_PROMISC;
	sp->sp_hdr.snoop_packetlen = plen;
	sp->sp_hdr.snoop_seq = ssn->seq++;

	cc = dbuf.len;
	return (cc < 0) ? 0 : cc;
}

static int
ssn_write(Snooper *sn, SnoopPacket *sp, int len)
{
#if 0
	int cc;

	cc = write(sn->sn_file, sp->sp_data + sn->sn_rawhdrpad, len);
	if (cc < 0)
		sn->sn_error = errno;
	return cc;
#endif
	return 0;
}

static int
ssn_ioctl(Snooper *sn, int cmd, void *data)
{
	struct strioctl si;

	switch (cmd) {
	    case SIOCSNOOPLEN:
	      {
		int snooplen = *(int *) data;
		si.ic_cmd = NIOCSSNAP;
		si.ic_timout = INFTIM;
		si.ic_len = sizeof snooplen;
		si.ic_dp = (char *) &snooplen;
		if (ioctl(sn->sn_file, I_STR, (char *) &si) < 0) {
			sn->sn_error = errno;
			return 0;
		}
		break;
	      }

	    /* XXX - Probably have to do something else here! */
	    case SIOCSNOOPING:
	      {
		unsigned long flags;
		SunSnooper *ssn = SSN(sn);
		int on = *(int *) data;
		if (on != 0)
			/* Add flags to create SGI snoop header equivalent */
			flags = NI_PROMISC | NI_TIMESTAMP | NI_LEN | NI_DROPS;
		else
			flags = NI_TIMESTAMP | NI_LEN | NI_DROPS;
		si.ic_cmd = NIOCSFLAGS;
		si.ic_timout = INFTIM;
		si.ic_len = sizeof flags;
		si.ic_dp = (char *) &flags;
		if (ioctl(sn->sn_file, I_STR, (char *) &si) < 0) {
			sn->sn_error = errno;
			return 0;
		}
		if (on != 0) {
			ssn->started = 1;
			ssn->startDropCounter = 1;

			/* Flush the read queue. */
			if (ioctl(sn->sn_file, I_FLUSH, (char *) FLUSHR) < 0) {
				sn->sn_error = errno;
				return 0;
			}
		} else {
			ssn->started = 0;
			ssn->dropCount += ssn->lastDrop - ssn->startDrop;
			ssn->startDrop = ssn->lastDrop;
		}
		break;
	      }

	    case SIOCRAWSTATS:
	      {
		SunSnooper *ssn = SSN(sn);
		struct snoopstats *st = (struct snoopstats *) data;

		st->ss_seq = ssn->seq;
		st->ss_ifdrops = ssn->dropCount
				 + ssn->lastDrop - ssn->startDrop;
		st->ss_sbdrops = 0;
		break;
	      }

	    default:
		return 0;
	}

	return 1;
}

static int
ssn_getaddr(Snooper *sn, int cmd, struct sockaddr *sa)
{
	SunSnooper *ssn;
	int sock, ok;
	struct ifreq ifreq;

	ssn = SSN(sn);
	sock = socket(sa->sa_family, SOCK_DGRAM, 0);
	if (sock < 0) {
		sn->sn_error = errno;
		return 0;
	}
	(void) strncpy(ifreq.ifr_name, ssn->interface, sizeof ifreq.ifr_name);
	if (ioctl(sock, cmd, (caddr_t) &ifreq) < 0) {
		sn->sn_error = errno;
		ok = 0;
	} else {
		*sa = ifreq.ifr_addr;
		ok = 1;
	}
	(void) close(sock);
	return ok;
}

static int
ssn_shutdown(Snooper *sn, enum snshutdownhow how)
{
#if 0
	if (shutdown(sn->sn_file, (int) how) < 0) {
		sn->sn_error = errno;
		return 0;
	}
#endif
	return 1;
}

static void
ssn_destroy(Snooper *sn)
{
	(void) close(sn->sn_file);
	delete(SSN(sn));
}
#endif /* sun */
