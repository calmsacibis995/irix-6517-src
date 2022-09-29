/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: ipnaf_sys.c,v 65.6 1999/02/05 16:54:31 mek Exp $";
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
 * $Log: ipnaf_sys.c,v $
 * Revision 65.6  1999/02/05 16:54:31  mek
 * Cleanup build warnings for IRIX kernel integration.
 *
 * Revision 65.5  1998/03/24 16:21:51  lmc
 * Fixed missing comment characters lost when the ident lines were
 * ifdef'd for the kernel.  Also now calls rpc_icrash_init() for
 * initializing symbols, and the makefile changed to build rpc_icrash.c
 * instead of dfs_icrash.c.
 *
 * Revision 65.4  1998/03/23  17:26:42  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1997/12/16 17:30:24  lmc
 * SOCKET_LOCK and SOCKET_UNLOCK only take one parameter now.  Also changed
 * polltime to match what is in the kudzu select.c code.
 *
 * Revision 65.2  1997/11/06  19:56:50  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:54  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:34  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:45:31  dce
 * *** empty log message ***
 *
 * Revision 1.5  1996/08/05  23:03:58  vrk
 * Removed an old debug printf.
 *
 * Revision 1.4  1996/04/25  23:33:55  vrk
 * In enumerate_interfaces(), the kmem_alloc-ed buf was not getting freed in
 * error paths - which could cause memleak conditions. Added code to
 * free buf in all errorpaths.
 *
 * Revision 1.3  1996/04/19  23:18:31  brat
 * Fixed a compiler error. Var "s" not used in enumerate_interfaces.
 *
 * Revision 1.2  1996/04/15  17:29:14  vrk
 * Old changes. Has some cmn_err (follow VRK)  that will be removed later.
 *
 * Revision 1.1  1995/09/02  02:31:19  dcebuild
 * Initial revision
 *
 * Revision 1.1.55.4  1994/06/10  20:54:06  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  14:59:52  devsrc]
 *
 * Revision 1.1.55.3  1994/05/27  15:35:38  tatsu_s
 * 	Merged up with DCE1_1.
 * 	[1994/05/20  20:52:54  tatsu_s]
 * 
 * 	DG multi-buffer fragments.
 * 	Added rpc__ip_is_*().
 * 	[1994/04/29  18:54:26  tatsu_s]
 * 
 * Revision 1.1.55.2  1994/02/02  21:48:53  cbrooks
 * 	OT9855 code cleanup breaks KRPC
 * 	[1994/02/02  20:59:57  cbrooks]
 * 
 * Revision 1.1.55.1  1994/01/21  22:31:20  cbrooks
 * 	RPC Code Cleanup
 * 	[1994/01/21  20:32:59  cbrooks]
 * 
 * Revision 1.1.4.2  1993/06/10  19:21:58  sommerfeld
 * 	Initial HPUX RP version.
 * 	[1993/06/03  22:03:33  kissel]
 * 
 * 	Add kernel support for rpc__ip_desc_inq_addr() to enumerate interfaces.
 * 	[1992/10/16  22:11:43  toml]
 * 
 * 	06/16/92  tmm  Created from COSL drop
 * 	[1992/06/18  18:30:41  tmm]
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME
**
**      ipnaf_osc
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  This module contains routines specific to the Internet Protocol,
**  the Internet Network Address Family extension service, and the
**  Berkeley Unix system.
**
**
*/
/*
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**  %a%copyright(,**  )
**
**  NAME
**
**      ipnaf_sys
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  This module contains routines specific to the Internet Protocol,
**  the Internet Network Address Family extension service, and the
**  Berkeley Unix system.
**
**  %a%private_begin
**
**  MODIFICATION HISTORY:
**
**  27-nov-90 nacey     rpc__socket_inq_addr() -> rpc__socket_inq_endpoint()
**  30-jul-90 nacey     cribbed from ip/ipnaf_bsd.c
**                      and old ip/ipnaf.c
**  17-jul-90 mishkin   initial code
**
**  %a%private_end  
**
*/

/* portime notes comments to SGI  
*  modifications to HP800/ipnaf_sys to suit sgi
*  check these -> rpc_socket_t is typedef struct socket *rpc_socket_t
*			memcpy changed to bcopy
*  changes as of 062695
*/

#include <commonp.h>
#include <com.h>
#include <comnaf.h>
#include <ipnaf.h>



#include <sys/hashing.h>


#include <net/if.h>


#include "netinet/in.h"
#include "netinet/in_var.h"

#ifdef	SGIMIPS
#include <sys/sockio.h>
#else
#include <sys/ioctl.h>
#endif

#if	defined(_KERNEL) && defined(SGIMIPS)
#include <sys/mbuf.h>
#include <sys/socket.h>
#ifdef	sa_len
#undef	sa_len
#endif
#include <sys/socketvar.h>
#include <sys/cmn_err.h>
#endif /* _KERNEL && SGIMIPS */


#ifdef _SOCKADDR_LEN
/*
 * Note that in the world of BSD 4.4, the struct ifreq's returned
 * from SIOCGIFCONF are *varying length*, but a minimum of 32 bytes.
 *
 * This has some interesting implications on how to parse the result
 * from SIOCGIFCONF.
 */
#endif


#define BUFSIZE_FOR_IFREQ 1024
#ifdef SGIMIPS
#define NO_SIOCGIFADDR
#endif



/***********************************************************************
 *
 *  Internal prototypes and typedefs.
 */

