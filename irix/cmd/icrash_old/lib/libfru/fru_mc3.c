/*
 * fru_mc3.c-
 *
 *      This file contains the code to analyze an MC3 board, the memory,
 * and the ASICs connected to it.
 *
 */

#include "evfru.h"			/* FRU analyzer definitions */

#include <sys/EVEREST/mc3.h>            /* MA chip */

#include "fru_pattern.h"

int addr2sbs(uint bloc, uint offset, uint *slot, uint *bank, uint *simm,
	 evcfginfo_t *ec);
int extract_bank(evcfginfo_t *ec, unsigned int addrlo, unsigned int addrhi);

void check_mc3(evbrdinfo_t *eb, everror_t *ee, everror_ext_t *eex,
	     whatswrong_t *ww, evcfginfo_t *ec)
{
	int vid;
	int leaf;
	int e_error, l_error;
	fru_element_t src, dest;

#ifdef DEBUG
	if (fru_debug)
		FRU_PRINTF("Check MC3\n");
#endif

	src.unit_type = FRU_MCBOARD;
	src.unit_num = NO_UNIT;

	vid = eb->eb_mem.eb_mc3num;

#ifdef DEBUG
	if (fru_debug)
		FRU_PRINTF("  VID %d\n", vid);
#endif

	e_error = ee->mc3[vid].ebus_error;

#ifdef DEBUG
	if (fru_debug)
		FRU_PRINTF("ebus error == 0x%x\n", e_error);
#endif

	conditional_update(ww, WITNESS, e_error,
			   MC3_EBUS_ERROR_ADDR | MC3_EBUS_ERROR_DATA,
			   &src, NO_ELEM);

	conditional_update(ww, PROBABLE, e_error,
		   MC3_EBUS_ERROR_SENDER_ADDR,
		   &src, NO_ELEM);

	conditional_update(ww, POSSIBLE, e_error,
		   MC3_EBUS_ERROR_SENDER_DATA,
		   &src, NO_ELEM);

	/*
	 * If MC3_EBUS_ERROR_DATA is set, we may just be propagating bad
	 * data.
	 */
	if ((e_error & (MC3_EBUS_ERROR_DATA | MC3_EBUS_ERROR_SENDER_DATA)) ==
		MC3_EBUS_ERROR_SENDER_DATA)
		    update_confidence(ww, PROBABLE, &src, NO_ELEM);

	src.unit_type = FRU_BANK;

	for (leaf = 0; leaf < 2; leaf++) {
		int bank;
#ifdef DEBUG
		if (fru_debug)
			FRU_PRINTF("     Leaf %d\n", leaf);
#endif

		l_error = ee->mc3[vid].mem_error[leaf];

		if (!l_error)
			continue;

#ifdef DEBUG
		if (fru_debug)
			FRU_PRINTF("      error == 0x%x\n", l_error);
#endif

		bank = extract_bank(ec, ee->mc3[vid].erraddrlo[leaf],
			     ee->mc3[vid].erraddrhi[leaf]);


		/* If we can't decode the bank, blame the board! */
		if (bank == NO_UNIT) {
			src.unit_type = FRU_MCBOARD;
		} else {
			src.unit_num = bank;
		}

		/*
		 * Single bit errors don't panic the machine so we'll
		 * only flag this as POSSIBLE.
		 */
		conditional_update(ww, POSSIBLE, l_error,
				   MC3_MEM_ERROR_READ_SBE, &src, NO_ELEM);

		if (e_error & MC3_EBUS_ERROR_SENDER_DATA) {
		    int level;
		    /*
		     * If MC3_EBUS_ERROR_DATA is set, we may just be
		     * propagating * bad data.  We can't definitively claim
		     * to be the source of the error.
		     */
		    if (e_error & MC3_EBUS_ERROR_DATA)
			level = POSSIBLE;
		    else
			level = DEFINITE;

		    conditional_update(ww, level,
			    l_error, MC3_MEM_ERROR_MULT
			    | MC3_MEM_ERROR_READ_MBE
			    | MC3_MEM_ERROR_PWRT_MBE, &src, NO_ELEM);
		} else {
		    /* If _neither_ MC3_EBUS_ERROR_DATA nor MC3_EBUS_ERROR_DATA
		     * is set, we likely have an MA problem.
		     */
		     
		    dest = src;
		    src.unit_type = FRU_MCBOARD;

		    /*
		     * Since the board doesn't admit to causing the problem,
		     * flag this as POSSIBLE.  For all we know, the
		     *  MC3_EBUS_ERROR_DATA bit should also be set.
		     */
		    conditional_update(ww, PROBABLE,
			    l_error, MC3_MEM_ERROR_MULT
			    | MC3_MEM_ERROR_READ_MBE
			    | MC3_MEM_ERROR_PWRT_MBE, &src, &dest);
			
		}
	}				

#ifdef DEBUG
	if (fru_debug)
		display_whatswrong(ww, ec);
#endif

	return;
}


