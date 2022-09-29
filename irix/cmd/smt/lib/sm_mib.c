/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.7 $
 */

#include <sys/types.h>
#include <sm_mib.h>
#include <smt_mib.h>

#define FMT     ".iso.org.dod.internet.private.enterprises.sgi.fddiMIB"
#define SGI_FMT ".iso.org.dod.internet.private.enterprises.sgi.fddiSGI"

static oid FDDIMIB[] =    { 1, 3, 6, 1, 4, 1, 59, 3 };
static oid SGIFDDIMIB[] = { 1, 3, 6, 1, 4, 1, 59, 4 };

int
underfdditree(oid *name)
{
	if (bcmp(name, FDDIMIB, sizeof(FDDIMIB)))
		return(0);
	return(OIDLEN(FDDIMIB));
}

int
undersgifdditree(oid *name)
{
	if (bcmp(name, SGIFDDIMIB, sizeof(SGIFDDIMIB)))
		return(0);
	return(OIDLEN(SGIFDDIMIB));
}

struct tree *
init_fddimib(char *mfile)
{
	return(init_mib(FMT, FDDIMIB, OIDLEN(FDDIMIB), mfile));
}

struct tree *
init_sgifddimib(char *mfile)
{
	return(init_mib(SGI_FMT,SGIFDDIMIB,OIDLEN(SGIFDDIMIB),mfile));
}

int
get_fddimib(char *buf)
{
	bcopy((char *)FDDIMIB, buf, sizeof(FDDIMIB));
	return(OIDLEN(FDDIMIB));
}

int
get_sgifddimib(char *buf)
{
	bcopy((char *)SGIFDDIMIB, buf, sizeof(SGIFDDIMIB));
	return(OIDLEN(SGIFDDIMIB));
}
