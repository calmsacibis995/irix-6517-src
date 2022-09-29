/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_net_irix.c,v 1.2 1998/03/23 16:26:29 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 *      Copyright (C) 1994 Transarc Corporation
 *      All rights reserved.
 */

/*
 * HISTORY
 * $Log: osi_net_irix.c,v $
 * Revision 1.2  1998/03/23 16:26:29  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 1.1  1998/03/06 19:37:49  gwehrman
 * Initial version.
 *
 *
 */

#include <dcedfs/sysincludes.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_net_mach.h>
#include <net/if.h>
#include <netinet/in_var.h>
#include <sys/hashing.h>

#if	defined(KERNEL)
/***********************************************************************
 *
 * osi_SameHost(addrp):	Check if the address passed in is configured
 *			on any of our network interfaces.
 *
 ***********************************************************************/

int osi_SameHost(struct in_addr *addrp)
{
    struct ifnet 		*ifp;

    INADDR_TO_IFP(*addrp,ifp);
    return (ifp ? 1 : 0);
}


/*
 * This match procedure is called after the hashing lookup has indexed
 * into the hash table and is comparing hash bucket entries for a match
 * with the supplied key (IP address). The ia_subnetmask field of the
 * 'struct in_ifaddr' is applied with a bitwise 'AND' to the s_addr
 * field of the key.  We return the non-zero if the result matches the
 * ia_subnet field of the 'struct in_ifaddr'.
 * NOTE: The key is the same as the ia_'addr' field.
 */
/* ARGSUSED */
int
inaddr_subnetmask_match(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
        struct in_ifaddr *ia;
        struct in_addr *addr;
        int match = 0;

        if (h->flags & HTF_INET) { /* internet address */

	    ia = (struct in_ifaddr *)h;
	    addr = (struct in_addr *)key;

	    if ((addr->s_addr & ia->ia_subnetmask) == ia->ia_subnet) {
		match = 1;
	    }
        }
        return match;
}


/***********************************************************************
 *
 * osi_SameSubNet(addrp):	Check if the address passed in belongs 
 *			to a subnet which is configured on any of
 *			our interfaces.
 *
 ***********************************************************************/

int osi_SameSubNet(struct in_addr *addrp)
{
    register struct in_ifaddr *ia;

    ia = (struct in_ifaddr *)hash_lookup(&hashinfo_inaddr,
		    inaddr_subnetmask_match,
		    (caddr_t)addrp,
		    (caddr_t)0,
		    (caddr_t)0);

    return (ia ? 1 : 0);
}


/*
 * This match procedure is called after the hashing lookup has indexed
 * into the hash table and is comparing hash bucket entries for a match
 * with the supplied key (IP address). The ia_bnetmask field of the
 * 'struct in_ifaddr' is applied with a bitwise 'AND' to the s_addr
 * field of the key.  We return the non-zero if the result matches the
 * ia_net field of the 'struct in_ifaddr'.
 * NOTE: The key is the same as the ia_'addr' field.
 */
/* ARGSUSED */
int
inaddr_netmask_match(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
        struct in_ifaddr *ia;
        struct in_addr *addr;
        int match = 0;

        if (h->flags & HTF_INET) { /* internet address */

	    ia = (struct in_ifaddr *)h;
	    addr = (struct in_addr *)key;

	    if ((addr->s_addr & ia->ia_netmask) == ia->ia_net) {
		match = 1;
	    }
        }
        return match;
}


/***********************************************************************
 *
 * osi_SameNet(addrp):	Check if the address passed in belongs to
 *			a network which is configured on any of
 *			our interfaces.
 *
 ***********************************************************************/

int osi_SameNet(struct in_addr *addrp)
{
    register struct in_ifaddr *ia;

    ia = (struct in_ifaddr *)hash_lookup(&hashinfo_inaddr,
		    inaddr_netmask_match,
		    (caddr_t)addrp,
		    (caddr_t)0,
		    (caddr_t)0);

    return (ia ? 1 : 0);
}

#else /* KERNEL */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <unistd.h>
/*
 * User space functions to help us along
 */
#define MAX_IFS 32
static struct sockaddr_in	myaddress = {0,0,0,0};
static struct sockaddr_in	mysubnetmask = {0.0,0,0};
static struct sockaddr_in	mynetmask = {0,0,0,0};
static char			myIfName[IFNAMSIZ];

