#ifndef __CORE_OUT_H__
#define __CORE_OUT_H__
#ifdef __cplusplus
extern "C" {
#endif
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986,1989 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.15 $"

#include <sgidefs.h>
#include <sys/types.h>

/*
 * Core file format
 *
 * The coreout struct lives at offset 0 in the core file.
 */
#define CORE_NIDESC	32		/* max # of info descriptors */
#define	CORE_NAMESIZE	80		/* maximum process name length */
#define	CORE_ARGSIZE	80		/* maximum process arguments length */

struct coreout {
	int		c_magic;	/* core magic number */
	int		c_version;	/* corefile version # */
	unsigned	c_vmapoffset;	/* byte offset to start of vmaps */
	int		c_nvmap;	/* # of vmaps */
	char		c_name[CORE_NAMESIZE];
					/* name of process (as in ps) */
	char		c_args[CORE_ARGSIZE];
					/* process arguments (as in ps) */
	int		c_sigcause;	/* signal that caused dump */

	struct idesc {
		unsigned i_offset;	/* byte offset to descriptor */
		unsigned i_len;		/* descriptor length in bytes */
		unsigned i_flags;	/* flags */
	} c_idesc[CORE_NIDESC];		/* information descriptors */
};

#define CORE_MAGIC	0xdeadadb0
#define CORE_MAGIC64	0xdeadad40
#define CORE_MAGICN32	0xbabec0bb
#define	CORE_VERSION1	1

/* map of a virtual space in a process */
struct vmap {
	unsigned v_vaddr;		/* virtual address */
	unsigned v_len;			/* length in bytes */
	unsigned v_offset;		/* offset in bytes from start of file */
	ushort_t   v_flags;		/* flags */
	ushort_t   v_type;		/* type of space */
};

/* vmap for core files from 64 bit processes */
struct vmap64 {
	__uint64_t	v_vaddr;	/* virtual address */
	__uint64_t	v_len;		/* length in bytes */
	__uint64_t	v_offset;	/* offset in bytes from start of file */
	ushort_t	v_flags;	/* flags */
	ushort_t	v_type;		/* type of space */
};

/* v_flags */
#define VDUMPED		0x1		/* space was dumped in core file */
#define VPARTIAL	0x2		/* 1st 4096 bytes of text regions,
					 * to get elf program headers */

/* v_type */
#define VTEXT		1		/* space is text */
#define VDATA		2		/* space is data/bss space */
#define VSTACK		3		/* space is stack */
#define VSHMEM		4		/* space is shared mem */
#define VLIBTEXT	5		/* space is shd lib text (OBSOLETE) */
#define VLIBDATA	6		/* space is shd lib data (OBSOLETE) */
#define	VGRAPHICS	7		/* space is graphics hardware */
#define	VMAPFILE	8		/* space is memory mapped file */
#define VPHYS		9		/* space maps physical I/O space */

struct core_thread_data {
	__uint64_t	thrd_offset;	/* offset to secondary thread data */
	uint_t		nthreads;	/* number of secondary threads */
	uint_t		desc_offset[CORE_NIDESC]; /* desc offsets within thrd data */
	uint_t		prda_offset;	/* prda offset within thrd data */
	uint_t		prda_len;	/* prda length */
};

#define CORE_OUT_H_REV	1		/* -tcl (see os/sig.c) */

/* i_flags values */
#define IVALID		0x1		/* descriptor is valid */

/* indexes into idesc array */
#define	I_GPREGS	0		/* 32 general purpose registers */


#define	I_FPREGS	1		/* 32 floating point registers */

#define I_SPECREGS	2		/* special purpose control registers
					 * int EPC, CAUSE, BADVADDR, MDHI, MDLO
					 * int fpcsr, fpeir
					 */

#define I_SIGHANDLER	3		/* signal handlers
					 * int *signal[MAXSIG]
					 */

#define I_EXDATA	4		/* exec data
					 * int tsize, dsize, bsize
					 */

#define I_THREADDATA	5		/* data for secondary (non-faulting)
					 * threads
					 */

#ifdef __cplusplus
}
#endif
#endif /* !__CORE_OUT_H__ */
