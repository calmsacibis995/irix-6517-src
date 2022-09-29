/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Revision: 1.8 $"

/*
 *	tpirawip
 *		provide TPI interface to the sockets-based
 *		RAW IP protocol stack
 *
 *	tpirawip uses tpisocket for most of its functionality.
 */

#include "tpisocket.h"
#include "net/route.h"
#include "net/raw_cb.h"
#include "netinet/in_systm.h"
#include "netinet/in.h"
#include "netinet/ip.h"
#include "netinet/in_pcb.h"
#include "netinet/udp.h"

extern int tpirawip_nummajor;
extern int tpirawip_majors[];
extern int tpirawip_maxdevices;
extern int tpiicmp_function();
extern int tpiicmp_convert_address();

/* controller stream stuff
 */
static struct module_info tpirawipm_info = {
	STRID_TPIRAWIP,			/* module ID */
	"TPIRAWIP",			/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* infinite maximum packet size */
	10,				/* hi-water mark */
	0,				/* lo-water mark */
};

static int tpirawip_open(queue_t *, dev_t *, int, int, cred_t *);
static struct qinit tpirawip_rinit = {
	NULL, tpisocket_rsrv, tpirawip_open,
	tpisocket_close, NULL, &tpirawipm_info, NULL
};

static struct qinit tpirawip_winit = {
	tpisocket_wput, tpisocket_wsrv, NULL,
	NULL, NULL, &tpirawipm_info, NULL
};

struct streamtab tpirawipinfo = {&tpirawip_rinit, &tpirawip_winit, NULL, NULL};
int tpirawipdevflag = 0;

static struct tpisocket **tpirawip_devices = NULL;


struct tpisocket_control tpirawip_control = {
	&tpirawip_nummajor,
	tpirawip_majors,
	&tpirawip_devices,
	&tpirawip_maxdevices,
	AF_INET,
	SOCK_RAW,
	IPPROTO_RAW,
	tpisocket_address_size,
	tpiicmp_convert_address,
	TSCF_DEFAULT_ADDRESS,	 /* tsc_flags */
	T_CLTS,		/* tsc_transport_type */
	0x10000, 	/* tsc_tsdu_size */
	-2,		/* tsc_etsdu_size */
	-2,		/* tsc_cdata_size */
	-2,		/* tsc_ddata_size */
	0x10000,	/* tsc_tidu_size */
	0,		/* tsc_provider_flag */
	tpiicmp_function /* tsc_function */
};


void
tpirawipinit()
{
	tpisocket_register(&tpirawip_control);
}


/*ARGSUSED*/
static int
tpirawip_open(
	register queue_t *rq,
	dev_t	*devp,
	int	flag,
	int	sflag,
	cred_t	*crp)
{
	return tpisocket_open(rq, devp, flag, sflag, crp,  &tpirawip_control);
}