/***********************************************************************
 *
 * osi_SetupIfInfo(): Get the interface information from the kernel
 * 	that we need for network adjacency computations.
 *
 ***********************************************************************/
/*
 * Some important assumptions:  This will only do AF_INET for now.
 * This will process only one address for now, the address of the primary
 * AF_INET interface that we get by trying to query each interface from
 * the list of configured interfaces that the kernel gives us.
 */
void osi_SetupIfInfo(void)
{
    int 		s, i;
    struct ifreq	*ifrp;
    struct ifconf	ifc;

    /* make a socket for calls */
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	printf("osi_SetupIFInfo: FAILED in socket setup, e=%d\n",errno);
	return;
    }

    /* allocate space for all the interfaces (names mostly) */
    ifrp = (struct ifreq *)osi_Alloc(MAX_IFS * sizeof(struct ifreq));
    ifc.ifc_buf = (caddr_t)ifrp;
    ifc.ifc_len = MAX_IFS * sizeof(struct ifreq);

    /* get all the configured interfaces */
    if (ioctl(s, SIOCGIFCONF, (caddr_t)&ifc) < 0) {
	printf("osi_SetupIFInfo: FAILED in SIOCGIFCONF, e=%d\n",errno);
	return;
    }

    /* figure out which interface supports AF_INET (other than loopback) */
    for (i=0; i<MAX_IFS; i++, ifrp++) {
	if ((ioctl(s, SIOCGIFADDR, (caddr_t)ifrp) == 0) &&
	    (bcmp(ifrp->ifr_name, "lo", 2) != 0)) {
	    break;
	}
    }
    bcopy(ifrp->ifr_name, myIfName, IFNAMSIZ);
    myaddress.sin_addr = ((struct sockaddr_in *)&ifrp->ifr_addr)->sin_addr;

    if (ioctl(s, SIOCGIFNETMASK, (caddr_t)ifrp) < 0) {
	printf("osi_SetupIFInfo: FAILED in SIOCGIFNETMASK e=%d\n",errno);
	return;
    }
    mysubnetmask.sin_addr = ((struct sockaddr_in *)&ifrp->ifr_addr)->sin_addr;
    osi_Free(ifc.ifc_buf, MAX_IFS * sizeof(struct ifreq));

    if (IN_CLASSA(myaddress.sin_addr.s_addr)) {
	mynetmask.sin_addr.s_addr = IN_CLASSA_NET;
    } else if (IN_CLASSB(myaddress.sin_addr.s_addr)) {
	mynetmask.sin_addr.s_addr = IN_CLASSB_NET;
    } else if (IN_CLASSC(myaddress.sin_addr.s_addr)) {
	mynetmask.sin_addr.s_addr = IN_CLASSC_NET;
    } else
	mynetmask.sin_addr.s_addr = 0;

    (void) close(s);
    return;
}


/***********************************************************************
 *
 * osi_SameHost(addrp):	Check if the address passed in is ours.
 *
 ***********************************************************************/

int osi_SameHost(struct in_addr *addrp)
{
    if (myaddress.sin_addr.s_addr == 0)
	osi_SetupIfInfo();

    if (myaddress.sin_addr.s_addr == addrp->s_addr)
	return(1);
    return(0);
}

/***********************************************************************
 *
 * osi_SameSubNet(addrp): Check if the address passed in belongs 
 *			to our subnet.
 *
 ***********************************************************************/

int osi_SameSubNet(struct in_addr *addrp)
{
    unsigned long	subnet;

    if (myaddress.sin_addr.s_addr == 0)
	osi_SetupIfInfo();

    subnet = myaddress.sin_addr.s_addr & mysubnetmask.sin_addr.s_addr;
    if ((addrp->s_addr & mysubnetmask.sin_addr.s_addr) == subnet)
	return(1);
    return(0);
}

/***********************************************************************
 *
 * osi_SameNet(addrp): Check if the address passed in belongs to
 *			our network.
 *
 ***********************************************************************/

int osi_SameNet(struct in_addr *addrp)
{
    unsigned long	net;
    
    if (myaddress.sin_addr.s_addr == 0)
	osi_SetupIfInfo();

    net = myaddress.sin_addr.s_addr & mynetmask.sin_addr.s_addr;
    if ((addrp->s_addr & mynetmask.sin_addr.s_addr) == net)
	return(1);
    return(0);
}

#endif /* KERNEL */
