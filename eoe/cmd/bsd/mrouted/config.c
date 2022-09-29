/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: config.c,v 1.12 1998/05/19 20:41:29 wli Exp $
 */


#include "defs.h"

struct ifconf ifc;

/*
 * Query the kernel to find network interfaces that are multicast-capable
 * and install them in the uvifs array.
 */
void
config_vifs_from_kernel()
{
    struct ifreq *ifrp, *ifend;
    register struct uvif *v;
    register vifi_t vifi;
    int n;
    u_int32 addr, dstaddr, mask, subnet;
    short flags;
    int num_ifreq = 32;	/* but note that more than 32 can't be active */

    ifc.ifc_len = num_ifreq * sizeof(struct ifreq);
    ifc.ifc_buf = malloc(ifc.ifc_len);
    while (ifc.ifc_buf) {
	if (ioctl(udp_socket, SIOCGIFCONF, (char *)&ifc) < 0)
	    log(LOG_ERR, errno, "ioctl SIOCGIFCONF");

	/*
	 * If the buffer was large enough to hold all the addresses
	 * then break out, otherwise increase the buffer size and
	 * try again.
	 *
	 * The only way to know that we definitely had enough space
	 * is to know that there was enough space for at least one
	 * more struct ifreq. ???
	 */

	if ((num_ifreq * sizeof(struct ifreq)) >
	     ifc.ifc_len + sizeof(struct ifreq))
	     break;

	num_ifreq *= 2;
	ifc.ifc_len = num_ifreq * sizeof(struct ifreq);
	ifc.ifc_buf = realloc(ifc.ifc_buf, ifc.ifc_len);
    }
    if (ifc.ifc_buf == NULL)
	log(LOG_ERR, ENOMEM, "config_vifs_from_kernel: ran out of memory");

    ifrp = (struct ifreq *)ifc.ifc_buf;
    ifend = (struct ifreq *)(ifc.ifc_buf + ifc.ifc_len);
    /*
     * Loop through all of the interfaces.
     */
    for (; ifrp < ifend; ifrp = (struct ifreq *)((char *)ifrp + n)) {
	struct ifreq ifr;
#if BSD >= 199006
	n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
	if (n < sizeof(*ifrp))
	    n = sizeof(*ifrp);
#else
	n = sizeof(*ifrp);
#endif
	/*
	 * Ignore any interface for an address family other than IP.
	 */
	log(LOG_DEBUG, 0, "found %s",ifrp->ifr_name);
	if (ifrp->ifr_addr.sa_family != AF_INET) {
	    log(LOG_DEBUG,0, "skipping %s; not AF_INET",ifrp->ifr_name);
	    continue;
	}

	addr = ((struct sockaddr_in *)&ifrp->ifr_addr)->sin_addr.s_addr;

	/*
	 * Need a template to preserve address info that is
	 * used below to locate the next entry.  (Otherwise,
	 * SIOCGIFFLAGS stomps over it because the requests
	 * are returned in a union.)
	 */
	bcopy(ifrp->ifr_name, ifr.ifr_name, sizeof(ifr.ifr_name));

	/*
	 * Ignore loopback interfaces and interfaces that do not support
	 * multicast.
	 */
	if (ioctl(udp_socket, SIOCGIFFLAGS, (char *)&ifr) < 0)
	    log(LOG_ERR, errno, "ioctl SIOCGIFFLAGS for %s", ifr.ifr_name);
	flags = ifr.ifr_flags;

	if ((flags & (IFF_LOOPBACK|IFF_MULTICAST)) != IFF_MULTICAST) {
		log(LOG_DEBUG,0,"skipping %s, flags=%x",ifr.ifr_name, ifr.ifr_flags);
		continue;
	}


        if (flags & IFF_POINTOPOINT) {
            	if (ioctl(udp_socket, SIOCGIFDSTADDR, (char *)&ifr) < 0 )
                	log(LOG_ERR, errno, "ioctl SIOCGIFDSTADDR for %s", ifr.ifr_name);
            	dstaddr = ((struct sockaddr_in *)&ifr.ifr_dstaddr)->sin_addr.s_addr;
        }

	/*
	 * Ignore any interface whose address and mask do not define a
	 * valid subnet number, or whose address is of the form {subnet,0}
	 * or {subnet,-1}.
	 */
        if (flags & IFF_POINTOPOINT) {
           	/*
            	* for IFF_POINTOPOINT interface, mask out the
            	* host bits only. any other masks are  meanless.
           	* mask = 0xffffff00.
            	*/
		mask = IN_CLASSC_NET;
        } else {
		if (ioctl(udp_socket, SIOCGIFNETMASK, (char *)&ifr) < 0)
	    		log(LOG_ERR, errno, "ioctl SIOCGIFNETMASK for %s", ifr.ifr_name);
		mask = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
	}
	subnet = addr & mask;
	if (!inet_valid_subnet(subnet, mask) ||
	    addr == subnet ||
	    addr == (subnet | ~mask)) {
	    log(LOG_WARNING, 0,
		"ignoring %s, has invalid address (%s) and/or mask (%s)",
		ifr.ifr_name, inet_fmt(addr, s1), inet_fmt(mask, s2));
	    continue;
	}

	/*
	 * Ignore any interface that is connected to the same subnet as
	 * one already installed in the uvifs array.
	 */
	for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	    if (strcmp(v->uv_name, ifr.ifr_name) == 0) {
		log(LOG_DEBUG, 0, "skipping %s (%s on subnet %s) (alias for vif#%u?)",
			v->uv_name, inet_fmt(addr, s1),
			inet_fmts(subnet, mask, s2), vifi);
		break;
	    }
	    if ((addr & v->uv_subnetmask) == v->uv_subnet ||
		(v->uv_subnet & mask) == subnet) {
		log(LOG_WARNING, 0, "ignoring %s, same subnet as %s",
					ifr.ifr_name, v->uv_name);
		break;
	    }
	}
	if (vifi != numvifs) continue;

	/*
	 * If there is room in the uvifs array, install this interface.
	 */
	if (numvifs == MAXVIFS) {
	    log(LOG_WARNING, 0, "too many vifs, ignoring %s", ifr.ifr_name);
	    continue;
	}
	v  = &uvifs[numvifs];
	v->uv_flags       = 0;
	v->uv_metric      = DEFAULT_METRIC;
	v->uv_admetric	  = 0;
	v->uv_rate_limit  = DEFAULT_PHY_RATE_LIMIT;
	v->uv_threshold   = DEFAULT_THRESHOLD;
        if (flags & IFF_POINTOPOINT)
            v->uv_lcl_addr    = dstaddr;    
        else                               
            v->uv_lcl_addr    = addr;
	v->uv_rmt_addr    = 0;
	v->uv_dst_addr	  = dvmrp_group;
	v->uv_subnet      = subnet;
	v->uv_subnetmask  = mask;
	v->uv_subnetbcast = subnet | ~mask;
	strncpy(v->uv_name, ifr.ifr_name, IFNAMSIZ);
	v->uv_prune_lifetime = 0;
	v->uv_groups      = NULL;
	v->uv_neighbors   = NULL;
	v->uv_acl         = NULL;
	v->uv_addrs	  = NULL;
	v->uv_filter	  = NULL;
	NBRM_CLRALL(v->uv_nbrmap);

	if (flags & IFF_POINTOPOINT)
	    v->uv_flags |= VIFF_REXMIT_PRUNES;

	log(LOG_INFO,0,"installing %s (%s on subnet %s) as vif #%u - rate=%d",
	    v->uv_name, inet_fmt(addr, s1), inet_fmts(subnet, mask, s2),
	    numvifs, v->uv_rate_limit);

	++numvifs;

	/*
	 * If the interface is not yet up, set the vifs_down flag to
	 * remind us to check again later.
	 */
	if (!(flags & IFF_UP)) {
	    v->uv_flags |= VIFF_DOWN;
	    vifs_down = TRUE;
	}
    }
}