int
addr2sbs(uint bloc, uint offset, uint *slot, uint *bank, uint *simm,
	 evcfginfo_t *ec)
{
    int s, l, b;
    short enable, simm_type, i_factor, i_position;
    uint base_bloc;
    evbrdinfo_t *brd;
    evbnkcfg_t *bnk;

    for (s = 0; s < EV_MAX_SLOTS; s++) {
	brd = &(ec->ecfg_board[s]);

	if (brd->eb_type == EVTYPE_MC3) {
#ifdef DECODE_DEBUG
	    FRU_PRINTF("Board %d is memory\n", s);
#endif
	    /* It's a memory board, do leaf loop */
	    for (l = 0; l < MC3_NUM_LEAVES; l++) {
#ifdef DECODE_DEBUG
		FRU_PRINTF("leaf %d\n", l);
#endif
	        for (b = 0; b < MC3_BANKS_PER_LEAF; b++) {

		    bnk = &(brd->eb_banks[l * MC3_BANKS_PER_LEAF + b]);
#ifdef DECODE_DEBUG
		    FRU_PRINTF("bank %d\n", b);
#endif
		/* Nonzero means enabled */
		    enable = bnk->bnk_enable;
#ifdef DECODE_DEBUG
		    FRU_PRINTF("Enable = %x\n", enable);
#endif
		    simm_type = bnk->bnk_size;
#ifdef DECODE_DEBUG
		    FRU_PRINTF("Type = %x\n", simm_type);
#endif
		    i_factor = bnk->bnk_if;
		    i_position = bnk->bnk_ip;
		    base_bloc = bnk->bnk_bloc;
#ifdef DECODE_DEBUG
		    FRU_PRINTF("Base bloc = %x\n", base_bloc);
#endif
		    if (enable && in_bank(bloc, offset, base_bloc, i_factor,
					i_position, simm_type)) {
			/* We have a winner! */
			*slot = s;
			/* Banks are in order within the slots */
#ifdef DECODE_DEBUG
			FRU_PRINTF("Winner is slot %d, leaf %d, bank %d\n",
						s, l, b);
#endif
			*bank = b + (l * MC3_BANKS_PER_LEAF);
			return 0;
#ifdef DECODE_DEBUG
		    } else {
			FRU_PRINTF("Disabled!\n");
#endif
		    } /* if enable... */
		}
	    }
	} /* If mem_slots... */
    } /* for s */

    return -1;
}


int extract_bank(evcfginfo_t *ec, unsigned int addrlo, unsigned int addrhi)
{
    long long address;
    unsigned int bloc_num, slot, bank, simm;

#ifdef DECODE_DEBUG
    FRU_PRINTF("addrhi == 0x%x, addrlo == 0x%x\n", addrhi, addrlo);
#endif

    address = (long long)addrhi << 32LL;
    address |= addrlo;

#ifdef DECODE_DEBUG
    FRU_PRINTF("address == 0x%llx\n", address);
#endif

    bloc_num = (addrlo >> 8) | ((addrhi & 0xff) << 24);

#ifdef DECODE_DEBUG
    FRU_PRINTF("bloc_num = 0x%x\n", bloc_num);
#endif

    if (addr2sbs(bloc_num, address & 0xff, &slot, &bank, &simm, ec))
	bank = NO_UNIT;

#ifdef DECODE_DEBUG
    FRU_PRINTF("Bank == %d\n");
#endif

#ifdef DEBUG
    if (fru_debug)
	    FRU_PRINTF("      address = 0x%llx\n", address);
#endif

    return bank;
}


