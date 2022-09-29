/*
 *  SpiderTCP Network Daemon
 *
 *  XEROX Echo Control Interfaces
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.echo.c
 *	@(#)echo.c	1.18
 *
 *	Last delta created	17:06:04 2/4/91
 *	This file extracted	14:52:32 11/13/92
 *
 */

#ident "@(#)echo.c	1.18	11/13/92"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/snet/echo_control.h>
#include <streamio.h>
#include <fcntl.h>
#include "debug.h"
#include "utils.h"

extern int	trace, nobuild;

/*NOTUSED*/
int
cf_echotype(argc, argv, fd)
int	argc;
char	**argv;
int	fd;
{
	struct strioctl	io;

	if (argc > 1)
	{
		error("ECHO_TYPE arg count (%d) wrong!", argc);
		return(1);
	}

	if (trace) printf("ioctl(%d, I_STR, ECHO_TYPE);\n", fd);
	if (nobuild) return(0);

	/* @INTERFACE@  ECHO: ioctl ECHO_TYPE */

	io.ic_cmd = ECHO_TYPE;
	io.ic_timout = 0;
	io.ic_len = 0;
	io.ic_dp = (char *) 0;

	if (S_IOCTL(fd, I_STR, &io) < 0)
		exit(syserr("ECHO_TYPE ioctl failed"));

	return(0);
}
