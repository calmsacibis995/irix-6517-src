/* busLocal.h - sysBusToLocal/sysLocalToBus header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,16jun92,ccc  created.
*/

/*
DESCRIPTION
This file contains header information for the sysBusToLocal() and
sysLocalToBus() routines.
*/

#ifndef __INCbusLocalh
#define __INCbusLocalh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#ifndef	_ASMLANGUAGE
#if defined(__STDC__) || defined(__cplusplus)

IMPORT	STATUS	sysLocalToBusAdrs (int adrsSpace, char *localAdrs,
				   char **pBusAdrs);
IMPORT	STATUS	sysBusToLocalAdrs (int adrsSpace, char *busAdrs,
				   char **pLocalAdrs);

#else	/* __STDC__ */

IMPORT	STATUS	sysLocalToBusAdrs ();
IMPORT	STATUS	sysBusToLocalAdrs ();

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCbusLocalh */
