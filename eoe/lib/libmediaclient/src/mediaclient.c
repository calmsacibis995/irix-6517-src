#include "mediaclient.h"

#include <assert.h>
#include <bstring.h>
#include <errno.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "xdr_mc.h"

/*****************************************************************************/
/* Constants and types. */

#define INITIAL_RETRY_INTERVAL	 1
#define MAX_RETRY_INTERVAL	16
#define PMAP_TIMEOUT		60	/* seconds */
#define MEDIAD_PROG	    391017
#define MEDIAD_VERSION		 1
#define MAX	    ((u_int) 0 - 1)	/* MAXUINT for XDR */

typedef enum port_state {
    IDLE, PMAPPING, CONNECTING, PAUSING, CONNECTED
} port_state;

typedef struct xdr_makes_me_scream {
    xdr_mc_closure_t	mc_stuff;	/* must be first */
    unsigned int	n_vols;
    unsigned int	n_devs;
    unsigned int	n_parts;
    mc_system_t	       *sys;
} my_xdr_closure_t;

typedef struct mc_port_private {
    mc_what_next_t	whatnext;
    mc_event_proc_t	event_callback;
    mc_closure_t	closure;
    enum port_state	state;
    int			fd;
    mc_system_t	       *sys;
    mc_event_t		event;
    struct sockaddr_in	address;
    XDR			xdrs;
    my_xdr_closure_t	more_xdrs;
    mc_bool_t		force_unmount;
} mc_port_private;

static bool_t xdr_partptrarray(XDR *, u_int *, mc_partition_t ***);
static bool_t xdr_devptr      (XDR *, mc_device_t **);
static bool_t xdr_partptr     (XDR *, mc_partition_t **);

static int    read_proc(void *, void *, u_int);
static int    write_proc(void *, void *, u_int);

/*****************************************************************************/
/* Port constructor/destructor. */

extern mc_port_t
mc_create_port(__uint32_t ipaddr, mc_event_proc_t cb, mc_closure_t closure)
{
    mc_port_private *port;

    port = (mc_port_private *) malloc(sizeof *port);
    if (!port)
	return NULL;
    bzero(port, sizeof *port);

/*  port->whatnext.mcwn_what      = MC_IDLE;	*/
    port->whatnext.mcwn_fd        = -1;
/*  port->whatnext.mcwn_seconds   = 0;		*/
/*  port->whatnext.mcwn_failcode  = MCF_SUCCESS	*/

    port->event_callback          = cb;
    port->closure                 = closure;
    port->state                   = PMAPPING;
    port->fd                      = -1;
/*  port->sys                     = NULL;	*/
    port->address.sin_family      = AF_INET;
    port->address.sin_port        = PMAPPORT;
    port->address.sin_addr.s_addr = ipaddr;
    port->xdrs.x_public           = (caddr_t) &port->more_xdrs;
    port->more_xdrs.mc_stuff.volpartproc = xdr_partptrarray;
    port->more_xdrs.mc_stuff.devpartproc = xdr_partptrarray;
    port->more_xdrs.mc_stuff.partdevproc = xdr_devptr;
    port->force_unmount		  = 0;

    return (mc_port_t) port;
}

extern void
mc_destroy_port(mc_port_t public)
{
    mc_port_private *port = (mc_port_private *) public;

    if (port->state == CONNECTED)
	xdr_destroy(&port->xdrs);
    if (port->fd >= 0)
	(void) close(port->fd);
    mc_destroy_system(port->sys);
    free(port);
}

/*****************************************************************************/
/* Connection management fu. */

static void
forced_unmounts(mc_port_private *port)
{
    write(port->fd, "S", 1);
}

void
mc_forced_unmounts(mc_port_t public)
{
    mc_port_private *port = (mc_port_private *) public;
    port->force_unmount = 1;
    if (port->fd != -1 && port->state == CONNECTED)
    {
        forced_unmounts(port);
    }
}

static void try_to_connect (mc_port_private *);
static void try_later      (mc_port_private *, mc_failure_code_t);
static void input_handler  (mc_port_private *);
static void output_handler (mc_port_private *);
static void timeout_handler(mc_port_private *);

