/**************************************************************************
 *                                                                        *
 * Copyright (C) 1991-1992 Silicon Graphics, Inc.                         *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.19 $"

/*
 * Priority I/O function prototypes and macro definitions
 */

#ifndef _PRIO_SYS_H
#define _PRIO_SYS_H

typedef long long bandwidth_t;

/* These should be the same as FREAD/FWRITE */
#define PRIO_READ_ALLOCATE	0x1
#define PRIO_WRITE_ALLOCATE	0x2
#define PRIO_READWRITE_ALLOCATE	(PRIO_READ_ALLOCATE | PRIO_WRITE_ALLOCATE)

extern int prioSetBandwidth (int		/* fd */,
                             int		/* alloc_type */,
                             bandwidth_t	/* bytes_per_sec */,
                             pid_t *		/* pid */);
extern int prioGetBandwidth (int		/* fd */,
                             bandwidth_t *	/* read_bw */,
                             bandwidth_t *	/* write_bw */);
extern int prioLock (pid_t *);
extern int prioUnlock (void);

/* Error returns */
#define PRIO_SUCCESS     0
#define PRIO_FAIL       -1 

#endif /* _PRIO_SYS_H */
