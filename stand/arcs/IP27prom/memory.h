/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.3 $"

#ifndef	_MEMORY_H_
#define	_MEMORY_H_

#ifdef _LANGUAGE_C

int	memory_init(char *name,
		    __uint64_t base, __uint64_t length,
		    int premium, int diag_mode);

int	memory_copy_prom(int diag_mode);
int	memory_clear_prom(int diag_mode);
int	memory_init_low(int diag_mode);
int	memory_init_all(nasid_t nasid, int skip_low, char *mem_disable,
		int diag_mode);
int	memory_disable(nasid_t nasid, char *mem_disable);
int	memory_empty_enable(nasid_t nasid);

#endif /* _LANGUAGE_C */

#endif /* _MEMORY_H_ */
