/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* Things used by clients */

#define HW_COPY_ADDR_NOT_CACHED	HW_COPY_TO_NOT_CACHED
#define HW_COPY_FROM_NOT_CACHED	(1<<0)
#define HW_COPY_TO_NOT_CACHED	(1<<1)
#define HW_COPY_ASYNC		(1<<2)
#define HW_COPY_CAN_SLEEP	(1<<3)

#define HW_COPY_CONTIGUOUS	(1<<4) /* both a restriction and a flag  */

/* Things used by hardware specific modules */

#define HW_COPY_NOT_AVAILABLE 0x00ffffff

typedef struct hw_copy_engine_s {
	/* non-zero from init routine means don't ever use this engine */
	int (*init)( void );
	void (*copy_op)(paddr_t from, paddr_t to, size_t bcount);
	void (*zero_op)(paddr_t addr, size_t bcount);

	int  (*delay)( void );
	void (*spin) ( void );
	unsigned int restrictions;
	unsigned int alignment_mask;
} hw_copy_engine;