extern const mc_what_next_t *
mc_execute(mc_port_t public)
{
    mc_port_private *port = (mc_port_private *) public;

    switch (port->whatnext.mcwn_what)
    {
    case MC_IDLE:
	try_to_connect(port);
	break;

    case MC_INPUT:
	input_handler(port);
	break;

    case MC_OUTPUT:
	output_handler(port);
	break;

    case MC_TIMEOUT:
	timeout_handler(port);
	break;

    default:
	assert(0);			/* Mustn't happen. */
    }
    return &port->whatnext;
}

static void
try_to_connect(mc_port_private *port)
{
    int fd;
    int yes;
    int rc;

    assert(port->fd == -1);
    assert(port->state == PMAPPING || port->state == CONNECTING);
    fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
    {   try_later(port, MCF_SYS_ERROR);
	return;
    }
    yes = 1;
    rc = ioctl(fd, FIONBIO, &yes);
    if (rc < 0)
    {   (void) close(fd);
	try_later(port, MCF_SYS_ERROR);
	return;
    }
    rc = connect(fd, &port->address, sizeof port->address);
    if (rc == 0)
    {   port->fd = fd;
	output_handler(port);
    }
    else if (errno == EINPROGRESS)
    {
	port->whatnext.mcwn_what = MC_OUTPUT;
	port->whatnext.mcwn_fd = fd;
	port->whatnext.mcwn_failcode = MCF_SUCCESS;
	port->fd = fd;
    }
    else
    {   (void) close(fd);
	try_later(port, MCF_NO_HOST);
    }
}

static void
try_later(mc_port_private *port, mc_failure_code_t failcode)
{
    /* Increment the retry interval. */

    if (port->whatnext.mcwn_seconds == 0)
	port->whatnext.mcwn_seconds = INITIAL_RETRY_INTERVAL;
    else if (port->whatnext.mcwn_seconds < MAX_RETRY_INTERVAL)
    {   port->whatnext.mcwn_seconds *= 2;
	if (port->whatnext.mcwn_seconds > MAX_RETRY_INTERVAL)
	    port->whatnext.mcwn_seconds = MAX_RETRY_INTERVAL;
    }

    /* Set a timer to wake up and retry later. */

    port->state = PAUSING;
    port->whatnext.mcwn_what = MC_TIMEOUT;
    port->whatnext.mcwn_failcode = failcode;
}

