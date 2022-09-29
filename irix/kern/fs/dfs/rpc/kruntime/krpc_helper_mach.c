/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: krpc_helper_mach.c,v 65.4 1998/03/23 17:26:47 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * @HP_COPYRIGHT@
 */
/*
 * HISTORY
 * $Log: krpc_helper_mach.c,v $
 * Revision 65.4  1998/03/23 17:26:47  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:20:52  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:51  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:55  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:34  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:45:31  dce
 * *** empty log message ***
 *
 * Revision 1.1  1995/10/04  01:16:49  dcebuild
 * Initial revision
 *
 * Revision 1.1.56.2  1994/06/10  20:54:07  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  14:59:53  devsrc]
 *
 * Revision 1.1.56.1  1994/01/21  22:31:22  cbrooks
 * 	RPC Code Cleanup
 * 	[1994/01/21  20:33:00  cbrooks]
 * 
 * Revision 1.1.4.2  1993/06/10  19:22:44  sommerfeld
 * 	Initial HPUX RP version.
 * 	[1993/06/03  22:03:54  kissel]
 * 
 * 	Initial revision.
 * 	[1993/01/15  21:16:53  toml]
 * 
 * $EndLog$
 */

 /*  code changed to make this a kernel loadable module for IRIX */

#define M_VERSION 1.0

char *krpchmversion = "M_VERSION";

#include <commonp.h>
#include <com.h>

#include <krpc_helper.h>
#include <krpc_helper_data.h>

#include <sys/systm.h>
#include <sys/file.h>
#if PRE64
#include <sys/user.h>
#endif
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/conf.h>
/*
#include <net/netmp.h>
*/

#include <sys/poll.h>		     /* irix/kern/sys/poll.h */
extern struct krpc_helper *pending;
extern rpc_mutex_t krpc_helperlock;


/* Device Flags */

int krpchdd_devflag = D_MP;

/* poll head to which polladd will add */
struct pollhead krpch_phead;

/*
 * Platform dependent routines for the krpc_helper module
 * live here
 */

int krpchdd_open
#ifdef _DCE_PROTO_
(
    dev_t dev
)
#else
(dev)
dev_t dev;
#endif
{
    /* select calls from dfsbind are Qed on this pollehad, initialize it */
    initpollhead( &krpch_phead );

    return(krpch_open());
}


int krpchdd_close
#ifdef _DCE_PROTO_
(
    dev_t dev
)
#else
(dev)
dev_t dev;
#endif
{
    krpch_close();
    return (0);
}

int krpchdd_ioctl
#ifdef _DCE_PROTO_
(
    dev_t   dev,
    int     cmd,
    caddr_t data,
    int	flag
)
#else
(dev, cmd, data, flag)
dev_t   dev;
int     cmd;
caddr_t data;
int	flag;
#endif
{
    return (krpch_ioctl(cmd, data));
}

int krpchdd_read
#ifdef _DCE_PROTO_
(
    dev_t dev,
    struct uio *uio
)
#else
(dev, uio, flag)
dev_t dev;
struct uio *uio;
#endif
{
    return(krpch_read(uio));
}

int krpchdd_write
#ifdef _DCE_PROTO_
(
    dev_t dev,
    struct uio *uio
)
#else
(dev, uio)
dev_t dev;
struct uio *uio;
#endif
{
    return(krpch_write(uio));
}


/*
 * for irix 
 * this driver routine will be called for the select call on this device
 * ../krpc_helper.c maintains a Q of pending requests.. dfsbind knows about
 * this through a select call on the krpch pseudo device. So all this does
 * is checks if the read event is being polled for, if yes, then check the
 * pending Q for entries. The write event does'nt apply.. 
 * being consistent with the way other device select calls behave
 * the krpc_helper will sleep for the time out period if none of the 
 * events of interest have occured
 */
int krpchdd_poll
(
	vnode_t *vp,
	register short events,
	int anyyet,
	register short *reventsp,
	struct pollhead **phpp 
)
{
	int happen=0;

	*reventsp = 0;

        if( events & POLLRDNORM )	   {  
		RPC_MUTEX_LOCK(krpc_helperlock);/* nobody mess around  */
						/* with pending Q */
		if(!QUEUE_EMPTY(pending))  {    /* check for entries in */ 
						/* the pending Q*/
			*reventsp |= POLLRDNORM;
			happen = 1;
			}
		RPC_MUTEX_UNLOCK(krpc_helperlock);
		}

        if( events & POLLIN )	   {  
		RPC_MUTEX_LOCK(krpc_helperlock);/* nobody mess around  */
						/* with pending Q */
		if(!QUEUE_EMPTY(pending))  {    /* check for entries in */ 
						/* the pending Q*/
			*reventsp |= POLLIN;
			happen = 1;
			}
		RPC_MUTEX_UNLOCK(krpc_helperlock);
		}

	if( events & POLLOUT )  {
		*reventsp |= POLLOUT;       /* this is always writable */
		happen = 1;
		}

	/* initialize phpp to poll head only if none of the
	 * events happened
	 */
	if(!happen && !anyyet)
		*phpp = &krpch_phead;

	return (0);
}

/*
 * Wakeup the helper process waiting on a KRPC channel. 
 */
void krpc_wakeup(void)
{

	/* polladd would have taken place only for the POLLRDNORM event
	 * so, check only for the this event has happend and wakeup
	 */
	pollwakeup( &krpch_phead, POLLRDNORM );
	pollwakeup( &krpch_phead, POLLIN );
}


#if 0
/* krpchdd_init is called once when this module is loaded */


void krpchdd_init( void )
 {
 
  krpc_helper_init();
 
 } 
#endif
