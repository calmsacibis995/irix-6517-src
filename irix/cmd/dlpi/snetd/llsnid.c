
/******************************************************************
 *
 *  SpiderX25 - Configuration utility
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *  LL_SET_SNID.C
 *
 *  LL Control Interface : registration of subnet_id with mux_id.
 *
 ******************************************************************/

/*
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.llsnid.c
 *	@(#)llsnid.c	1.16
 *
 *	Last delta created	15:02:18 12/1/91
 *	This file extracted	14:52:30 11/13/92
 *
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <streamio.h>
#include <fcntl.h>
#include "debug.h"
#include "utils.h"

#include <sys/snet/uint.h>
#include <sys/snet/ll_control.h>
#include <sys/snet/ll_proto.h>
#include <sys/snet/x25_proto.h>
#include <sys/snet/x25_control.h>

extern int	trace, nobuild;

#define TIMEOUT 100

int
cf_ll_set_snid(argc, argv, fd, mux)
int	argc;
char	**argv;
int	fd;
int	mux;
{
	if (argc != 2 || strlen(argv[1]) != 1)
	{
		error("usage: LL_SET_SNID=snid");
		return 1;
	}

	if (trace)
	{
		printf("ioctl(%d, I_STR, LL_SET_SNID={mux=%d", fd, mux);
		printf(", snid=%s})\n", argv[1]);
	}

	if (nobuild) return(0);

	/* @INTERFACE@  LL: ioctl L_SETSNID */
	{
		struct	strioctl  strio;
		struct	ll_snioc  snioc;

		snioc.lli_type = LI_SNID;
		snioc.lli_snid = snidtox25(argv[1]);
		snioc.lli_spare[0] = 0;
		snioc.lli_spare[1] = 0;
		snioc.lli_spare[2] = 0;
		snioc.lli_index = mux;

		strio.ic_cmd    = L_SETSNID;
		strio.ic_timout = TIMEOUT;
		strio.ic_len    = sizeof(snioc);
		strio.ic_dp     = (char *)&snioc;	

		if (trace)
		{
		printf("snid=%x, index=%x\n",snioc.lli_snid,snioc.lli_index);
		}
		if (S_IOCTL(fd, I_STR, (char *)&strio) == -1)
			exit(syserr("L_SETSNID ioctl failed"));
	}
	return 0;
}