static void
output_handler(mc_port_private *port)
{
    struct sockaddr_in addr;
    CLIENT *client;
    int rc;
    enum clnt_stat clnt_rc;
    unsigned short portno;

    rc = connect(port->fd, &port->address, sizeof port->address);
    if (rc < 0 && errno != EISCONN)
    {
	(void) close(port->fd);
	port->fd = -1;
	try_later(port, port->state == PMAPPING ? MCF_NO_HOST : MCF_NO_MEDIAD);
	return;
    }
    switch (port->state)
    {
    case PMAPPING:

	/* We have connected with portmapper; make a PMAP_GETPORT call. */

	addr.sin_port = PMAPPORT;
	client = clnttcp_create(&addr, PMAPPROG, PMAPVERS, &port->fd,
					RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	clnt_rc = RPC_FAILED;
	portno = 0;
	if (client)
	{   struct pmap pmp = { MEDIAD_PROG, MEDIAD_VERSION, IPPROTO_TCP, 0 };
	    struct timeval timeout = { PMAP_TIMEOUT, 0 };
	    clnt_rc = CLNT_CALL(client, PMAPPROC_GETPORT, (xdrproc_t) xdr_pmap,
			   &pmp, (xdrproc_t) xdr_u_short, &portno, timeout);
	    CLNT_DESTROY(client);
	}
	(void) close(port->fd);
	port->fd = -1;
	if (clnt_rc == RPC_SUCCESS && portno != 0)
	{   port->state = CONNECTING;
	    port->address.sin_port = portno;
	    try_to_connect(port);
	}
	else
	    try_later(port, MCF_NO_MEDIAD);
	break;

    case CONNECTING:

	port->state = CONNECTED;
	port->whatnext.mcwn_what = MC_INPUT;
	port->whatnext.mcwn_fd = port->fd;
	port->whatnext.mcwn_seconds = 0;
	port->whatnext.mcwn_failcode = MCF_SUCCESS;
	xdrrec_create(&port->xdrs, 100, 100, port, read_proc, write_proc);
	if (port->force_unmount)
	{
	    forced_unmounts(port);
	}
	break;

    default:
	{ int unknown_connector_state = 0; assert(unknown_connector_state); }
	break;
    }
}

static void
timeout_handler(mc_port_private *port)
{
    assert(port->state == PAUSING);
    port->state = PMAPPING;
    port->address.sin_port = PMAPPORT;
    try_to_connect(port);
}

/*****************************************************************************/

extern int
mc_port_connect(mc_port_t public)
{
    mc_port_private *port = (mc_port_private *) public;
    struct timeval sleep_interval, *tvp;
    fd_set readfds, writefds;
    int rc;

    while (port->state != CONNECTED)
    {
	const mc_what_next_t *whatnext = mc_execute(port);

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	tvp = NULL;
	if (whatnext->mcwn_what == MC_TIMEOUT)
	{
	    /* Return failure on the first error. */
	    return -1;
	}
	else if (whatnext->mcwn_what == MC_INPUT)
	    FD_SET(whatnext->mcwn_fd, &readfds);
	else if (whatnext->mcwn_what == MC_OUTPUT)
	    FD_SET(whatnext->mcwn_fd, &writefds);
	else
	    return -1;
	
	/* Call select */

	rc = select(FD_SETSIZE, &readfds, &writefds, NULL, tvp);
	if (rc !=  (whatnext->mcwn_what != MC_TIMEOUT))
	    return -1;
    }
    return port->fd;
}

/*****************************************************************************/
/* Decoding the system state. */

extern const mc_system_t *
mc_system_state(mc_port_t public)
{
    mc_port_private *port = (mc_port_private *) public;
    return port->sys;
}

static void
input_handler(mc_port_private *port)
{
    mc_system_t sys;
    u_int nv, nd, np;

    assert(port->state == CONNECTED);

    port->xdrs.x_op = XDR_DECODE;
    port->more_xdrs.sys = &sys;
    assert(port->xdrs.x_public == (caddr_t) &port->more_xdrs);
    if (xdrrec_skiprecord(&port->xdrs) &&
	xdr_u_int(&port->xdrs, &nv) &&
	xdr_u_int(&port->xdrs, &nd) &&
	xdr_u_int(&port->xdrs, &np) &&
	xdr_mc_event(&port->xdrs, &port->event))
    {
	/*
	 *  Allocate memory for vols, devs and parts.  Leave ptrs in sys.
	 */

	mc_volume_t *vp = (mc_volume_t *) malloc(nv * sizeof *vp);
	mc_device_t *dp = (mc_device_t *) malloc(nd * sizeof *dp);
	mc_partition_t *pp = (mc_partition_t *) malloc(np * sizeof *pp);

	if (!vp || !dp || !pp)
	{   if (vp) free(vp);
	    if (dp) free(dp);
	    if (pp) free(pp);
	    return;			/* malloc failed; give up. */
	}
	bzero(vp, nv * sizeof *vp);
	bzero(dp, nd * sizeof *dp);
	bzero(pp, np * sizeof *pp);

	sys.mcs_n_volumes = nv;
	sys.mcs_n_devices = nd;
	sys.mcs_n_parts = np;
	sys.mcs_volumes = vp;
	sys.mcs_devices = dp;
	sys.mcs_partitions = pp;

	if (xdr_mc_system(&port->xdrs, &sys))
	{
	    /*
	     *  Got it.  Copy it and free the original. (The copy is in a
	     * single contiguously malloced chunk.  The original is spread
	     * all over the kingdom.  Did I mention that XDR makes me
	     * scream?
	     */

	    mc_destroy_system(port->sys);
	    port->sys = mc_dup_system(&sys);
	    port->xdrs.x_op = XDR_FREE;
	    assert(port->xdrs.x_public == (caddr_t) &port->more_xdrs);
	    (void) xdr_mc_system(&port->xdrs, &sys);

	    /* Call the user's callback. */

	    (*port->event_callback)(port, port->closure, &port->event);
	}
	else
	{
	    /* Input error. */

	    xdr_destroy(&port->xdrs);
	    (void) close(port->fd);
	    port->fd = -1;
	    try_later(port, MCF_INPUT_ERROR);
	}
    }
    else
    {
	/* Lost connection.  Go back to trying to open it. */

	xdr_destroy(&port->xdrs);
	(void) close(port->fd);
	port->fd = -1;
	try_later(port, MCF_NO_MEDIAD);
    }
}

/*
 *  xdr_partptrarray XDR-translates an array of mc_partition pointers.
 */

static bool_t
xdr_partptrarray(XDR *xdrs, u_int *npartp, mc_partition_t ***ppp)
{
    return xdr_array(xdrs,
		     (caddr_t *) ppp, npartp, MAX, sizeof **ppp, xdr_partptr);
}

/*
 *  xdr_devptr() XDR-translates one mc_device pointer.  It's called
 *  via a jump table from xdr_mc_partition().
 */

static bool_t
xdr_devptr(XDR *xdrs, mc_device_t **devp)
{
    my_xdr_closure_t *more_xdrs = (my_xdr_closure_t *) xdrs->x_public;
    u_int index;

    if (xdrs->x_op == XDR_ENCODE)
	index = (u_int) (*devp - more_xdrs->sys->mcs_devices);
    if (!xdr_u_int(xdrs, &index))
	return FALSE;

    /* Verify index is in range. */

    if (xdrs->x_op != XDR_FREE && index >= more_xdrs->sys->mcs_n_devices)
	return FALSE;

    if (xdrs->x_op == XDR_DECODE)
	*devp = more_xdrs->sys->mcs_devices + index;
    return TRUE;
}

/*
 *  xdr_partptr() XDR-translates one mc_partition pointer.  It's called
 *  from xdr_partptrarray() via xdr_array().
 */

static bool_t
xdr_partptr(XDR *xdrs, mc_partition_t **partp)
{
    my_xdr_closure_t *more_xdrs = (my_xdr_closure_t *) xdrs->x_public;
    u_int index;

    if (xdrs->x_op == XDR_ENCODE)
	index = (u_int) (*partp - more_xdrs->sys->mcs_partitions);

    if (!xdr_u_int(xdrs, &index))
	return FALSE;

    /* Verify index is in range. */

    if (xdrs->x_op != XDR_FREE && index >= more_xdrs->sys->mcs_n_parts)
	return FALSE;

    if (xdrs->x_op == XDR_DECODE)
	*partp = more_xdrs->sys->mcs_partitions + index;
    return TRUE;
}

/*
 *  read_proc() is the read proc provided to xdrrec_create.  It is
 *  called whenever the XDR stream needs more data.
 */

static int
read_proc(void *closure, void *vb, u_int nbytes)
{
    mc_port_private *port = (mc_port_private *) closure;
    char *buffer = (char *) vb;
    int rc;

    rc = (int) read(port->fd, buffer, nbytes);
    if (rc == 0)
	rc = -1;			/* EOF is error. */
    return rc;
}

/*
 *  write_proc() is the write proc provided to xdrrec_create.  I don't
 *  think it's ever called.
 */

static int
write_proc(void *closure, void *vb, u_int nbytes)
{
    mc_port_private *port = (mc_port_private *) closure;
    char *buffer = (char *) vb;

    int never_called = 0; assert(never_called);

    return (int) write(port->fd, buffer, nbytes);
}

/*****************************************************************************/
/* Southland. */

static char *
copystring(char **dstp, char *src)
{
    char *dst = *dstp;
    (void) strcpy(dst, src);
    *dstp = dst + strlen(dst) + 1;
    return dst;
}

extern mc_system_t *
mc_dup_system(const mc_system_t *old_sys)
{
    const int nv = old_sys->mcs_n_volumes;
    const int nd = old_sys->mcs_n_devices;
    const int np = old_sys->mcs_n_parts;
    const mc_volume_t *ovp = old_sys->mcs_volumes;
    const mc_device_t *odp = old_sys->mcs_devices;
    const mc_partition_t *opp = old_sys->mcs_partitions;

    int i, j, npp, nc;
    size_t sys_size;
    void *p;
    mc_system_t *new_sys;
    mc_volume_t *vp;
    mc_device_t *dp;
    mc_partition_t *pp;
    mc_partition_t **ppp, **ppp0;
    char *cp, *cp0;

    /* count partition ptrs. */

    npp = mc_count_pp(old_sys);

    /* count bytes of char strings. */

    nc = mc_count_c(old_sys);

    /* Calculate total space requirement and allocate it. */

    sys_size = (sizeof (mc_system_t) +
		nv * sizeof (mc_volume_t) +
		nd * sizeof (mc_device_t) +
		np * sizeof (mc_partition_t) +
		npp * sizeof (mc_partition_t *) +
		nc * sizeof (char));
    p = malloc(sys_size);
    if (!p)
	return NULL;
	
    /*
     *  Subdivide the memory chunk.  The order is:
     *
     *	    mc_partition * np
     *	    mc_system
     *	    mc_volume * nv
     *	    mc_device * nd
     *	    partition pointers * npp
     *	    characters * nc
     *
     *  The partitions come first because they need 64 bit alignment.
     */

    pp = (mc_partition_t *) p;		/* mc_partitions */
    p = &pp[np];
    new_sys = p;			/* mc_system */
    p = &new_sys[1];
    vp = (mc_volume_t *) p;		/* mc_volumes */
    p = &vp[nv];
    dp = (mc_device_t *) p;		/* mc_devices */
    p = &dp[nd];
    ppp0 = ppp = (mc_partition_t **) p;	/* partition pointers */
    p = &ppp[npp];
    cp0 = cp = (char *) p;		/* characters */
    
    /* Initialize the mc_system structure. */

    bcopy(old_sys, new_sys, sizeof *new_sys);
    new_sys->mcs_volumes = vp;
    new_sys->mcs_devices = dp;
    new_sys->mcs_partitions = pp;

    /* Initialize the mc_volume structures. */

    bcopy(ovp, vp, nv * sizeof *vp);
    for (i = 0; i < nv; i++)
    {   vp[i].mcv_label     = copystring(&cp, ovp[i].mcv_label);
	vp[i].mcv_fsname    = copystring(&cp, ovp[i].mcv_fsname);
	vp[i].mcv_dir       = copystring(&cp, ovp[i].mcv_dir);
	vp[i].mcv_type      = copystring(&cp, ovp[i].mcv_type);
	vp[i].mcv_subformat = copystring(&cp, ovp[i].mcv_subformat);
	vp[i].mcv_mntopts   = copystring(&cp, ovp[i].mcv_mntopts);
	vp[i].mcv_parts     = ppp;
	for (j = 0; j < ovp[i].mcv_nparts; j++)
	    vp[i].mcv_parts[j] = pp + (ovp[i].mcv_parts[j] - opp);
	ppp += ovp[i].mcv_nparts;
    }

    /* Initialize the mc_devices. */

    bcopy(odp, dp, nd * sizeof *dp);
    for (i = 0; i < nd; i++)
    {   dp[i].mcd_name      = copystring(&cp, odp[i].mcd_name);
	dp[i].mcd_fullname  = copystring(&cp, odp[i].mcd_fullname);
	dp[i].mcd_ftrname   = copystring(&cp, odp[i].mcd_ftrname);
	dp[i].mcd_devname   = copystring(&cp, odp[i].mcd_devname);
	dp[i].mcd_parts     = ppp;
	for (j = 0; j < odp[i].mcd_nparts; j++)
	    dp[i].mcd_parts[j] = pp + (odp[i].mcd_parts[j] - opp);
	ppp += odp[i].mcd_nparts;
    }

    /* Initialize the mc_partitions. */

    bcopy(opp, pp, np * sizeof *pp);
    for (i = 0; i < np; i++)
    {   pp[i].mcp_device    = dp + (opp[i].mcp_device - odp);
	pp[i].mcp_format    = copystring(&cp, opp[i].mcp_format);
    }

    assert(ppp == ppp0 + npp);
    assert(cp == cp0 + nc);

    return new_sys;
}

extern void
mc_destroy_system(mc_system_t *sys)
{
    mc_partition_t *pp;

    if (sys == NULL)
	return;

    pp = sys->mcs_partitions;
    assert((char *) sys == (char *) pp + sys->mcs_n_parts * sizeof *pp);

    free(pp);				/* Wheee!  That was easy! */
}
