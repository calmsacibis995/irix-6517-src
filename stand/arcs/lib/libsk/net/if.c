#ident	"lib/libsk/net/if.c:  $Revision: 1.48 $"

/*
 * if.c - Generic Ethernet interface layer
 *
 * These routines appear in the conf.c _device_table and provide
 * generic Ethernet interface functions, calling the controller
 * specific routines as required.
 */

#include <arcs/types.h>
#include <arcs/hinv.h>
#include <arcs/errno.h>

#include <sys/param.h>
#include <net/in.h>
#include <saio.h>
#include <saioctl.h>
#include <net/socket.h>
#include <net/arp.h>
#include <net/ei.h>
#include <net/mbuf.h>
#include <libsc.h>
#include <libsk.h>
#include <if.h>

#define cei(x)	((struct ether_info *)(x->DevPtr))
#define	cei_if		ei_acp->ac_if

/*
 * Table of interfaces actually present (filled in at ifopen time)
 */
struct if_tbl {
	struct if_func *ifc_fp;	/* pointer to interface functions */
	int ifc_ctlr;		/* controller number within type */
};

struct if_tbl _if_tbl[MAX_IFCS];

/*
 * ifinit
 *	initialize globals
 *	call init routines of all supported interfaces
 *	call protocol initialization routines
 */
int
_ifinit(void)
{
	register int i;

	bzero(_if_tbl, sizeof _if_tbl);

	for (i = 0; i < _nifcs; i++)
		(*_if_func[i].iff_init)();
	
	_init_arp();

	return 0;
}

/*
 * _if_tbl_alloc
 *	allocate an entry in the _if_tbl
 */
struct if_tbl *
_if_tbl_alloc(void)
{
	register struct if_tbl *iftp;

	for (iftp = _if_tbl; iftp < &_if_tbl[MAX_IFCS]; iftp++)
		if (iftp->ifc_fp == NULL)
			return (iftp);

	return (NULL);
}

/*
 * _init_if_tbl
 *	Build the table of interfaces that are present.
 *
 *	The iob->Controller field is an index into the list of actual
 *	Ethernet interfaces present on the system, of all possible
 *	types.  Thus we need to compute which interfaces are
 *	actually present in order to make sense of an open
 *	request.
 */
static void
_if_tbl_init(void)
{
	register int i, n, c;
	register struct if_tbl *iftp;

	/*
	 * For each type of interface, loop on controller numbers
	 * until the probe routine returns < 0, which means "impossible
	 * unit number".
	 */
	for (i = 0; i < _nifcs; i++) {
		for (c = 0; ; c++) {
			if ((n = (*_if_func[i].iff_probe)(c)) < 0)
				break;
			if (n == 0)
				continue;
			if ((iftp = _if_tbl_alloc()) == NULL) {
				printf("too many Ethernet cards: %s %d %s\n",
				    "at most", MAX_IFCS,
				    "are supported");
				return;
			}
			iftp->ifc_fp = &_if_func[i];
			iftp->ifc_ctlr = c;
		}
	}

}

/*
 * ifopen
 *	determine how many interfaces of each type are present
 *	call the open routine for the appropriate interface
 */
int
_ifopen(IOBLOCK *iob)
{
	register struct if_tbl *iftp;

	if (iob->DevPtr == 0) {
		iob->DevPtr = (void *) malloc (sizeof (struct ether_info));
		if (iob->DevPtr == 0)
		    return(iob->ErrorNumber = EIO);
	}

	/* not allowed pseudo unit */
	if (iob->Controller >= MAX_IFCS || iob->Unit != 0) {
		free (iob->DevPtr);
		return(iob->ErrorNumber = ENXIO);
	}

	/*
	 * Build the interface table if required
	 */
	if (_if_tbl->ifc_fp == NULL)
		_if_tbl_init();

	iftp = &_if_tbl[iob->Controller];
	if (iftp->ifc_fp == NULL) {
		free (iob->DevPtr);
		return(iob->ErrorNumber = ENXIO);
	}

	/*
	 * Stash the real controller number in the unit field so
	 * that we don't need to look it up each time.
	 */
	iob->Unit = iftp->ifc_ctlr;
	iob->Partition = 0;

	/*
	 * Call the interface open routine
	 */
	if ((*iftp->ifc_fp->iff_open)(iob) != ESUCCESS) {
		free (iob->DevPtr);
		return iob->ErrorNumber;
	}
	
	/*
	 * Make _scandevs look at this interface
	 */
	cei(iob)->ei_scan_entered = 0;
	iob->Flags |= F_SCAN;
#ifdef SN0
        {
		extern int symmon;
	
		if (!symmon)
		    iob->Flags |= F_NBLOCK;
	}
#endif
	return(ESUCCESS);
}

void
_ifpoll(IOBLOCK *iob)
{
	/* Polling is not re-entrant
	 */
	if (!cei(iob)->ei_scan_entered) {
		cei(iob)->ei_scan_entered++;
		(*cei(iob)->cei_if.if_poll)(iob);
		cei(iob)->ei_scan_entered--;
	}
	iob->ErrorNumber = ESUCCESS;
}

/*
 * ifstrategy
 *	perform io through an interface
 */
