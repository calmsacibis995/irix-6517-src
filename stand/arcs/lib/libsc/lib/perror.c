/* ARCS error messages, and a perror stubb to print them out.
 */

#ident "$Revision: 1.16 $"
#include <libsc.h>
#include <libsc_internal.h>

static char *_arcs_errors[] = {
	"no error",				/* ESUCCESS */
	"argument list too long",		/* E2BIG */
	"permission denied",			/* EACCESS */
	"resource temporarily unavailable",	/* EAGAIN */
	"bad file descriptor",			/* EBADF */
	"resource busy",			/* EBUSY */
	"bad addresss",				/* EFAULT */
	"invalid argument",			/* EINVAL */
	"IO error",				/* EIO */
	"is a directory",			/* EISDIR */
	"too many open files",			/* EMFILE */
	"too many links",			/* EMLINK */
	"filename too long",			/* ENAMETOOLONG */
	"no such device",			/* ENODEV */
	"no such file or directory",		/* ENOENT */
	"execute format error",			/* ENOEXEC */
	"not enough space",			/* ENOMEM */
	"no space left on device",		/* ENOSPC */
	"not a directory",			/* ENOTDIR */
	"no a tty",				/* ENOTTY */
	"media not loaded",			/* ENXIO */
	"read only filesystem",			/* EROFS */
	0,					/* 22 */
	0,					/* 23 */
	0,					/* 24 */
	0,					/* 25 */
	0,					/* 26 */
	0,					/* 27 */
	0,					/* 28 */
	0,					/* 29 */
	0,					/* 30 */
	"address not available",		/* EADDRNOTAVAIL */
	"timed out",				/* ETIMEDOUT */
	"connection closed",			/* ECONNABORTED */
	"could not connect to server",		/* ENOCONNECT */
	0
};

#define EMAX	34

/* same as libc strerror */
char *
arcs_strerror(long n)
{
	if ((n<0) || (n>EMAX) || !_arcs_errors[n]) {
		static char ebuf[28];
		sprintf(ebuf, "unknown error %d", n);
		return ebuf;
	}
	return(_arcs_errors[n]);
}

void
perror(long err, char *str)
{
	if (str)
		printf("%s: ",str);
	printf("%s\n",arcs_strerror(err));
}