typedef boolean (*enumerate_fn_p_t) _DCE_PROTOTYPE_ ((
rpc_socket_t,
struct ifreq *,
unsigned32,
struct sockaddr *,
rpc_ip_addr_p_t,
rpc_ip_addr_p_t
    ));

INTERNAL void enumerate_interfaces _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t         /*protseq_id*/,
        rpc_socket_t             /*desc*/,
        enumerate_fn_p_t         /*efun*/,
        rpc_addr_vector_p_t     * /*rpc_addr_vec*/,
        rpc_addr_vector_p_t     * /*netmask_addr_vec*/,
        unsigned32              * /*st*/
    ));

INTERNAL boolean get_addr _DCE_PROTOTYPE_ ((
        rpc_socket_t             /*desc*/,
        struct ifreq            * /*ifr*/,
        unsigned32               /*if_flags*/,
        struct sockaddr         * /*if_addr*/,
        rpc_ip_addr_p_t          /*ip_addr*/,
        rpc_ip_addr_p_t          /*netmask_addr*/
    ));                            

INTERNAL boolean get_broadcast_addr _DCE_PROTOTYPE_ ((
        int                      /*desc*/,
        struct ifreq            * /*ifr*/,
        unsigned32               /*if_flags*/,
        struct sockaddr         * /*if_addr*/,
        rpc_ip_addr_p_t          /*ip_addr*/,
        rpc_ip_addr_p_t          /*netmask_addr*/
    ));

/*
 * Since we are in kernel, probably we should lookup them everytime
 * called instead of caching?
 */
typedef struct
{
    unsigned32  num_elt;
    struct
    {
        unsigned32  addr;
        unsigned32  netmask;
    } elt[1];
} rpc_ip_s_addr_vector_t, *rpc_ip_s_addr_vector_p_t;

INTERNAL rpc_ip_s_addr_vector_p_t local_ip_addr_vec = NULL;


#ifdef	_KERNEL

/*
   This is a copy of irix/kern/bsd/net/if.c/ifconf
*/

#ifdef OLD
PRIVATE int get_if
(
    struct ifconf *ifc
)
{
   register struct ifnet *ifp;     /* struct for a network interface */
   register struct ifaddr *ifa;
   register char *cp, *ep;
   struct ifreq ifr, *ifrp;        /* used for socket ioctl's */
   int ifrlen;
   int space = ifc->ifc_len;
   int error = 0;
   
   cmn_err(CE_NOTE, "In get_if space = %d", space);
   ifrp = ifc->ifc_req;
   ep = ifr.ifr_name + sizeof (ifr.ifr_name) - 2;

   for(ifp = ifnet; space > sizeof (ifr) && ifp; ifp = ifp->if_next)
   {
      bcopy(ifp->if_name, ifr.ifr_name, sizeof (ifr.ifr_name) - 2);

      for (cp = ifr.ifr_name; cp < ep && *cp; cp++)
         ;

 /* appending unit number to name string...  */
#ifdef SGIMIPS
      sprintf(cp, "%d", ifp->if_unit);        /* may be > 9 */
#else
      *cp++ = '0' + ifp->if_unit;
      *cp = '\0';
#endif
      cmn_err(CE_NOTE, "If name = %s", ifr.ifr_name);
      cmn_err(CE_NOTE, "If if_unit = %s", ifp->if_unit);
      if ((ifa = ifp->in_ifaddr) == 0)  {
		bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
		bcopy((caddr_t)&ifr, (caddr_t)ifrp, sizeof (ifr));

                space -= sizeof (ifr), ifrp++;
        } else 
              for ( ; space > sizeof (ifr) && ifa; ifa = ifa->ifa_next) {
                  ifr.ifr_addr = *ifa->ifa_addr;
                  bcopy((caddr_t)&ifr, (caddr_t)ifrp, sizeof (ifr));
                  space -= sizeof (ifr), ifrp++;
               }
	}

   ifc->ifc_len -= space;
   cmn_err(CE_NOTE, "In get_if() -  Returning error  = %d", error);
   return (error);
}
#endif /* OLD */

/*
 * Return interface configuration
 * of system.  List may be used
 * in later ioctl's (above) to get
 * other information.
 */
