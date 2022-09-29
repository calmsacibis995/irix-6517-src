/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.3 $"

#ifndef _SYMBOL_H_
#define _SYMBOL_H_

typedef struct syminfo_s {
	char *sym_name;
	void *sym_addr;
	int sym_size;
} syminfo_t;

	
int	symbol_lookup_name(syminfo_t *sym_info);
int	symbol_lookup_addr(syminfo_t *sym_info);

void	symbol_name_off(void *addr, char *buf);
int	symbol_dis(inst_t *addr);

char   *prom_fetch_kname(inst_t *inst_p, char *buf, int jmp);	/* for disMips.c */

#endif /* _SYMBOL_H_ */
