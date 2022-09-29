/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * racermp.h -- Contains the data structures for communication
 * 	between the Boot master processor and the slave processors,
 *	as well as the structures for communicating between the
 *	kernel and the PROM.
 */

#ifndef __SYS_RACER_RACERMP_H__
#define __SYS_RACER_RACERMP_H__

#ident "$Revision: 1.8 $"

/*
 * The MPCONF data structure is used to communicate between
 * the Boot Master processor and the slave processors.  It
 * resides at a fixed place in virtual memory.
 * the MPCONF array is indexed using the physical processor ID
 */

#define MPCONF_MAGIC	0xBADDEED2
#define	MPCONF_ADDR	(K0BASE+0x600)
#define MPCONF_SIZE  	128

#ifdef LANGUAGE_ASSEMBLY
#define MP_MAGICOFF		0x00
#define MP_PRID			(MP_MAGICOFF +	0x4)
#define MP_PHYSID		(MP_PRID +	0x4)
#define MP_VIRTID		(MP_PHYSID +	0x4)
#define MP_SCACHESZ		(MP_VIRTID +	0x4)
#define MP_FANLOADS		(MP_SCACHESZ +	0x4)
#define MP_UNUSED2		(MP_FANLOADS +	0x2)
#define MP_LAUNCHOFF		(MP_UNUSED2 +	0x2)
#define MP_RNDVZOFF		(MP_LAUNCHOFF +	0x8)
#define MP_UNUSED3		(MP_RNDVZOFF +	0x8)
#define MP_UNUSED4		(MP_UNUSED3 +	0x8)
#define MP_UNUSED5		(MP_UNUSED4 +	0x8)
#define MP_STACKADDR		(MP_UNUSED5 +	0x8)
#define MP_LPARM		(MP_STACKADDR +	0x8)
#define MP_RPARM		(MP_LPARM +	0x8)
#define MP_IDLEFLAG		(MP_RPARM +	0x8)

#endif /* LANGUAGE_ASSEMBLY */

#ifdef _LANGUAGE_C

typedef struct mpconf_blk {	
	uint	mpconf_magic;
	int	pr_id;		/* Processor implementation and revision */
	int	phys_id;	/* Physical ID of the processor */ 
	int	virt_id;	/* CPU virtual ID */	
	int	scache_size;	/* 2nd level cache size for this CPU */
	short	fanloads;	/* CPU 0 set by prom based on XIO config */
	ushort	unused2;
	volatile void *launch;	   /* routine to start bootstrap */
	volatile void *rendezvous; /* all this, when done launching */
	void	*unused3;
	void	*unused4;
	void	*unused5;
	volatile void *stack;	 /* Stack pointer */
	volatile void *lnch_parm;/* User can pass single parm to launch */
	volatile void *rndv_parm;/* User can pass single parm to rendezvous */
	int	idle_flag;	 /* processor in idle loop, standalone */
	int	unused6;
	__uint64_t padto128[4];
} mpconf_t;

#define MPCONF ((mpconf_t *) MPCONF_ADDR)

#endif /* _LANGUAGE_C */

#endif /* __SYS_RACER_RACERMP_H__ */