char
bank_letter(uint leaf, uint bank) {

    if ((leaf > 1) || (bank > 3))
	return '?';

    return 'A' + leaf + (bank << 1);
}

#ifdef FRU_PATTERN_MATCHER

int match_mc3leaf(fru_entry_t **token, char vid, everror_t *ee, evcfginfo_t *ec,
		  whatswrong_t *ww, fru_case_t *case_ptr)
{
    fru_entry_t *cur_token;
    char fru = 0;
    char match = 1;
    char leaf;
    short bank;

    for (leaf = 0; leaf < 2; leaf++) {
	cur_token = *token;
	fru = 0;

	for (cur_token = next_token(cur_token);
		 !is_control_token(cur_token);
		 cur_token = next_token(cur_token)) {
	
	    if (fru |= is_fru_token(cur_token)) {
#ifdef DEBUG
		if (fru_debug)
		    FRU_PRINTF("FRU would be MC3 leaf\n");
#endif
		continue;
	    }

	} /* for !is_control_token() */

	if (fru && match) {
		/*
		 * If someone else claimed the error already, we don't know the
		 * FRU unit.
		 */
		if (ww->src.unit_type == FRU_BANK) {
		    bank = NO_UNIT;
		} else {
		    bank = extract_bank(ec, ee->mc3[vid].erraddrlo[leaf],
					     ee->mc3[vid].erraddrhi[leaf]);
		}

		update_pattern(ww, case_ptr, FRU_BANK, bank);
	}

	if (cur_token->entry_type != ENDSUBUNIT)
		FRU_PRINTF("Token should have been ENDSUBUNIT!\n");
	else
		/* Consume ENDSUBUNIT token. */
		cur_token = next_token(cur_token);

    }

    *token = cur_token;

    if (match && fru)
	return FRU_MATCH_FRU;
    else if (match)
	return FRU_MATCH_YES;
    else
	return FRU_MATCH_NO;
}


int match_mc3board(fru_entry_t **token, everror_t *ee, evcfginfo_t *ec,
		   whatswrong_t *diag, fru_case_t *case_ptr)
{
    evbrdinfo_t *eb;
    int slot;
    int match = 1;
    fru_entry_t *cur_token;
    char vid;
    char fru = 0;
    char exists = 0;
    whatswrong_t *ww;

    for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
	eb=&(ec->ecfg_board[slot]);
	if (eb->eb_type == EVTYPE_MC3) {
	    ww = diag + slot;
	    cur_token = *token;
	    vid = eb->eb_mem.eb_mc3num;
	    exists = 1;

	    for (cur_token = next_token(cur_token);
		     !is_control_token(cur_token);
		     cur_token = next_token(cur_token)) {
		if (fru |= is_fru_token(cur_token)) {
#ifdef DEBUG
		    if (fru_debug)
			FRU_PRINTF("FRU would be MC3 _BOARD_ (MA?)\n");
#endif

		    update_pattern(ww, case_ptr, FRU_MCBOARD, NO_UNIT);
			
		    continue;
		}

		if (cur_token->entry_type != MA_EBUSERR) {
		    FRU_PRINTF("Bad token.  Should be MA_EBUSERR, ENDBOARD, or"
			   "BEGINSUBUNIT\n");
		    return -1;
		}

		if ((ee->mc3[vid].ebus_error & cur_token->mask) !=
			cur_token->value) {
		    match = 0;
/* return ? */
		}
	    }

	    if (cur_token->entry_type == ENDBOARD) {
		if (cur_token->value != EVTYPE_MC3) {
		    FRU_PRINTF("Bad board type on ENDBOARD\n");
		    return -1;
		}
	    } else if (cur_token->entry_type != BEGINSUBUNIT) {
		FRU_PRINTF("Bad control token.  Should be ENDBOARD or"
			" BEGINSUBUNIT\n");
		return -1;
	    } else { /* IS BEGINSUBUNIT */
		if (cur_token->value != SUBUNIT_LEAF) {
			FRU_PRINTF("Bad subunit type.\n");
		}

		if (match_mc3leaf(&cur_token, vid, ee, ec, &diag[slot],
				  case_ptr)) {
		}
		
	    }
	}
    }

    *token = find_board_end(cur_token);

    if (match && exists)
	return 1;
    else
	return 0;
}

#endif /* FRU_PATTERN_MATCHER */