int
_ifstrategy(IOBLOCK *iob, int func)
{
	register struct mbuf *m;
	struct so_table *st;
	int ocnt;

	if (cei(iob)->ei_registry < 0) {
bad:
		printf("socket not bound\n");
		return(iob->ErrorNumber = EINVAL);
	}
	st = &_so_table[cei(iob)->ei_registry];
	if (st->st_count <= 0) {
		printf("bad socket reference\n");
		goto bad;
	}

	if (func == READ) {
		while ((iob->Flags & F_NBLOCK) == 0 && st->st_mbuf == NULL)
			_scandevs();

		/*
		 * It's all or nothing when reading a packet
		 */
		if ((iob->Count > 0) && (m = _so_remove(st))) {
			ocnt = _min(m->m_len, iob->Count);
			bcopy(mtod(m, char *), iob->Address, ocnt);
			bcopy((char *)&m->m_srcaddr,
			    (char *)&cei(iob)->ei_srcaddr,
			    sizeof(cei(iob)->ei_srcaddr));
			_m_freem(m);
			iob->Count = ocnt;
			return(ESUCCESS);
		} else
			iob->Count = 0;
		return(ESUCCESS);
	} else if (func == WRITE) {
		m = _m_get(0,0);
		if (m == 0) {
			printf("ifstrategy: WRITE out of mbufs\n");
			return(iob->ErrorNumber = EIO);
		}
		if (iob->Count > MLEN - MMAXOFF) {
			_m_freem(m);
			printf("datagram too large\n");
			return(iob->ErrorNumber = EIO);
		}
		m->m_off = MMAXOFF;
		bcopy(iob->Address, mtod(m, char *), iob->Count);
		m->m_len = iob->Count;
		ocnt = _udp_output(iob, &cei(iob)->cei_if, m);
		/* iob->Count is correct */
		return(ocnt < 0 ? iob->ErrorNumber : ESUCCESS);
	} else
		printf("ifstrategy: bad function\n");

	return(iob->ErrorNumber = EIO);		/* safty catch */
}

/*  If there is a mbuf for the socket pointed to by fd, then claim
 * we can return the number of bytes in that mbuf (although there
 * can be more than one mbuf, reporting one packet of data is
 * probably sufficient).
 */
static int
_ifgrs(IOBLOCK *iob) 
{
	struct so_table *st;
	
	st = &_so_table[cei(iob)->ei_registry];

	if (st->st_mbuf == NULL)
		return(iob->ErrorNumber = EAGAIN);
	iob->Count = st->st_mbuf->m_len;
	return(ESUCCESS);
}

/*
 * _ifioctl
 *	perform interface ioctls
 */
int
_ifioctl(IOBLOCK *iob)
{
	struct so_table *st;
	struct sockaddr_in *sin;
	__psint_t cmd = (__psint_t)iob->IoctlCmd;
	caddr_t *arg = (caddr_t *)iob->IoctlArg;
	u_short sarg;

	/*
	 * Perform generic interface ioctls here.
	 * Switch out to the device ioctl routine for the
	 * device-specific ones.
	 */
	switch (cmd) {

	case NIOCBIND:
		/*
		 * scan registry table, add new entry if entry for port
		 * doesn't already exist
		 */
		if (cei(iob)->ei_registry >= 0) {
			printf("already bound\n");
			return(iob->ErrorNumber = EIO);
		}
		sarg = (__psunsigned_t)arg & 0xffff;
		cei(iob)->ei_udpport = sarg;
		if (st = _get_socket(sarg)) {
			cei(iob)->ei_registry = st - _so_table;
			return(ESUCCESS);
		}

		printf("out of socket buffers\n");
		return(iob->ErrorNumber = ENOMEM);

	case NIOCSIFADDR:
		sin = (struct sockaddr_in *)&cei(iob)->cei_if.if_addr;
		bzero(sin, sizeof(struct sockaddr_in));
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = *(u_int *)arg;
		cei(iob)->cei_if.if_flags |= IFF_UP;
		arpwhohas(cei(iob)->ei_acp,
			&sin->sin_addr);
		return (ESUCCESS);

	default:
		return ((*cei(iob)->cei_if.if_ioctl)(iob));
	}

	/*NOTREACHED*/
}

/*
 * ifclose
 *	release any socket that's being held
 */
int
_ifclose(IOBLOCK *iob)
{
	/*
	 * reverse the open process
	 */
	iob->Flags &= ~F_SCAN;
	_if_tbl[iob->Controller].ifc_fp->iff_close(iob);
	if (cei(iob)->ei_registry >= 0)
		_free_socket(cei(iob)->ei_registry);

	cei(iob)->ei_registry = -1;
	if (iob->DevPtr) {
		free (iob->DevPtr);
		iob->DevPtr = 0;
	}

	return 0;
}

/*
 * Convert Ethernet address to printable representation.
 */
char *
ether_sprintf(u_char *ap)
{
	static char etherbuf[18];

#define	C(i)	(*(ap+i) & 0xff)
	sprintf(etherbuf, "%x:%x:%x:%x:%x:%x",
		C(0), C(1), C(2), C(3), C(4), C(5));
#undef	C
	return (etherbuf);
}

/* ARCS - new stuff */
/*ARGSUSED*/
int
_if_strat(COMPONENT *self, IOBLOCK *iob)
{
	switch (iob->FunctionCode) {

	case	FC_INITIALIZE:
		return (_ifinit ());

	case	FC_OPEN:
		return (_ifopen (iob));

	case	FC_READ:
		return (_ifstrategy (iob, READ)); 

	case	FC_WRITE:
		return (_ifstrategy (iob, WRITE)); 

	case	FC_CLOSE:
		return (_ifclose (iob));

	case	FC_IOCTL:
		return (_ifioctl (iob));

	case	FC_GETREADSTATUS:
		return(_ifgrs(iob));

	case	FC_POLL:
		_ifpoll (iob);

	default:
		return (-1);
	};
	/*NOTREACHED*/
}
