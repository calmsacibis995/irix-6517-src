#ifdef INET6
/* Part of RFC2133: Get interface name/index via sysctl */

#ifdef __STDC__
        #pragma weak in6addr_any = _in6addr_any
        #pragma weak in6addr_loopback = _in6addr_loopback
	#pragma weak if_indextoname = _if_indextoname
	#pragma weak if_nametoindex = _if_nametoindex
	#pragma weak if_nameindex = _if_nameindex
	#pragma weak if_freenameindex = _if_freenameindex
#endif
#include "synonyms.h"

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <bstring.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <netinet/in.h>  /* for struct in6_addr */
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/*
 * These two are not used by this file but are part of rfc2133 so I define
 * them here.
 */
const struct in6_addr in6addr_any = { 0, 0, 0, 0 };
const struct in6_addr in6addr_loopback = { 0, 0, 0, 1 };

static caddr_t
getifs(size_t *size)
{
	int mib[] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0 };
	caddr_t buf;

	if (sysctl(mib, 6, NULL, size, NULL, 0) < 0)
		return(0);
	if ((buf = malloc(*size)) == NULL)
		return(0);
	if (sysctl(mib, 6, buf, size, NULL, 0) < 0) {
		free(buf);
		return(0);
	}
	return (buf);
}

static struct sockaddr_dl *
get_sdl(struct if_msghdr *ifm)
{
	struct sockaddr_dl *sdl;

	if (ifm->ifm_type != RTM_IFINFO)
		return(NULL);
	if (ifm->ifm_addrs != (1 << RTAX_IFP))
		return(NULL);
	sdl = (struct sockaddr_dl *)(ifm + 1);
	if (sdl->sdl_family != AF_LINK || sdl->sdl_nlen >= IFNAMSIZ)
		return(NULL);
	return (sdl);
}

/* Convert an interface name into its index number */
unsigned int
if_nametoindex(const char *ifname)
{
	struct if_msghdr *ifm;
	caddr_t cp;
	caddr_t buf;
	struct sockaddr_dl *sdl;
	size_t namelen;
	size_t size;

	if ((ifname == NULL) || (*ifname == '\0'))
		return(0);
	if ((buf = getifs(&size)) == NULL)
		return(0);
	for (cp = buf; cp < buf + size; cp += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)cp;

		if ((sdl = get_sdl(ifm)) == NULL)
			continue;
		namelen = strlen(ifname);
		if ((namelen == (int)sdl->sdl_nlen) &&
		  (bcmp(sdl->sdl_data, ifname, namelen) == 0)) {
			free(buf);
			return(sdl->sdl_index);
		}
	}
	free(buf);
	return(0);
}

/* Convert an interface number into the interface name */
char *
if_indextoname(unsigned int ifindex, char *ifname)
{
	struct if_msghdr *ifm;
	caddr_t cp;
	caddr_t buf;
	struct sockaddr_dl *sdl;
	size_t size;

	if (ifindex == 0)
		return(NULL);
	if ((buf = getifs(&size)) == NULL)
		return(NULL);
	for (cp = buf; cp < buf + size; cp += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)cp;

		if ((sdl = get_sdl(ifm)) == NULL)
			continue;
		if (ifindex == (unsigned int)sdl->sdl_index) {
			strncpy(ifname, sdl->sdl_data, (int)sdl->sdl_nlen);
			ifname[sdl->sdl_nlen] = '\0';
			free(buf);
			return(ifname);
		}
		
	}
	free(buf);
	return(NULL);
}

/* return a list of interface name and index pairs */
struct if_nameindex *
if_nameindex(void)
{
	struct if_msghdr *ifm;
	struct if_nameindex *ifni;
	caddr_t cp, rcp;
	caddr_t rbuf;
	int rsize, rcount;
	caddr_t buf;
	struct sockaddr_dl *sdl;
	size_t size;

	if ((buf = getifs(&size)) == NULL)
		return((struct if_nameindex *)0);
	rsize = 0;
	rcount = 0;
	for (cp = buf; cp < buf + size; cp += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)cp;

		if ((sdl = get_sdl(ifm)) == NULL)
			continue;
		rsize += sizeof(struct if_nameindex) + IFNAMSIZ;
		rcount++;
	}
	if (rsize) {
		rsize += sizeof(struct if_nameindex);
		if ((rbuf = malloc(rsize)) == NULL) {
			free(buf);
			return((struct if_nameindex *)0);
		}
		bzero(rbuf, rsize);
		ifni = (struct if_nameindex *)rbuf;
		rcp = (caddr_t)(ifni + rcount + 1);
		for (cp = buf; cp < buf + size; cp += ifm->ifm_msglen) {
			ifm = (struct if_msghdr *)cp;

			if (ifm->ifm_type == RTM_IFINFO) {
				if ((sdl = get_sdl(ifm)) == NULL)
					continue;
				ifni->if_index = (unsigned int)sdl->sdl_index;
				ifni->if_name = rcp;
				strncpy(rcp, sdl->sdl_data,
				  (size_t)sdl->sdl_nlen);
				ifni++;
				rcp += IFNAMSIZ;
			}
		}
		free(buf);
		return((struct if_nameindex *)rbuf);
	}
	free(buf);
	return((struct if_nameindex *)0);
}

void
if_freenameindex(struct if_nameindex *ptr)
{
	free(ptr);
}
#endif
