/* ARCS error numbers
 */

#ifndef _ARCS_ERRNO_H
#define _ARCS_ERRNO_H

#ident "$Revision: 1.8 $"

#define ESUCCESS		0
#define E2BIG			1
#define EACCES			2
#define EAGAIN			3
#define EBADF			4
#define EBUSY			5
#define EFAULT			6
#define EINVAL 			7
#define EIO			8
#define EISDIR			9
#define EMFILE			10
#define EMLINK			11
#define ENAMETOOLONG		12
#define ENODEV			13
#define ENOENT			14
#define ENOEXEC			15
#define ENOMEM			16
#define ENOSPC			17
#define ENOTDIR			18
#define ENOTTY			19
#define ENXIO			20
#define EROFS			21

/* Needed errnos not in ARCS spec, but currently needed. */
#define EADDRNOTAVAIL		31
#define ETIMEDOUT		32
#define ECONNABORTED		33
#define ENOCONNECT		34

#define ENOSYS  89      /* Function not implemented hack for compile error */

#endif