/*ARGSUSED*/
PRIVATE int
get_if ( struct ifconf *ifc )
{
/*	register struct ifconf *ifc = (struct ifconf *)data; */
#if _K64U64
	struct ifconf native_ifconf;
#endif
	register struct ifnet *ifp = ifnet;
	struct in_ifaddr *ia;
	char *cp, *ep;
	struct ifreq ifr, *ifrp;
	int space;
	int error = 0;
        
/*
        extern struct ifconf *irix5_to_ifconf(struct ifconf *user_ic, struct ifconf *native_ic);
	extern struct ifconf *ifconf_to_irix5(struct ifconf *native_ic, struct ifconf *user_ic);
*/
/*
        XLATE_FROM_IRIX5(irix5_to_ifconf, ifc, &native_ifconf);
*/
/*      ifc = irix5_to_ifconf(ifc, &native_ifconf); */
	space = ifc->ifc_len;

#if 0 /* VRK - DEBUG */
        cmn_err(CE_NOTE, "In get_if(): space = %d", space);
#endif /* VRK */

	ifrp = ifc->ifc_req;
	ep = ifr.ifr_name + sizeof (ifr.ifr_name) - 2;

	for (; space > sizeof (ifr) && ifp; ifp = ifp->if_next) {

		/* Create interface name along with unit number in buffer */
		(void)strncpy(ifr.ifr_name, ifp->if_name, sizeof ifr.ifr_name);
		for (cp = ifr.ifr_name; cp < ep && *cp; cp++)
			continue;
		sprintf(cp, "%d", ifp->if_unit);	/* may be > 9 */

#if 0 /* VRK - DEBUG */
		cmn_err(CE_NOTE, "In get_if(): name = %s", ifr.ifr_name);
		cmn_err(CE_NOTE, "In get_if(): cp = %s", cp);
#endif /* VRK */

		/*
		 * return primary address for interface so we either copy the
		 * primary interface address into the buffer or zero it.
		 */
		if ((ia = (struct in_ifaddr *)ifp->in_ifaddr)) {
			ifr.ifr_addr = *(ia->ia_ifa.ifa_addr);
		} else {
#if 0 /* VRK - DEBUG */
			cmn_err(CE_NOTE, "In get_if(): Zeroing the addr \n");
#endif /* VRK */
			bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
		}

		/*
		 * copy back to caller name and primary i/f address
		 */
#ifndef _HAVE_SA_LEN
/*
		error = copyout(&ifr, ifrp, sizeof(ifr));
*/
		bcopy ( (caddr_t) &ifr, (caddr_t) ifrp, sizeof(ifr));
#if 0 /* VRK - DEBUG */
		cmn_err(CE_NOTE, "In get_if(): ifrp =  %16X\n", ifrp);
#endif /* VRK */
		if (error) {
#if 0 /* VRK - DEBUG */
			cmn_err(CE_NOTE, "In get_if(): Error in Copyout(1) (ifr)\n");
#endif /* VRK */
			break;
		}
		space -= sizeof (ifr), ifrp++;
#else
		{
		register struct sockaddr *sa = ifa->ifa_addr;

		if (sa->sa_len <= sizeof(*sa)) {
			ifr.ifr_addr = *sa;
			error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
					sizeof (ifr));
#if 0 /* VRK - DEBUG */
			cmn_err(CE_NOTE, "In get_if(): Error in Copyout (2) (ifr)\n");
#endif /* VRK */
			ifrp++;
		} else {
			space -= sa->sa_len - sizeof(*sa);
			if (space < sizeof (ifr))
				break;
			error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
					sizeof (ifr.ifr_name));
#if 0 /* VRK - DEBUG */
			cmn_err(CE_NOTE, "In get_if(): Error in Copyout (3) (ifr)\n");
#endif /* VRK */
			if (error == 0)
			    error = copyout((caddr_t)sa,
			      (caddr_t)&ifrp->ifr_addr, sa->sa_len);
			ifrp = (struct ifreq *)
				(sa->sa_len+ (caddr_t)&ifrp->ifr_addr);
		}
		if (error)
			break;
		space -= sizeof (ifr);
		}
#endif /* _HAVE_SA_LEN */
	} /* end of for ifnet loop */
	ifc->ifc_len -= space;

/*	XLATE_TO_IRIX5(ifconf_to_irix5, (struct ifconf *)data, ifc); 
	(void)ifconf_to_irix5(ifc, (struct ifconf *)data);
*/
#if 0 /* VRK - DEBUG */
	cmn_err(CE_NOTE, "In get_if(): Returing. Error = %d\n", error);
#endif /* VRK */
	return (error);
}

#endif	/* _KERNEL */

/*
**++
**
**  ROUTINE NAME:       enumerate_interfaces 
**
**  SCOPYE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Return a vector of IP RPC addresses.  Note that this function is
**  shared by both "rpc__ip_desc_inq_addr" and "rpc__ip_get_broadcast"
**  so that we have to have only one copy of all the gore (ioctl's)
**  associated with inquiring about network interfaces.  This routine
**  filters out all network interface information that doesn't correspond
**  to up, non-loopback, IP-addressed network interfaces.  The supplied
**  procedure pointer (efun) does the rest of the work.
**  
**
**  INPUTS:
**
**      protseq_id      Protocol Sequence ID representing a particular
**                      Network Address Family, its Transport Protocol,
**                      and type.
**
**      desc            Descriptor, indicating a socket that has been
**                      created on the local operating platform.
**
**      efun            Procedure pointer supplied to "do the rest of the work".
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      rpc_addr_vec    Returned vector of RPC addresses.
**
**      netmask_addr_vec Returned vector of netmask RPC addresses.
**
**      status          A value indicating the status of the routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void enumerate_interfaces
    (protseq_id, desc, efun, rpc_addr_vec, netmask_addr_vec, status)

rpc_protseq_id_t        protseq_id;
rpc_socket_t            desc;
enumerate_fn_p_t        efun;
rpc_addr_vector_p_t     *rpc_addr_vec;		/* typdef to struct ifconf * */
rpc_addr_vector_p_t     *netmask_addr_vec;
unsigned32              *status;

