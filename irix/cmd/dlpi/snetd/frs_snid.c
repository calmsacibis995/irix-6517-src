/*
 * Copyright (c) 1995 Spider Systems Limited
 *
 * This Source Code is furnished under Licence, and may not be
 * copied or distributed without express written agreement.
 *
 * All rights reserved.  Made in Scotland.
 *
 * Authors: Fraser Moore
 *
 * frs_snid.c of netd module
 *
 * SpiderFRAME-RELAY
 * Release 1.0.3 95/06/15
 * 
 * 
 */

/*
 * frs_snid.c - sets the ppa value associated with the frs lower stream
 */

#include <sys/types.h>
#include <stropts.h>
#include <fcntl.h>
#include "snid.h"
#include "tune.h"
#include "frsctl.h"

#include "utils.h"

extern int syserr();

extern int trace, nobuild;

/*
 * cf_frs_setsnid() - Netd function to set FRS PPA
 *
 * This function is suitable for use by netd(1M) to set the PPA associated
 * with a lower FRS stream.
 *
 *	argc:		Argument count (must be 2)
 *
 *	argv:		Argument vector (frs_set_ppa, ppaval)
 *
 *	fd:		FSR file descriptor
 *
 *	mux:		Lower stream link identifier
 *
 * Returns: An integer with 0 indicating success, non-zero indicating error.
 */

int cf_frs_setsnid(int argc, char **argv, int fd, int mux)
{
uint32 ppa;

if (argc != 2 || strlen(argv[1]) > SN_ID_LEN)
	{
	error("usage: FRS_SET_SNID=snid", NULL, NULL);
	return(1);
	}

if (trace)
	{
	printf("ioctl(%d, I_STR, FRS_SET_SNID={mux=%d", fd, mux);
	printf(", snid=%s})\n", argv[1]);
	}

if (nobuild) return(0);

ppa = strtosnid(argv[1]);
if (frs_set_snid(fd, mux, ppa) < 0)
	{
	S_EXIT(syserr("FR_SETSNID ioctl failed", NULL, NULL));
	}
return(0);
}
