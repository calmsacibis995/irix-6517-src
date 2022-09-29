/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * err_hints.h -- 
 *	Hint Values used as part of the ERROR Message.
 * 	Hint code format : aabbccc
 *	Where:
 *		aa -> Board Type (IP19/IP21/IO4/..)
 *		bb -> Subsystem within Board (CACHE, TLB, ...)
 *		ccc -> Individual test code for the subsystem
 *
 *	Three of them together should point to a FRU in case of errors.
 *          
 */

#include <sys/types.h>

#ifndef _SYS_ERR_HINTS_
#define _SYS_ERR_HINTS_

/* No Board/Subsystem can have a value of 0 */
#define	INVALID_HINT		0x000000

#define	BOARD_HINT_MASK		0xff00000
#define	BOARD_HINT_SHIFT	20

#define	IP19_BOARD_HINT		0x0100000
#define	IP21_BOARD_HINT		0x0200000
#define	MC3_BOARD_HINT		0x0300000
#define	IO4_BOARD_HINT		0x0400000


#define	SUBSYS_HINT_MASK	0x00ff000
#define	SUBSYS_HINT_SHIFT	12
/* IP19 Subsystem Hints */

#define	IP19_A			(IP19_BOARD_HINT | 0x01000)
#define	IP19_D			(IP19_BOARD_HINT | 0x02000)
#define	IP19_CC			(IP19_BOARD_HINT | 0x03000)

#define	IP19_PCACHE		(IP19_BOARD_HINT | 0x04000)
#define	IP19_SCACHE		(IP19_BOARD_HINT | 0x05000)
#define	IP19_R4K		(IP19_BOARD_HINT | 0x06000)
#define	IP19_CACHE		(IP19_BOARD_HINT | 0x07000)
#define	IP19_TLB		(IP19_BOARD_HINT | 0x08000)
#define	IP19_FPU		(IP19_BOARD_HINT | 0x09000)



/* MC3 Subsystem Hints	*/

#define	MC3_MA_HINT		(MC3_BOARD_HINT | 0x01000)
#define	MC3_MD_HINT		(MC3_BOARD_HINT | 0x02000)
#define	MC3_BANK_HINT		(MC3_BOARD_HINT | 0x03000)
#define	MC3_LEAF_HINT		(MC3_BOARD_HINT | 0x04000)
#define	MC3_SIMM_HINT		(MC3_BOARD_HINT | 0x05000)


/* IO4 Subsystem Hints 	*/

#define	IO4_IA			(IO4_BOARD_HINT | 0x01000)
#define	IO4_ID			(IO4_BOARD_HINT | 0x02000)
#define	IO4_MAPRAM		(IO4_BOARD_HINT | 0x03000)
#define	IO4_IBUS		(IO4_BOARD_HINT | 0x04000)

#define	IO4_SCIP		(IO4_BOARD_HINT | 0x0c000)
#define	IO4_SCSI		(IO4_BOARD_HINT | 0x0d000)
#define	IO4_EPC			(IO4_BOARD_HINT | 0x0e000)
#define	IO4_FCHIP		(IO4_BOARD_HINT | 0x0f000)
#define	IO4_VMECC		(IO4_BOARD_HINT | 0x11000)
#define	IO4_DANG		(IO4_BOARD_HINT | 0x12000)
#define	IO4_FCG			(IO4_BOARD_HINT | 0x41000)


/* Data Structures Associated with Hints */
/* Associated with each Subsystem (which should roughly map to individual 
 * directories in the IDE), there would be a file Catalog_dirname.c
 * where the a table of hints, the catalog_function, and catalog_t 
 * datastructure would be defined. 
 */


typedef struct _hint{
	uint	hint_num;
	char	*hint_func;
	char	*hint_msg;
	char	*fru_analysis;
}hint_t;


typedef struct _catfunc {
	uint	*subsystem;	/* array of (hint_num & 0xffff000) */
	void	(*printfn)(void *);
}catfunc_t;


typedef struct _catalog{
	catfunc_t	*cat_fntable;
	hint_t		*hints;
}catalog_t;




#endif