{
    rpc_ip_addr_p_t         ip_addr;
    int                     i, s;
    /* unsigned char           buf[1024];  XXX risky for _KERNEL */
    unsigned char           *buf;
    struct ifconf           ifc;
    struct ifreq            *ifr, *last_ifr;
    struct ifreq            ifreq;
    short                   if_flags;
    struct sockaddr         if_addr;
    rpc_ip_addr_p_t         netmask_addr = NULL;
#ifdef	_KERNEL
    int                     errno;
#endif	/* _KERNEL */

#ifdef _SOCKADDR_LEN
    int                     ifrlen;
#else
#define ifrlen sizeof(struct ifreq)
#endif	/* _SOCKADDR_LEN */

    CODING_ERROR (status);

    /* malloc buffer to fill in the ifreq struct , was earlier 
     * done statically
     */
    buf = (unsigned char *) kmem_zalloc( BUFSIZE_FOR_IFREQ, KM_SLEEP );

    if (buf == NULL){
        *status = rpc_s_no_memory;
         return;
    }

    /*
     * Get the list of network interfaces.
     */
    ifc.ifc_len = BUFSIZE_FOR_IFREQ;
    ifc.ifc_buf = (caddr_t) buf;
    
ifconf_again:
#ifdef	_KERNEL
    /*
     * for OSF, would be great to do
     * if (errno = osf_ifioctl(desc, OSIOCGIFCONF, (caddr_t) &ifc)),
     * but that would do copyout()'s to user space, so ...
     */
    if (errno = get_if(&ifc))
#else
    if (ioctl (desc, (int) SIOCGIFCONF, (caddr_t) &ifc) < 0)
#endif	/* _KERNEL */
    {
        if (errno == EINTR)
        {
            goto ifconf_again;
        }
	if ( buf )
		kmem_free (buf, BUFSIZE_FOR_IFREQ);
        *status = -2;   /* !!! */
	return;
    }

    RPC_DBG_PRINTF(rpc_e_dbg_general, 10,
		   ("%d bytes of ifreqs, ifreq is %d bytes\n", ifc.ifc_len, sizeof(struct ifreq)));


    /*
     * Figure out how many interfaces there must be and allocate an  
     * RPC address vector with the appropriate number of elements.
     * (We may ask for a few too many in case some of the interfaces
     * are uninteresting.)
     */
    RPC_MEM_ALLOC (
        *rpc_addr_vec,
        rpc_addr_vector_p_t,
        (sizeof **rpc_addr_vec) +
		   (((ifc.ifc_len/sizeof (struct ifreq)) - 1) * (sizeof (rpc_addr_p_t))),
        RPC_C_MEM_RPC_ADDR_VEC,
        RPC_C_MEM_WAITOK);
    
    if (*rpc_addr_vec == NULL)
    {
#if 0 /* VRK - DEBUG */
	cmn_err(CE_NOTE, "In enumerate_interfaces(1) - Returning status (%d)\n", *status); 
#endif /* VRK */
	if ( buf )
		kmem_free (buf, BUFSIZE_FOR_IFREQ);
	
        *status = rpc_s_no_memory;
        return;
    }
    if (netmask_addr_vec != NULL)
    {
        RPC_MEM_ALLOC (
            *netmask_addr_vec,
            rpc_addr_vector_p_t,
            (sizeof **netmask_addr_vec) +
        (((ifc.ifc_len/sizeof (struct ifreq)) - 1) * (sizeof (rpc_addr_p_t))),
            RPC_C_MEM_RPC_ADDR_VEC,
            RPC_C_MEM_WAITOK);
        
        if (*netmask_addr_vec == NULL)
        {
#if 0 /* VRK - DEBUG */
	    cmn_err(CE_NOTE, "In enumerate_interfaces(2) - Returning status (%d)\n", *status); 
#endif /* VRK */
            RPC_MEM_FREE (*rpc_addr_vec, RPC_C_MEM_RPC_ADDR_VEC);
	    if ( buf )
		kmem_free (buf, BUFSIZE_FOR_IFREQ);
	
            *status = rpc_s_no_memory;
            return;
        }

        (*netmask_addr_vec)->len = 0;
    }

    /*
     * Go through the interfaces and get the info associated with them.
     */
    (*rpc_addr_vec)->len = 0;
    last_ifr = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);

    for (ifr = ifc.ifc_req; ifr < last_ifr; ifr = (struct ifreq *)(((caddr_t)ifr) + ifrlen))
    {
#ifdef _SOCKADDR_LEN
        ifrlen = sizeof (struct ifreq) - sizeof(struct sockaddr) + ifr->ifr_addr.sa_len ;
#endif
        RPC_DBG_PRINTF(rpc_e_dbg_general, 10, ("interface %s\n", ifr->ifr_name));
        /*
         * Get the interface's flags.  If the flags say that the interface
         * is not up or is the loopback interface, skip it.  Do the
         * SIOCGIFFLAGS on a copy of the ifr so we don't lose the original
         * contents of the ifr.  (ifr's are unions that hold only one
         * of the interesting interface attributes [address, flags, etc.]
         * at a time.)
         */
        memcpy(&ifreq, ifr, sizeof(ifreq));

ifflags_again:
#ifdef _KERNEL
	/* VRK added socket lock/unlock */
	SOCKET_LOCK(desc);
        errno = ifioctl(desc, SIOCGIFFLAGS, (caddr_t)&ifreq);
	SOCKET_UNLOCK(desc);
	if ( errno )

#else
	if (ioctl(desc, SIOCGIFFLAGS, &ifreq) < 0)
#endif	/* _KERNEL */
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 10,
                ("SIOCGIFFLAGS returned errno %d\n", errno));
            if (errno == EINTR)
            {
                goto ifflags_again;
            }
            continue;
        }
        if_flags = ifreq.ifr_flags;     /* Copy out the flags */
        RPC_DBG_PRINTF(rpc_e_dbg_general, 10, ("flags are ex\n", if_flags));

	/* Ignore interfaces which are not 'up'. */
	if ((if_flags & IFF_UP) == 0)
		continue;
	
        /*
         * Get the addressing stuff for this interface.
         */
