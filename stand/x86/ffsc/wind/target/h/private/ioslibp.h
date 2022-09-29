/* iosLibP.h - private I/O system header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,09nov93,rrr  added aio support
01b,22sep92,rrr  added support for c++
01a,23aug92,jcf  written.
*/

#ifndef __INCiosLibPh
#define __INCiosLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "ioslib.h"
#include "dlllib.h"
#include "errnolib.h"

/* typedefs */

typedef struct		/* FD_ENTRY - entries in file table */
    {
    DEV_HDR *	pDevHdr;	/* device header for this file */
    int 	value;		/* driver's id for this file */
    char *	name;		/* actual file name */
    BOOL 	inuse;		/* active entry */
    } FD_ENTRY;

typedef struct		/* DRV_ENTRY - entries in driver jump table */
    {
    FUNCPTR	de_create;
    FUNCPTR	de_delete;
    FUNCPTR	de_open;
    FUNCPTR	de_close;
    FUNCPTR	de_read;
    FUNCPTR	de_write;
    FUNCPTR	de_ioctl;
    BOOL	de_inuse;
    } DRV_ENTRY;

/* macros */

#define	STD_FIX(fd)	((fd)+3)
#define	STD_UNFIX(fd)	((fd)-3)
#define	STD_MAP(fd)	(STD_UNFIX(((fd) >= 0 && (fd) < 3) ? \
						ioTaskStdGet (0, fd) : (fd)))

#define	FD_CHECK(fd)	(((fd) >= 0 && (fd) < maxFiles && fdTable[(fd)].inuse)?\
			&fdTable[(fd)] : (FD_ENTRY *) \
			(errnoSet (S_iosLib_INVALID_FILE_DESCRIPTOR), NULL))

/* variable declarations */

extern DL_LIST 		iosDvList;
extern DRV_ENTRY *	drvTable;
extern FD_ENTRY *	fdTable;
extern int 		maxDrivers;
extern int 		maxFiles;
extern VOIDFUNCPTR	iosFdNewHookRtn;
extern VOIDFUNCPTR	iosFdFreeHookRtn;

#ifdef __cplusplus
}
#endif

#endif /* __INCiosLibPh */
