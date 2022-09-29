/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.20 $"

#include <sys/types.h>
#include <sys/reg.h>
#include <sys/cmn_err.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/io4.h>

extern int interlvmem;	/* Memory interleave factor */

#ifdef MC3_CFG_READ_WAR
lock_t	mc3_cfgLock;
#endif

void
mc3_init(void)
{
	int slot, bank;
	int mc3num = 0;
	int num_ip = 0;
	int need_fixup = 0;
	evbnkcfg_t *base_bnk[EV_MAX_MC3S*MC3_NUM_BANKS];
	int interleave = 0;
	evreg_t *regaddr;
	unsigned regval;

#ifdef MC3_CFG_READ_WAR
	initnlock(&mc3_cfgLock, "mc3_cfgLock");
#endif

	for (slot = 1; slot < EV_MAX_SLOTS; slot++) {

		if (EVCFGINFO->ecfg_board[slot].eb_enabled == 0	||
		    EVCFGINFO->ecfg_board[slot].eb_type    != EVTYPE_MC3)
			continue;

		/* initialize here because prom doesn't */
		EVCFGINFO->ecfg_board[slot].eb_mem.eb_mc3num = mc3num;
		mc3num++;

		for (bank = 0; bank < MC3_NUM_BANKS; bank++) {
		    evbnkcfg_t *bnk = &(EVCFGINFO->ecfg_board[slot].eb_banks[bank]);

		    if (bnk->bnk_enable && bnk->bnk_size != MC3_NOBANK &&
		        bnk->bnk_if   >  interleave)
			interleave = bnk->bnk_if;

		    /* Remember where the first bank of an interleave is */
		    if (bnk->bnk_ip == 0  &&  bnk->bnk_count) {
			base_bnk[num_ip] = bnk;
			num_ip++;
		    }
		    if (bnk->bnk_count == 0)
			need_fixup++;
		}

		mc3_error_clear(slot);

		regaddr = EV_CONFIGADDR(slot,0,MC3_EBUSERRINT);
		evintr_connect( regaddr,
				EVINTR_LEVEL_MC3_EBUS_ERROR,
				SPLERROR,
				EVINTR_DEST_MC3_EBUS_ERROR,
				(EvIntrFunc)mc3_intr_ebus_error,
				(void *)(__psint_t)EVINTR_LEVEL_MC3_EBUS_ERROR);
		regval = EV_GET_REG(regaddr);
		EV_SET_REG(regaddr, regval|MC3_ERRINT_ENABLE);
				
		regaddr = EV_CONFIGADDR(slot,0,MC3_MEMERRINT);
		evintr_connect( regaddr,
				EVINTR_LEVEL_MC3_MEM_ERROR,
				SPLERROR,
				EVINTR_DEST_MC3_MEM_ERROR,
				(EvIntrFunc)mc3_intr_mem_error,
				(void *)(__psint_t)EVINTR_LEVEL_MC3_MEM_ERROR);
		regval = EV_GET_REG(regaddr);
		EV_SET_REG(regaddr, regval|MC3_ERRINT_ENABLE);
	}

	interlvmem = 1 << interleave;

	/*
	 *  We want the configuration table to have a correct bnk_count
	 *  for every member of the interleave.  Currently, only the
	 *  first member has the correct bnk_count.
	 */
	if (need_fixup) {
	    for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
		evbrdinfo_t *board = &(EVCFGINFO->ecfg_board[slot]);

		if (board->eb_enabled == 0	||
		    board->eb_type    != EVTYPE_MC3)
			continue;

		for (bank = 0; bank < MC3_NUM_BANKS; bank++) {
		    evbnkcfg_t *bnk = &(board->eb_banks[bank]);
		    int ip_index;

		    if (bnk->bnk_size  != MC3_NOBANK &&
			bnk->bnk_ip    >  0	     &&
			bnk->bnk_count == 0) {
			/*
			 *  Search for the base bank of the interleave,
			 *  and copy the bnk_count info to this entry.
			 */
			for (ip_index = 0; ip_index < num_ip; ip_index++)
			    if (bnk->bnk_bloc == base_bnk[ip_index]->bnk_bloc) {
				bnk->bnk_count = base_bnk[ip_index]->bnk_count;
				break;
			    }
		    }
		}
	    }
	}
}