#ifdef NO_SIOCGIFADDR
	/* irix note - the earlier get_if() call fills in the address
         *
         * Note that some systems do not return the address for the
         * interface given.  However the ifr array elts contained in
         * the ifc block returned from the SIOCGIFCONF ioctl above already
         * contains the correct addresses. So these systems should define
         * NO_SIOCGIFADDR in their platform specific include file.
         */
	if_addr = ifr->ifr_addr;
#else
 	/* won't get in here for irix */
        /*
         * Do the SIOCGIFADDR on a copy of the ifr.  See above.
         */
	bcopy( (char *)&ifreq, (char *)ifr, sizeof(ifreq));

ifaddr_again:

#ifdef _KERNEL
#ifdef	__OSF__
        if (errno = osf_ifioctl(desc, SIOCGIFADDR, (caddr_t)&ifreq))
#else
#error	"Requires porting to your OS"
#endif /* _KERNEL */
#else
	if (ioctl(desc, SIOCGIFADDR, &ifreq) < 0)
#endif	/* _KERNEL */
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 10,
                ("SIOCGIFADDR returned errno %d\n", errno));
            if (errno == EINTR)
            {
                goto ifaddr_again;
            }

            /* XXX probably could just continue here XXX */
	    *status = -4;
            goto FREE_IT;
        }

        memcpy (&if_addr, &ifreq.ifr_addr, sizeof(struct sockaddr));

#endif	/* NO_SIOCGIFADDR */
 
        /*
         * If this isn't an Internet-family address, ignore it.
         */
        if (if_addr.sa_family != AF_INET)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 10, ("AF %d not INET\n", if_addr.sa_family));
            continue;
        }

        /*
         * Allocate and fill in an IP RPC address for this interface.
         */
        RPC_MEM_ALLOC (
            ip_addr,
            rpc_ip_addr_p_t,
            sizeof (rpc_ip_addr_t),
            RPC_C_MEM_RPC_ADDR,
            RPC_C_MEM_WAITOK);

        if (ip_addr == NULL)
        {
            *status = rpc_s_no_memory;
            goto FREE_IT;
        }

        ip_addr->rpc_protseq_id = protseq_id;
        ip_addr->len            = sizeof (struct sockaddr_in);
        if (netmask_addr_vec != NULL)
        {
            RPC_MEM_ALLOC (
                netmask_addr,
                rpc_ip_addr_p_t,
                sizeof (rpc_ip_addr_t),
                RPC_C_MEM_RPC_ADDR,
                RPC_C_MEM_WAITOK);
            
            if (netmask_addr == NULL)
            {
                *status = rpc_s_no_memory;
                RPC_MEM_FREE (ip_addr, RPC_C_MEM_RPC_ADDR);
                goto FREE_IT;
            }
            
            netmask_addr->rpc_protseq_id = protseq_id;
            netmask_addr->len            = sizeof (struct sockaddr_in);
        }

        /*
         * Call out to do any final filtering and get the desired IP address
         * for this interface.  If the callout function returns false, we
         * forget about this interface.
         */
        if (!(*efun) (desc, ifr, if_flags, &if_addr, ip_addr, netmask_addr))
        {
            RPC_MEM_FREE (ip_addr, RPC_C_MEM_RPC_ADDR);
            if (netmask_addr != NULL)
                RPC_MEM_FREE (netmask_addr, RPC_C_MEM_RPC_ADDR);
            continue;
        }

        (*rpc_addr_vec)->addrs[(*rpc_addr_vec)->len++] = (rpc_addr_p_t) ip_addr;
        if (netmask_addr_vec != NULL && netmask_addr != NULL)
            (*netmask_addr_vec)->addrs[(*netmask_addr_vec)->len++]
                = (rpc_addr_p_t) netmask_addr;
    }

    if ((*rpc_addr_vec)->len == 0) 
    {
        *status = -5;   /* !!! */
        goto FREE_IT;
    }

    *status = rpc_s_ok;
#if 0 /* VRK - DEBUG */
    cmn_err(CE_NOTE, "In enumerate_interfaces(3) - Returning status (%d)\n", *status); 
#endif /* VRK */
    return;

FREE_IT:
    if ( buf )
	kmem_free (buf, BUFSIZE_FOR_IFREQ);

    for (i = 0; i < (*rpc_addr_vec)->len; i++)
    {
        RPC_MEM_FREE ((*rpc_addr_vec)->addrs[i], RPC_C_MEM_RPC_ADDR);
    }

    RPC_MEM_FREE (*rpc_addr_vec, RPC_C_MEM_RPC_ADDR_VEC);
    if (netmask_addr_vec != NULL)
    {
        for (i = 0; i < (*netmask_addr_vec)->len; i++)
        {
            RPC_MEM_FREE ((*netmask_addr_vec)->addrs[i], RPC_C_MEM_RPC_ADDR);
        }
        RPC_MEM_FREE (*netmask_addr_vec, RPC_C_MEM_RPC_ADDR_VEC);
    }
}

