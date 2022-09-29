#include <stdio.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>

/*
 * 
 * This code is to be used in miniroot instead of using
 * netstat in the mrnetrc script.
 *
 * The problem with using netstat is that is uses NLIST
 * but it is possible that the kernel has not been
 * placed on the /swap filesystem yet.  Thus causing
 * netstat to fail and print "/unix no namelist" error message.
 *
 * The script that calls this routine either expects success (0)
 * or failure (1).  This is the reason why the if a system call 
 * errors the return value is "-1".
 * 
 */
main()
{
	int s, n;
	char buf[BUFSIZ];
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;

	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) 
		return(-1);

	ifc.ifc_len = sizeof (buf);
	ifc.ifc_buf = buf;
	if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) 
		return(-1);

	ifr = ifc.ifc_req;
	for (n = ifc.ifc_len / sizeof (struct ifreq); n > 0; n--, ifr++) {
		if (strncmp(ifr->ifr_name,"lo0",3) != 0) {
			ifreq = *ifr;
			if (ioctl(s, SIOCGIFFLAGS, (char *)&ifreq) < 0)
				return(-1);

			if ((ifreq.ifr_flags & (IFF_UP|IFF_RUNNING)) 
				== (IFF_UP|IFF_RUNNING))
				   return(0);
		}
	}

	/*
	 * At this point the network is not up and running ....
	 */
	return (1);
}
