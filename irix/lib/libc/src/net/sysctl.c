/* wrapper to emulate parts of the 4.4BSD-Lite sysctl() system call
 */

#ident "$Revision: 1.3 $"

#ifdef __STDC__
	#pragma weak sysctl = _sysctl
#endif
#include "synonyms.h"

#include <unistd.h>
#include <errno.h>
#include <bstring.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/route.h>
#include <net/soioctl.h>
#include <cap_net.h>


#ifndef  _LIBC_ABI
int
sysctl(int *name,
       u_int namelen,
       void *oldp,
       size_t *oldlenp,
       void *newp,
       size_t newlen)
{
	struct rtsysctl ctl;
	int rt_sock;
	int serrno;
	struct utsname utsname;


	if (namelen < 2 || namelen > CTL_MAXNAME) {
		errno = EINVAL;
		return -1;
	}

	if (name[0] == CTL_NET) {
		rt_sock = cap_socket(AF_ROUTE, SOCK_RAW, 0);
		if (rt_sock < 0)
			return -1;

		ctl.name = name;
		ctl.namelen = namelen;
		ctl.oldp = oldp;
		ctl.oldlen = *oldlenp;
		ctl.newp = newp;
		ctl.newlen = newlen;
		if (0 > ioctl(rt_sock, _SIOCRTSYSCTL, &ctl)) {
			serrno = errno;
			close(rt_sock);
			errno = serrno;
			return -1;
		}
		*oldlenp = ctl.oldlen;
		(void)close(rt_sock);
		return 0;
	}


	if (name[0] == CTL_KERN) {
		switch (name[1]) {
		case KERN_VERSION:
			if (newp != 0) {
				errno = EPERM;
				return -1;
			}
			if (oldp == 0) {
				*oldlenp = SYS_NMLN;
				return 0;
			}
			if (*oldlenp < SYS_NMLN) {
				errno = ENOMEM;
				return -1;
			}
			if (0 > uname(&utsname))
				return -1;
			bcopy(utsname.version, oldp, SYS_NMLN);
			return 0;

		default:
			errno = EOPNOTSUPP;
			return -1;
		}
	}

	errno = EOPNOTSUPP;
	return -1;
}
#endif /* _LIBC_ABI */