/*
**++
**
**  ROUTINE NAME:       get_addr
**
**  SCOPYE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  This function is called from "rpc__ip_desc_inq_addr" via
**  "enumerate_interfaces".  See comments in "enumerate_interfaces" for
**  details.
**
**
**  INPUTS:             none
**
**      desc            Socket being used for ioctl's.
**
**      ifr             Structure describing the interface.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      ip_addr
**
**      netmask_addr    netmask address
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     
**
**      result          true => we generated up an address for this interface
**                      false => we didn't.
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL boolean get_addr (desc, ifr, if_flags, if_addr, ip_addr, netmask_addr)

rpc_socket_t            desc;
struct ifreq            *ifr;
unsigned32              if_flags;
struct sockaddr         *if_addr;
rpc_ip_addr_p_t         ip_addr;
rpc_ip_addr_p_t         netmask_addr;

{
    struct ifreq            ifreq;
#ifdef	_KERNEL
    int                     errno;
    int				s;
#endif	/* _KERNEL */

    if (netmask_addr == NULL)
    {
	/* Ignore loopback. */
	if ((if_flags & IFF_LOOPBACK) != 0)
            return(false);
#ifdef	_SOCKADDR_LEN
        /* If we have obtained a bsd4.4-style sockaddr, convert it     *
         * now to a bsd4.3-style sockaddr, which is what RPC deals in. */
        ((struct osockaddr *)if_addr)->sa_family = if_addr->sa_family;
#endif
        memcpy (&ip_addr->sa, if_addr, sizeof(struct sockaddr_in));
        return (true);
    }
    else
    {
#ifdef	_SOCKADDR_LEN
        ((struct osockaddr *)if_addr)->sa_family = if_addr->sa_family;
#endif

        memcpy (&ip_addr->sa, if_addr, sizeof(struct sockaddr_in));

        /*
         * Inquire the interface's netmask address.
         */
        memcpy(&ifreq, ifr, sizeof(ifreq));

    ifnetaddr_again:

#ifdef _KERNEL
	SOCKET_LOCK(desc);
        errno = ifioctl(desc, SIOCGIFNETMASK, (caddr_t)&ifreq);
	SOCKET_UNLOCK(desc);
        if (errno)
#else

        if (ioctl(desc, SIOCGIFNETMASK, &ifreq) < 0)
#endif	/* _KERNEL */
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 10,
                ("SIOCGIFNETMASK returned errno %d\n", errno));
            if (errno == EINTR)
            {
                goto ifnetaddr_again;
            }
            return (false);
        }

#ifdef	_SOCKADDR_LEN
        ((struct osockaddr *)&ifreq.ifr_addr)->sa_family = ifreq.ifr_addr.sa_family;
#endif

        memcpy (&netmask_addr->sa, &ifreq.ifr_addr, sizeof(struct sockaddr_in));

        return (true);
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc__ip_desc_inq_addr
**
**  SCOPYE:              PRIVATE - declared in ipnaf.h
**
**  DESCRIPTION:
**      
**  Receive a socket descriptor which is queried to obtain family, endpoint
**  and network address.  If this information appears valid for an IP
**  address,  space is allocated for an RPC address which is initialized
**  with the information obtained from the socket.  The address indicating
**  the created RPC address is returned in rpc_addr.
**
**  INPUTS:
**
**      protseq_id      Protocol Sequence ID representing a particular
**                      Network Address Family, its Transport Protocol,
**                      and type.
**
**      desc            Descriptor, indicating a socket that has been
**                      created on the local operating platform.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      rpc_addr_vec
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok               The call was successful.
**
**          rpc_s_no_memory         Call to malloc failed to allocate memory.
**
**          rpc_s_cant_inq_socket  Attempt to get info about socket failed.
**
**          Any of the RPC Protocol Service status codes.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/
	
PRIVATE void rpc__ip_desc_inq_addr 
#ifdef _DCE_PROTO_
(
    rpc_protseq_id_t        protseq_id,
    rpc_socket_t            desc,
    rpc_addr_vector_p_t     *rpc_addr_vec,
    unsigned32              *status
)
#else
(protseq_id, desc, rpc_addr_vec, status)
rpc_protseq_id_t        protseq_id;
rpc_socket_t            desc;
rpc_addr_vector_p_t     *rpc_addr_vec;
unsigned32              *status;
#endif
{
    rpc_ip_addr_p_t         ip_addr;
    rpc_ip_addr_t           loc_ip_addr;
    unsigned16              i;


    CODING_ERROR (status);

    /*
     * Do a "getsockname" into a local IP RPC address.  If the network
     * address part of the result is non-zero, then the socket must be
     * bound to a particular IP address and we can just return a RPC
     * address vector with that one address (and endpoint) in it.
     * Otherwise, we have to enumerate over all the local network
     * interfaces the local host has and construct an RPC address for
     * each one of them.
     */
    loc_ip_addr.len = sizeof (rpc_ip_addr_t);

#ifdef _KERNEL
    if (rpc__socket_inq_endpoint (desc, (rpc_addr_p_t)&loc_ip_addr) < 0)
#else
    if (getsockname (desc, &loc_ip_addr.sa, &loc_ip_addr.len) < 0)
#endif /* _KERNEL */
    {
        *status = -1;   /* !!! */
        return;
    }

    if (loc_ip_addr.sa.sin_addr.s_addr == 0)
    {
        enumerate_interfaces
            (protseq_id, desc, get_addr, rpc_addr_vec, NULL, status);

        if (*status != rpc_s_ok)
        {
            return; 
        }
        for (i = 0; i < (*rpc_addr_vec)->len; i++)
        {
            ((rpc_ip_addr_p_t) (*rpc_addr_vec)->addrs[i])->sa.sin_port = loc_ip_addr.sa.sin_port;
        }
    }
    else
    {
        /*
         * allocate memory for the new RPC address
         */
        RPC_MEM_ALLOC (
            ip_addr,
            rpc_ip_addr_p_t,
            sizeof (rpc_ip_addr_t),
            RPC_C_MEM_RPC_ADDR,
            RPC_C_MEM_WAITOK);

        if (ip_addr == NULL)
        {
            *status = rpc_s_no_memory;
            return;
        }

#ifndef	IPNAF_BSD
        /*
         * insert individual parameters into RPC addr
         */
        (ip_addr)->rpc_protseq_id = protseq_id;
        (ip_addr)->len = sizeof (struct sockaddr_in);

        /*
         * get the socket info into the RPC addr
         */
        if (!RPC_SOCKET_IS_ERR(rpc__socket_inq_endpoint (desc, (rpc_addr_p_t) ip_addr)))
        {
            *status = rpc_s_ok;
        }
        else
        {
            RPC_MEM_FREE (ip_addr, RPC_C_MEM_RPC_ADDR);
            *status = rpc_s_cant_inq_socket;
            return;
        }
#endif

	RPC_MEM_ALLOC (
            *rpc_addr_vec,
            rpc_addr_vector_p_t,
            sizeof **rpc_addr_vec,
            RPC_C_MEM_RPC_ADDR_VEC,
            RPC_C_MEM_WAITOK);
    
        if (*rpc_addr_vec == NULL)
        {
            RPC_MEM_FREE (ip_addr, RPC_C_MEM_RPC_ADDR);
            *status = rpc_s_no_memory;
            return;
        }

#ifdef	IPNAF_BSD
        ip_addr->rpc_protseq_id = protseq_id;
        ip_addr->len            = sizeof (struct sockaddr_in);
        ip_addr->sa             = loc_ip_addr.sa;
#endif
	
        (*rpc_addr_vec)->len = 1;
        (*rpc_addr_vec)->addrs[0] = (rpc_addr_p_t) ip_addr;

        *status = rpc_s_ok;
        return;
    }
}


