
/******************************************************************
 *
 *  SpiderX25 - Configuration utility
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *  X25_SET_SNID.C
 *
 *  X25 Control Interface : registration of subnet_id with class.
 *
 ******************************************************************/

/*
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.x25snid.c
 *	@(#)x25snid.c	1.26
 *
 *	Last delta created	15:02:11 12/1/91
 *	This file extracted	14:52:31 11/13/92
 *
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <streamio.h>
#include <fcntl.h>
#include "debug.h"
#include "utils.h"
#include <stdio.h>

#include <sys/snet/uint.h>
#include <sys/snet/ll_proto.h>
#include <sys/snet/ll_control.h>
#include <sys/snet/x25_proto.h>
#include <sys/snet/x25_control.h>

extern int	trace, nobuild;

#define TIMEOUT 100

struct class {
	char	*name;
	uint8	class;
} classes[] = {
	{"LC_LLC1",	LC_LLC1},
	{"LC_LLC2",	LC_LLC2},
	{"LC_LAPBDTE",	LC_LAPBDTE},
	{"LC_LAPBXDTE",	LC_LAPBXDTE},
	{"LC_LAPBDCE",	LC_LAPBDCE},
	{"LC_LAPBXDCE",	LC_LAPBXDCE},
	{"LC_LAPDTE",	LC_LAPDTE},
	{"LC_LAPDCE",	LC_LAPDCE},
	{0, 0} };

int
cf_x25_set_snid(argc, argv, fd, mux)
int	argc;
char	**argv;
int	fd;
int	mux;
{
	int locSAP, lopSAP;

	/* must have subnet_id and class present */
	if ((argc <= 2) || (argc > 5))
	{
		error("X25_SET_SNID arg count (%d) wrong!", argc);
		error("usage: X25_SET_SNID={snid, class, local-SAP, loop-SAP}");
		return 1;
	}

	/* subnet_id must be present, and must be a
		single alphanumeric character.		*/
	if (strlen(argv[1]) != 1)
	{
		error("usage: X25_SET_SNID={snid, class, local-SAP, loop-SAP}");
		return 1;
	}


	if (trace)
	{
		printf("ioctl(%d, I_STR, N_snident={mux=%d", fd, mux);
		printf(", snid=%s, class=%s", argv[1], argv[2]);
		if (argv[3] == (char *) 0)
			printf(",,})\n");
		else
		{
			printf(", local-SAP=%s", argv[3]);

			if (argv[4] == (char *) 0)
				printf(",})\n");
			else
				printf(", loop-SAP=%s})\n", argv[4]);
		}
	}

	if (nobuild) return 0;

	/* @INTERFACE@  X25: ioctl N_snident */
	{
		struct	strioctl  strio;
		struct	xll_reg   xlreg;
		struct  class    *clp;

		/* Search for class */
		for (clp = classes; clp->name; clp++)
			if (streq(clp->name, argv[2]))
				break;

		/* Error if not found */
		if (!clp->name)
			exit(error("X25_SET_SNID: class %s not known",
				argv[2]));

		/* Assume SAPs already consistency checked
		    in x25config and netcreate.             */
	
		if (argv[3] == (char *) 0)
		{
			/* if no local SAP present, use default. */
			/* N.B. SAPs are ignored for LAPB (by X.25)
				so they are irrelevant. */
			xlreg.lreg.ll_normalSAP = 0x7E;
		}
		else
		{
			if (streq(argv[2], "LC_LLC1") || streq(argv[2],"LC_LLC2"))
			{	/* LLC type class, read the local SAP */
				/* must convert string of 2 hex. digits to uint8 */
				(void)sscanf(argv[3],"%x",&locSAP) ;
				xlreg.lreg.ll_normalSAP   = (uint8) locSAP;
			}
			else
				/* class is LAPB type, SAP irrelevant */
				xlreg.lreg.ll_normalSAP = 0x7E;
		}
	

		if (argv[4] == (char *) 0)
		{
			/* if no loop SAP present, use default. */
			/* N.B. SAPs are ignored for LAPB (by X.25)
				so they are irrelevant. */
			xlreg.lreg.ll_loopbackSAP = 0x70;
		}
		else
		{
			if (streq(argv[2], "LC_LLC1") || streq(argv[2],"LC_LLC2"))
			{
				/* LLC type class, read the loop SAP */
				(void)sscanf(argv[4],"%x",&lopSAP) ;
				xlreg.lreg.ll_loopbackSAP   = (uint8) lopSAP;
			}
			else
				/* class is LAPB type, SAP irrelevant */
				xlreg.lreg.ll_loopbackSAP = 0x70;
		}
	
		/* Test SAPs are not the same */
		if (xlreg.lreg.ll_loopbackSAP == xlreg.lreg.ll_normalSAP)
		{
			exit(error("X25_SET_SNID: snid (%s): loop-SAP same as local-SAP(%x).",
				argv[1], xlreg.lreg.ll_loopbackSAP));
		}

		/* Test local_SAP does not equal zero */
		if (xlreg.lreg.ll_normalSAP == 0)
		{
			exit(error("X25_SET_SNID: snid (%s): local-SAP equals zero.",argv[1]));
		}

		/* Test loop_SAP does not equal zero */
		if (xlreg.lreg.ll_loopbackSAP == 0)
		{
			exit(error("X25_SET_SNID: snid (%s): loop-SAP equals zero.",argv[1]));
		}

		/* Test local_SAP is even */
		if (xlreg.lreg.ll_normalSAP & 1)
		{
			exit(error("X25_SET_SNID: snid (%s): local-SAP is not even.",argv[1]));
		}

		/* Test loop_SAP is even */
		if (xlreg.lreg.ll_loopbackSAP & 1)
		{
			exit(error("X25_SET_SNID: snid (%s): loop-SAP is not even.",argv[1]));
		}
		
		xlreg.lmuxid		  = mux;
		xlreg.lreg.ll_snid	  = snidtox25(argv[1]);
		xlreg.lreg.ll_type        = LL_REG;
		xlreg.lreg.ll_class       = clp->class;

		strio.ic_cmd    = N_snident;
		strio.ic_timout = TIMEOUT;
		strio.ic_len    = sizeof(xlreg);
		strio.ic_dp     = (char *)&xlreg;	

		if (S_IOCTL(fd, I_STR, (char *)&strio) == -1)
			exit(syserr("N_snident ioctl failed"));
	}
	return 0;
}
