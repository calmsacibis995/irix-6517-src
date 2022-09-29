/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: uuidsys.c,v 65.7 1998/03/23 17:26:40 gwehrman Exp $";
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
 * $Log: uuidsys.c,v $
 * Revision 65.7  1998/03/23 17:26:40  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.6  1998/01/20 20:02:59  lmc
 * Fixed the identFunc, which somehow had been duplicated.
 *
 * Revision 65.4  1998/01/07  17:20:53  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1998/01/07  17:20:52  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:49  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:57  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:33  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:45:32  dce
 * *** empty log message ***
 *
 * Revision 1.1  1995/08/04  18:16:37  dcebuild
 * Initial revision
 *
 * Revision 1.1.51.3  1994/06/10  20:54:15  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  15:00:01  devsrc]
 *
 * Revision 1.1.51.2  1994/02/02  21:48:57  cbrooks
 * 	OT9855 code cleanup breaks KRPC
 * 	[1994/02/02  21:00:08  cbrooks]
 * 
 * Revision 1.1.51.1  1994/01/21  22:31:32  cbrooks
 * 	RPC Code Cleanup
 * 	[1994/01/21  20:33:05  cbrooks]
 * 
 * Revision 1.1.4.2  1993/06/10  19:25:43  sommerfeld
 * 	Initial HPUX RP version.
 * 	[1993/06/03  22:25:26  kissel]
 * 
 * 	06/16/92  tmm  Created from COSL drop.
 * 	[1992/06/18  18:31:09  tmm]
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME:
**
**      uuid_osc.c
**
**  FACILITY:
**
**      UUID
**
**  ABSTRACT:
**
**      UUID - OSF/1 Operating System Component (osc) dependent routines
**
*/

/*  
*    MICROTIME set to microtime in kern/ml/timer.c
*    changes as of 062295
*/

#include <commonp.h>            /* common definitions                   */
#include <dce/uuid.h>           /* uuid idl definitions (public)        */
#include <uuidp.h>              /* uuid idl definitions (private)       */

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

/*
#include <sys/kernel.h>
*/
#include <sys/param.h>
#include <sys/systm.h>
#ifdef PRE64
#include <sys/user.h>       /* need the u area for a thread handle */
#endif
#include <sys/proc.h>       /* need for u.u_procp.c_cptr */
#ifdef SGIMIPS
#include <os/proc/pproc_private.h>
#endif

#ifdef	PRIVATE
#undef	PRIVATE
#define	PRIVATE
#endif

/* device flags we require to be set */

#define IFF_FLAGS       (IFF_UP | IFF_RUNNING)

/*
 * U U I D _ _ G E T _ O S _ T I M E
 *
 * Get OS time - contains platform-specific code.
 *
 */

PRIVATE void uuid__get_os_time 
#ifdef _DCE_PROTO_
(
    uuid_time_t        *os_time
)
#else
(os_time)
uuid_time_t        *os_time;
#endif
{

    struct timeval      tp;

    /* for SGIMIPS set to microtime in kern/ml/timer.c   */
    MICROTIME(&tp);
    os_time->hi = tp.tv_sec;
    os_time->lo = tp.tv_usec*10;
}


/*
 * U U I D _ _ G E T _ O S _ P I D
 *
 * Get the process id
 */
PRIVATE unsigned32 uuid__get_os_pid (void)
{
#ifdef PRE64
    return ((unsigned32) u.u_procp->p_pid );
#else
    /* This is the one fixed for IRIX 6.4.  This ifdef is stupid
	because this code is in the SGIMIPS directory, but I wanted
	to track the IRIX6.4 changes.  */
    return ((unsigned32) (curprocp ? curprocp->p_pid : 0));
#endif
}


/*
 * U U I D _ _ G E T _ O S _ A D D R E S S
 *
 * Get our ethernet hardware address - contains platform-specific code
 *
 */

PRIVATE void uuid__get_os_address 
#ifdef _DCE_PROTO_
(
    uuid_address_p_t        addr,
    unsigned32              *status
)
#else
(addr, status)
uuid_address_p_t        addr;
unsigned32              *status;
#endif
{
    struct ifnet *ifp;

    /*
     * Scan through the kernel's network interface configuration
     * database looking for any network interface that is UP and
     * isn't a LOOPBACK.
     * does't need locking.. similar to ifa_ifwithaddr() in net/if.c 
     */
    for (ifp = ifnet; ifp; ifp = ifp->if_next)
    {
        if ((ifp->if_flags & IFF_FLAGS) != IFF_FLAGS || (ifp->if_flags & IFF_LOOPBACK))
            continue;
        bcopy(((struct arpcom *)ifp)->ac_enaddr, addr, sizeof (uuid_address_t));

        break;
    }

    *status = uuid_s_ok;
}