#ifdef _KERNEL
/*
**++
**
**  ROUTINE NAME:       rpc__ip_get_broadcast
**
**  SCOPYE:              PRIVATE - EPV declared in ipnaf.h
**
**  DESCRIPTION:
**      
**
**
**
**  INPUTS:
**
**      naf_id          Network Address Family ID serves
**                      as index into EPV for IP routines.
**
**      rpc_protseq_id
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      rpc_addrs
**
**      status          A value indicating the status of the routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__ip_get_broadcast 
#ifdef _DCE_PROTO_
(
    rpc_naf_id_t        naf_id,
    rpc_protseq_id_t    protseq_id,
    rpc_addr_vector_p_t *rpc_addrs,
    unsigned32          *status 
)
#else
(naf_id, rpc_protseq_id, rpc_addrs, status)
rpc_naf_id_t        naf_id;
rpc_protseq_id_t    protseq_id;
rpc_addr_vector_p_t *rpc_addrs;
unsigned32          *status; 
#endif
{
    *rpc_addrs = NULL; /* Kernel version doesn't do a lot currently */
    *status = -1 /* !!! error_status_ok */;

}
#else	/* _KERNEL */

#endif /* _KERNEL */

/*
**++
**
**  ROUTINE NAME:       rpc__ip_init_local_addr_vec
**
**  SCOPYE:              PRIVATE - declared in ipnaf.h
**
**  DESCRIPTION:
**      
**  Initialize the local address vectors.
**
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      status          A value indicating the status of the routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:
**
**      Update local_ip_addr_vec
**
**--
**/

PRIVATE void rpc__ip_init_local_addr_vec
#ifdef _DCE_PROTO_
(
    unsigned32 *status
)
#else
(status)
unsigned32 *status; 
#endif
{
    rpc_socket_t            desc;
    unsigned32              lstatus;
    unsigned32              i;
#ifdef	_KERNEL
    int                     errno;
#endif	/* _KERNEL */
    rpc_addr_vector_p_t     rpc_addr_vec = NULL;
    rpc_addr_vector_p_t     netmask_addr_vec = NULL;

    CODING_ERROR (status);

    /*
     * Open a socket to pass to "enumerate_interface".
     */
    errno = socreate(AF_INET, &desc, SOCK_DGRAM, 0);

    if (errno) 
    {
        *status = rpc_s_cant_create_socket;   /* !!! */
        return;
    }

    enumerate_interfaces
        (RPC_C_PROTSEQ_ID_NCADG_IP_UDP, desc, get_addr,
         &rpc_addr_vec, &netmask_addr_vec, status);
    errno = soclose(desc);

    if (*status != rpc_s_ok)
    {
        return;
    }

    /*
     * Do some sanity check.
     */

    if (rpc_addr_vec == NULL
        || netmask_addr_vec == NULL
        || rpc_addr_vec->len != netmask_addr_vec->len
        || rpc_addr_vec->len == 0)
    {
        RPC_DBG_GPRINTF(("(rpc__ip_init_local_addr_vec) no local address\n"));
        *status = rpc_s_no_addrs;
        goto free_rpc_addrs;
    }

    RPC_MEM_ALLOC (
        local_ip_addr_vec,
        rpc_ip_s_addr_vector_p_t,
        (sizeof *local_ip_addr_vec)
            + ((rpc_addr_vec->len - 1) * (sizeof (local_ip_addr_vec->elt[0]))),
        RPC_C_MEM_UTIL,
        RPC_C_MEM_WAITOK);
    if (local_ip_addr_vec == NULL)
    {
        *status = rpc_s_no_memory;
        goto free_rpc_addrs;
    }

    local_ip_addr_vec->num_elt = rpc_addr_vec->len;

    for (i = 0; i < rpc_addr_vec->len; i++)
    {
        local_ip_addr_vec->elt[i].addr =
            ((rpc_ip_addr_p_t) rpc_addr_vec->addrs[i])->sa.sin_addr.s_addr;
        local_ip_addr_vec->elt[i].netmask =
            ((rpc_ip_addr_p_t) netmask_addr_vec->addrs[i])->sa.sin_addr.s_addr;
#ifdef DEBUG
        if (RPC_DBG2(rpc_e_dbg_general, 10))
        {
            char         buff[16], mbuff[16];
            unsigned8    *p, *mp;

            p = (unsigned8 *) &(local_ip_addr_vec->elt[i].addr);
            mp = (unsigned8 *) &(local_ip_addr_vec->elt[i].netmask);
            RPC_DBG_PRINTF(rpc_e_dbg_general, 10,
("(rpc__ip_init_local_addr_vec) local network [%d.%d.%d.%d] netmask [%d.%d.%d.%d]\n",
                            UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]),
                            UC(mp[0]), UC(mp[1]), UC(mp[2]), UC(mp[3])));
        }
