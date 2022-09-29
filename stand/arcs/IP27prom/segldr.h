/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.7 $"

#ifndef	_SEGLDR_H_
#define	_SEGLDR_H_

#include <sys/SN/promhdr.h>

int		segldr_check(promhdr_t *ph);
int		segldr_list(promhdr_t *ph);
int		segldr_lookup(promhdr_t *ph, char *seg_name);
int		segldr_load(promhdr_t *ph, int segnum,
			    __uint64_t alt_data, __uint64_t alt_load,
			    __uint64_t tmpbuf, __uint64_t tmpsize);
int		segldr_jump(promhdr_t *ph, int segnum,
			    __uint64_t alt_entry,
			    __uint64_t arg,
			    __uint64_t sp);
int		segldr_old_ioprom(promhdr_t *, promseg_t *) ;

#endif /* _SEGLDR_H_ */
