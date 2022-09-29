/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _KSYS_PRIOCTL_H
#define _KSYS_PRIOCTL_H

#ident	"$Revision: 1.2 $"

/*
 * Internal defines for implementing procfs ioctls.  These are intended
 * for use by such files as fs/procfs/prioctl.c and cpr/ckpt_procfs.c
 * and any other similar files that are created.  They should not be
 * widely included. 
 */

#define PIOCA_VALID	0x0001	/* Valid ioctl handled here. */
#define PIOCA_WRITE     0x0002  /* Requires a write file descriptor. */
#define PIOCA_PSINFO    0x0004  /* Valid on a psinfo prnode. */
#define PIOCA_GETBUF    0x0008  /* Always get a buffer for this ioctl. */
#define PIOCA_CGETBUF   0x0010  /* Get a buffer if cmdaddr is not NULL. */
#define PIOCA_THREAD    0x0020  /* Valid when directed at a specific thread. */
#define PIOCA_PROC      0x0040  /* Valid when directed at a process as a whole. */
#define PIOCA_ZOMBIE    0x0080  /* Valid when directed at a zombie process. */
#define PIOCA_PRGS      0x0100  /* Data returned is from prgetstats. */
#define PIOCA_MTHREAD   0x0200  /* Select a random thread on multi-threaded */
				/* processes. */
#define	PIOCA_CKPT	0x0400	/* ioctl handled by ckpt */

#define PIOCA_VALIDP    (PIOCA_VALID | PIOCA_PROC)
#define PIOCA_VALIDT    (PIOCA_VALID | PIOCA_THREAD)
#define PIOCA_VALIDMT   (PIOCA_VALID | PIOCA_MTHREAD)
#define PIOCA_VALIDPT   (PIOCA_VALID | PIOCA_PROC | PIOCA_THREAD)

typedef struct pioc_ctx {
	int 	pc_cmd;		/* Ioctl command code. */
        int     pc_tabi;        /* Target abi. */
        caddr_t pc_buf;         /* Address of allocated buffer. */
        int     pc_attr;        /* Attribute flags for ioctl command. */
#if (_MIPS_SIM == _ABIO32) 
        int     pc_kabi;        /* Kernel abi. */
#endif
} pioc_ctx_t;


#endif /* _KSYS_PRIOCTL_H */