#endif
    }

free_rpc_addrs:
    if (rpc_addr_vec != NULL)
    {
        for (i = 0; i < rpc_addr_vec->len; i++)
        {
            RPC_MEM_FREE (rpc_addr_vec->addrs[i], RPC_C_MEM_RPC_ADDR);
        }
        RPC_MEM_FREE (rpc_addr_vec, RPC_C_MEM_RPC_ADDR_VEC);
    }
    if (netmask_addr_vec != NULL)
    {
        for (i = 0; i < netmask_addr_vec->len; i++)
        {
            RPC_MEM_FREE (netmask_addr_vec->addrs[i], RPC_C_MEM_RPC_ADDR);
        }
        RPC_MEM_FREE (netmask_addr_vec, RPC_C_MEM_RPC_ADDR_VEC);
    }
    return;
}

/*
**++
**
**  ROUTINE NAME:       rpc__ip_is_local_network
**
**  SCOPYE:              PRIVATE - declared in ipnaf.h
**
**  DESCRIPTION:
**      
**  Return a boolean value to indicate if the given RPC address is on
**  the same IP subnet.
**
**
**  INPUTS:
**
**      rpc_addr        The address that forms the path of interest
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      status          A value indicating the status of the routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      result          true => the address is on the same subnet.
**                      false => not.
**
**  SIDE EFFECTS:       none
**
**--
**/
PRIVATE boolean32 rpc__ip_is_local_network
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t rpc_addr,
    unsigned32   *status
)
#else
(rpc_addr, status)
rpc_addr_p_t rpc_addr;
unsigned32   *status; 
#endif
{
    rpc_ip_addr_p_t         ip_addr = (rpc_ip_addr_p_t) rpc_addr;
    unsigned32              addr1;
    unsigned32              addr2;
    unsigned32              i;

    CODING_ERROR (status);

    if (rpc_addr == NULL)
    {
        *status = rpc_s_invalid_arg;
        return false;
    }

    *status = rpc_s_ok;

    if (local_ip_addr_vec == NULL)
    {
        /*
         * We should call rpc__ip_init_local_addr_vec() here. But, it
         * requires the mutex lock for local_ip_addr_vec. For now just return
         * false.
         */
        return false;
    }

    /*
     * Compare addresses.
     */
    for (i = 0; i < local_ip_addr_vec->num_elt; i++)
    {
        if (ip_addr->sa.sin_family != AF_INET)
        {
            continue;
        }

        addr1 = ip_addr->sa.sin_addr.s_addr & local_ip_addr_vec->elt[i].netmask;
        addr2 = local_ip_addr_vec->elt[i].addr & local_ip_addr_vec->elt[i].netmask;

        if (addr1 == addr2)
        {
            return true;
        }
    }

    return false;
}

/*
**++
**
**  ROUTINE NAME:       rpc__ip_is_local_addr
**
**  SCOPYE:              PRIVATE - declared in ipnaf.h
**
**  DESCRIPTION:
**      
**  Return a boolean value to indicate if the given RPC address is the
**  the local IP address.
**
**
**  INPUTS:
**
**      rpc_addr        The address that forms the path of interest
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      status          A value indicating the status of the routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      result          true => the address is local.
**                      false => not.
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE boolean32 rpc__ip_is_local_addr
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t rpc_addr,
    unsigned32   *status
)
#else
(rpc_addr, status)
rpc_addr_p_t rpc_addr;
unsigned32   *status; 
#endif
{
    rpc_ip_addr_p_t         ip_addr = (rpc_ip_addr_p_t) rpc_addr;
    unsigned32              i;

    CODING_ERROR (status);

    if (rpc_addr == NULL)
    {
        *status = rpc_s_invalid_arg;
        return false;
    }

    *status = rpc_s_ok;

    if (local_ip_addr_vec == NULL)
    {
        /*
         * We should call rpc__ip_init_local_addr_vec() here. But, it
         * requires the mutex lock for local_ip_addr_vec. For now just return
         * false.
         */
        return false;
    }

    /*
     * Compare addresses.
     */
    for (i = 0; i < local_ip_addr_vec->num_elt; i++)
    {
        if (ip_addr->sa.sin_family != AF_INET)
        {
            continue;
        }

        if (ip_addr->sa.sin_addr.s_addr == local_ip_addr_vec->elt[i].addr)
        {
            return true;
        }
    }

    return false;
}
