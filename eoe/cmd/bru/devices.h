/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	devices.h    structures and defines for manipulating device info
 *
 *  SCCS
 *
 *	@(#)devices.h	9.11	5/11/88
 *
 *  SYNOPIS
 *
 *	#include "devices.h"
 *
 *  DESCRIPTION
 *
 *	The first thing bru does when it encounters a read/write
 *	error on an archive is consult an internal device table
 *	for guidance on how to proceed.  For each environment,
 *	this table should contain recovery strategy information
 *	for each common archive device.
 *
 *	If the archive device is not in the table then default
 *	builtin strategies apply.
 */

#if unix || xenix
#ifndef EPERM
#include <errno.h>		/* May need to be <sys/errno.h> for BSD 4.2 */
				/* That won't work for xenix */
#endif
#endif

#define D_TRIES		4	/* Max R/W retries at each error */
#define D_RECUR		3	/* Max recursive error recovery limit */


/*
 *	Bits for the capabilities flags word.
 *
 *	D_ADVANCE means that even when reads or writes fail due to bad
 *	spots, the media advances so that the bad spot is eventually
 *	skipped over by successive read/writes.  This also implies that
 *	the equation <current position> = <previous position> + <bytes in/out>
 *	is not always valid in the presence of media errors.
 *
 */

#define D_REOPEN	000001	/* Close and reopen when at end of device */
#define D_ISTAPE	000002	/* Device is a tape drive */
#define D_RAWTAPE	000004	/* Device is a raw magtape drive */
#define D_NOREWIND	000010	/* No automatic rewind on close of file */
#define D_ADVANCE	000020  /* Read/write always advances media */
#define D_NOREOPEN	000040	/* Explicit no reopen, D_REOPEN obsolete */
#define D_QFWRITE	000100	/* Query before first write to vol */
#define D_EJECT		000200	/* Eject media after use (Mac'sh machines) */
#define D_FORMAT	000400	/* Format the media if necessary */

/*
 *	A partial read/write is one that returns less than nbytes
 *	A zero read/write is one that returns -1
 */

struct device {			/* Known archive devices */
    char *dv_dev;		/* Named system device */
    int dv_flags;		/* Flags for various capabilities */
    long dv_msize;		/* Media size if known */
    unsigned int dv_bsize;	/* Default buffer size for this device */
    int dv_seek;		/* Minimum seek resolution, 0 if no seeks */
    int dv_prerr;		/* Errno for partial reads at end of device */
    int dv_pwerr;		/* Errno for partial writes at end of device */
    int dv_zrerr;		/* Errno for zero reads at end of device */
    int dv_zwerr;		/* Errno for zero writes at end of device */
    int dv_frerr;		/* Errno for unformatted media read attempt */
    int dv_fwerr;		/* Errno for unformatted media write attempt */
    int dv_wperr;		/* Errno for write protected */
};

extern int errno;		/* Error number */
