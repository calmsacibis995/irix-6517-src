/*
 *  SpiderTCP Network Daemon
 *
 *      STREAMS interfaces
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.streams.c
 *	@(#)streams.c	1.22
 *
 *	Last delta created	16:13:33 7/9/92
 *	This file extracted	14:52:26 11/13/92
 *
 *
 */

#ident "@(#)streams.c	1.22	11/13/92"

#include <fcntl.h>
#include <stropts.h>
#include <streamio.h>
#include "debug.h"
#include "utils.h"

extern int	trace, nobuild;

static int	xopen();
static int	xclose();
static int	xlink();

int
doopen(device)
char *device;
{
	int	fd;

	if (trace) printf("open(\"%s\", O_RDWR)", device);
	if (nobuild)
		fd = xopen();
	else
	{
		if ((fd = S_OPEN(device, O_RDWR)) == -1)
		{
			printf("\n");
			exit(syserr("failed to open driver \"%s\"", device));
		}
	}
	printf(" => %d\n", fd);
	return(fd);
}

doclose(fd)
int fd;
{
	if (trace) printf("close(%d)\n", fd);
	if (nobuild) return(xclose(fd));

	return(S_CLOSE(fd));
}

dopush(fd, module)
int	fd;
char	*module;
{
	if (trace) printf("ioctl(%d, I_PUSH, \"%s\")\n", fd, module);
	if (nobuild) return (0);

	if (S_IOCTL(fd, I_PUSH, module) == -1)
		exit(syserr("failed to push module \"%s\"", module));

	return(0);
}

#ifdef SXBUS
int
do_ioctl(fd, cmd, dptr, dlen)
char *dptr;
{
	struct strioctl strblk;
	strblk.ic_cmd = cmd;
	strblk.ic_timout = 0;
	strblk.ic_len = dlen;
	strblk.ic_dp = dptr;

	return ioctl(fd, I_STR, &strblk);
}

/*
 *  For now - define the ioctls locally
 */

#define		IS_PREFIX	0x7d00
#define		IS_LINK		(IS_PREFIX | 1)
#define 	IS_PUSH		(IS_PREFIX | 2)
#define 	IS_GETID	(IS_PREFIX | 3)
#endif /* SXBUS */

int
#ifdef SXBUS
dolink(ufd, lfd, cross_bus)
#else /* SXBUS */
dolink(ufd, lfd)
#endif /* SXBUS */
int ufd, lfd;
{
	int	muxid;

	if (trace) printf("ioctl(%d, I_LINK, %d)", ufd, lfd);
	if (nobuild)
		muxid = xlink();
	else
	{
#ifdef SXBUS
		if (cross_bus)
		{
			if ((muxid = do_ioctl(lfd, IS_GETID, (char *)0, 0)) == -1)
			{
				printf("\n");
				exit(syserr(
				"Failed to get front end ID for stream %d", lfd));
			}

			if ((muxid = do_ioctl(ufd, IS_LINK,
				(char *)&muxid, sizeof(int))) == -1)
			{
				printf("\n");
				exit(syserr(
				"Failed to link stream %d under driver %d",
					lfd, ufd));
			}
		}
		else
#endif /* SXBUS */
		if ((muxid = S_IOCTL(ufd, I_LINK, lfd)) == -1)
		{
			printf("\n");
			exit(syserr("failed to link stream %d under driver %d",
				lfd, ufd));
		}
	}
	printf(" => %d\n", muxid);
	return(muxid);
}

int
doexpmux(ufd, muxid)
int ufd, muxid;
{
	if (muxid && trace) printf("muxid(%d) => %d\n", ufd, muxid);
	return 0;	/* Dummy fd value */
}

/*
 *  Emulate open/close/I_LINK for debugging.
 */

static long	xfds = 0L;

static int
xopen()
{
	int	fd;

	for (fd=0; fd<sizeof(long)*8; fd++)
	{
		if ((xfds&(01<<fd)) == 0L)
		{
			xfds |= (01<<fd);
			return fd;
		}
	}

	return -1;
}

static int
xclose(fd)
int fd;
{
	if (fd < 0 || sizeof(long)*8 <= fd)
		return -1;

	if (xfds&(01<<fd))
	{
		xfds &= ~(01<<fd);
		return 0;
	}

	return -1;
}

static int	xmuxid = 100;

static int
xlink()
{
	return xmuxid++;
}
