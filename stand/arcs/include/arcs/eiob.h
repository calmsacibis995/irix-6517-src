/*
 * SysExec internal iob structures and definitions
 */

#ifndef _ARCS_EIOB_H
#define _ARCS_EIOB_H

#ident "$Revision: 1.12 $"

#include <arcs/hinv.h>		/* for COMPONENT */
#include <arcs/folder.h>	/* for IOBLOCK */
#include <arcs/fs.h>		/* for FSBLOCK */
#ifdef SN
#include <sys/SN/klconfig.h> /* for klinfo_t */
#endif

/* Extended iob structure 
 * 
 * Only to be used internally by the I/O system 
 *
 *	File system strategy functions get passed 
 *		an FSBLOCK as a parameter
 *	Device driver strategy functions get passed 
 *		an IOBLOCK and a COMPONENT as a parameter
 *
 * Some of these fields are needed by file systems.  When a
 * file is first opened, the appropriate fields are copied
 * to the FSBLOCK so the file system has access to them.
 */
struct eiob {
    IOBLOCK	iob;		/* ARCS-device iob */
    int		(*devstrat)(COMPONENT *, IOBLOCK *);	/* Device driver strategy function */
    COMPONENT	*dev;		/* Component for this device */
#ifdef SN0
    klinfo_t	*klcompt ;      /* Component in SN0 */
#endif
    FSBLOCK	fsb;		/* File system information block */
    int		(*fsstrat)(FSBLOCK *);	/* File system strategy function */
    char	*filename;	/* Name of opened file */
    int		fd;		/* file number */
};

struct eiob *new_eiob(void);
struct eiob *get_eiob(ULONG);
void free_eiob(struct eiob *);

#endif
