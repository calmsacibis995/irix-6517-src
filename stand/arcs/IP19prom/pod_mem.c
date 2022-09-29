/* pod_mem.c - 
 *	Contains functions for testing Everest raw and configured memory.
 *	pod_check_mem: 		Test already configured memory.
 *	pod_check_rawmem: 	Start the BIST and test memory board registers
 */

#include "pod.h"
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/evconfig.h>
#include "pod_failure.h"
#include "prom_memory.h"

int pod_check_mem(uint);
int pod_check_rawmem(int, evbnkcfg_t *[]);
int pod_address_test(uint);
int timed_out(uint, uint);
int decode_address(uint);
int addr2sbs(uint, uint, uint*, uint*, uint*);
char bank_letter(uint, uint);
int disable_slot_bank(uint, uint, evbnkcfg_t *[], int, uint);

/* Check already configured at its configured addresses and in its configured
   interleave factor.
	num_banks:	number of banks in thr configuration array
	banks:	 	bank configuration information
	mem_size:	total configured memory size in blocs
   Returns:
	0 on success
	nonzero on address line failure
*/

int pod_check_mem(uint mem_size)
{
	int status;

	if (status = pod_address_test(mem_size))
		return status;
}

/* pod_address_test - Check all of the address bits up to mem_size.
 *	mem_size = memory size in blocs.
 *	returns the address line at which comparison failed or zero.
 */ 
int pod_address_test(uint mem_size)
{
	uint count, test;	/* Correspond to address lines */
	uint addr;		/* Bloc address */
#if 0
	kxset(1);		/* Turn on the KX bit */
#endif
	for (count = 3; count < 8; count++)
		u64sw(BASE_ADDR, (1 << count), count);
	for (addr = 0x1; addr < mem_size; count++) {
		u64sw(addr + BASE_ADDR, 0, count);
			/* BASE_ADDR is the base memory address in blocs */
		addr = 1 << (count - 7);
	}

	for (count = 3; count < 8; count++) {
		test = u64lw(BASE_ADDR, 1 << count);
		if (count != test) {
			return count;
		}
		u64sw(BASE_ADDR, 1 << count, 0);
	}
	for (addr = 0x1; addr < mem_size; count++) {
		test = u64lw(addr + BASE_ADDR, 0);
		if (count != test) {
			return count;
		}
		u64sw(addr + BASE_ADDR, 0, 0);
		addr = 1 << (count - 7);
	}

#if 0
	kxset(0);		/* Turn off KX bit */
#endif

	return 0;
}


int pod_walk(uint lo, uint hi, int cont, int safe)
{
    uint addr, data;
    uint read_val;
    uint slot, bank, simm;
    char key;
    uint failures = 0;

    if (lo & 3) {
	loprintf("*** The address must be word aligned! \n");
	return EVDIAG_TBD;
    }

    lo &= 0x1fffffff;
    hi &= 0x1fffffff;
    lo |= 0xa0000000;
    hi |= 0xa0000000;
    loprintf("Converting to uncached, unmapped addresses (%x to %x)\n", lo, hi);

    for (addr = lo; addr < hi; addr += 4) {
	if (!(addr & 0x0fff)) {
		loprintf("Testing %x...\r", addr);
		if (pod_poll(0)) {
			pod_getc();
			loprintf("Hit q to quit, other key to continue\n");
			key = pod_getc();
			if (key == 'q')
				return 0;
		}
	}
	data = 0x80000000U;			/* Bit 31 == 1 */
	while (data != 0) {
	    /* The safe version restore what it read.  The other doesn't. */
	    if (safe) {
		read_val = wr_restore(addr, data);
	    } else {
		store_word(addr);
		read_val = load_word(addr);
	   } 

	    if (read_val != data) {
		loprintf("Addr 0x%x: wrote 0x%x, read 0x%x\n", addr, data,
								read_val);
		failures++;
		decode_address(addr & 0x1fffffff);
		if (!cont) {
			loprintf("Hit y to continue, other key to quit\n");
			key = pod_getc();
			if (key == 'q')
			    return EVDIAG_TBD;
		}
	    } /* if read_val */
	    data >>= 1;
	} /* while data */
    } /* for addr */

    if (failures) {
	loprintf("\n*** Detected %d failures.\n", failures);
	return EVDIAG_TBD;
    } else {
	loprintf("Test Passed.               \n");
	return 0;
    }
}


/*
 * timed_out returns zero before the timeout and nonzero after.
 * t0 is the begin time.  t1 is the end time.  Handles wraps properly.
 */
int timed_out(unsigned int t0, unsigned int t1) {
unsigned int cur_time;

	cur_time = load_double_bloc(EV_RTC);
	if (t0 < t1)
		return ((cur_time < t0) || (cur_time > t1));
	else
		return ((cur_time < t0) && (cur_time > t1));
}


int pod_check_rawmem(int num_banks, evbnkcfg_t *bank_arr[])
{
	int i;
	unsigned int time;
	unsigned int timeout;
	int status;
	int someone_failed = 0;
	int leaf;
	int bank;
	
	for (i = 0; i < num_banks; i += 8) {
/* loprintf("Starting BIST on slot %b\n", bank_arr[i]->bnk_slot); */
		/* Enable all banks for BIST */
		EV_SET_CONFIG(bank_arr[i]->bnk_slot, MC3_BANKENB, 0xff);
		EV_SET_CONFIG(bank_arr[i]->bnk_slot, MC3_LEAFCTLENB, 0xf);
		EV_SET_CONFIG(bank_arr[i]->bnk_slot, MC3_BISTRESULT, 0xf);
	}

	time = load_double_bloc(EV_RTC);
	timeout = time + MC3_BIST_TOUT;

	/* Wait for everyone to finish or a timeout */	
	for (i = 0; i < num_banks; i += 8) {
		loprintf(" %a", bank_arr[i]->bnk_slot);
		while ((EV_GET_CONFIG(bank_arr[i]->bnk_slot, MC3_BISTRESULT) & 3)
				&& !(timed_out(time, timeout)))
			;
	}

	for (i = 0; i < num_banks; i++) {
		/* Fetch the BIST result */
		status = EV_GET_CONFIG(bank_arr[i]->bnk_slot, MC3_BISTRESULT);
		status >>= 4;
		leaf = (i >> 2) % 2;
		bank = i % 4;

		/* Check the BIST results for each bank. */
		if (status & (1 << (i & 7))) {
			loprintf("\n*** Self-test FAILED on slot %a, leaf %d, bank %d (%c)\n",
					bank_arr[i]->bnk_slot, leaf, bank,
					bank_letter(leaf, bank));
			bank_arr[i]->bnk_diagval = EVDIAG_BIST_FAILED;
			bank_arr[i]->bnk_enable = 0;
			status = EV_GET_CONFIG(bank_arr[i]->bnk_slot, MC3_BANKENB);

		}

		/* Set or clear the bank enable bit for each bank */
		if ((bank_arr[i]->bnk_size != MC3_NOBANK) &&
						bank_arr[i]->bnk_enable) {
			status = EV_GET_CONFIG(bank_arr[i]->bnk_slot,
								MC3_BANKENB);
			status |= (MC3_BENB(leaf, bank));
/*
loprintf("Turning on slot %d, leaf %d bank %d\n", bank_arr[i]->bnk_slot,
				leaf, bank);
*/
			EV_SET_CONFIG(bank_arr[i]->bnk_slot, MC3_BANKENB,
								status);
		} else {
/*
loprintf("Turning OFF slot %d, leaf %d bank %d\n", bank_arr[i]->bnk_slot,
				leaf, bank);
*/
			status = EV_GET_CONFIG(bank_arr[i]->bnk_slot,
								MC3_BANKENB);
			status &= ~(MC3_BENB(leaf, bank));
			EV_SET_CONFIG(bank_arr[i]->bnk_slot, MC3_BANKENB,
								status);
		}
	}
	return someone_failed;
}


/* in_bank returns 1 if the given address is in the same leaf position
 * as the bank whose interleave factor and position are provided
 * and the address is in the correct range.
 */
int in_bank(uint bloc, uint offset, uint base_bloc, uint i_factor,
					uint i_position, uint simm_type)
{
	uint end_bloc;
	uint cache_line_num;

	end_bloc = base_bloc + MemSizes[simm_type] * MC3_INTERLEAV(i_factor);

#ifdef DEBUG
	loprintf("base = %x, if = %d, ip = %d, type = %d",
		base_bloc, i_factor, i_position, simm_type);
#endif

	/* If it's out of range, return 0 */
	if (bloc < base_bloc || bloc >= end_bloc) {
#ifdef DEBUG
		loprintf("Not here.\n");
		/* Nope. */
#endif
		return 0;
	}

#ifdef DEBUG
	loprintf("End == %x\n", end_bloc);
#endif
	/* Is this bank the right position in the "interleave"?
	 * See if the "block" address falls in _our_ interleave position
	 */
	cache_line_num = (bloc << 1) + (offset >> 7);

	if ((cache_line_num % MC3_INTERLEAV(i_factor)) == i_position) {
#ifdef DEBUG
	loprintf("This bank!\n");
#endif
		/* We're the one */
		return 1;
	}

#ifdef DEBUG
	loprintf("In range, wrong IP.\n");
#endif
	/* Nope. */
	return 0;
}


decode_address(unsigned int paddr)
{
	uint slot, leaf, bank, simm;
	if (addr2sbs(paddr >> 8, paddr & 0xff, &slot, &bank, &simm)) {
		loprintf("*** Unable to decode address %x!\n", paddr);
	} else {
		leaf = bank >> 2;
		bank &= 3;
#ifdef SIMMDECODE
		loprintf("%x decodes to slot 0x%b, leaf %d bank %d (%c), simmm %d\n",
			paddr, slot, leaf, bank, bank_letter(leaf, bank), simm);
#else
		loprintf("%x decodes to slot 0x%b, leaf %d, bank %d (%c)\n",
			paddr, slot, leaf, bank, bank_letter(leaf, bank));
#endif 
	}
}


char bank_letter(uint leaf, uint bank) {
	if ((leaf > 1) || (bank > 3))
		return "?";
	
	return 'A' + leaf + (bank << 1);
}


int disable_slot_bank(uint bloc, uint off, evbnkcfg_t *banks[], int num_banks,
		      uint diagval)
{
	uint slot;
	uint bank;
	uint simm;
	uint b;

	if (addr2sbs(bloc, off, &slot, &bank, &simm))
		return -1;

	for (b = 0; b < num_banks; b += 8) {
		if (banks[b]->bnk_slot == slot) {
			if (banks[b + bank]->bnk_slot != slot) {
				loprintf("*** IP19 prom logic error: disable_slot_bank()!\n");
				return -1;
			}

			if (banks[b + bank]->bnk_enable == 0) {
				loprintf("*** Slot %a, leaf %d, bank %d ALREADY DISABLED!\n",
				slot, bank >> 2, bank & 3);
				return -1;
			}
			banks[b + bank]->bnk_enable = 0;
			banks[b + bank]->bnk_diagval = diagval;
			loprintf("    Disabling slot %a, leaf %d, bank %d\n",
				slot, bank >> 2, bank & 3);
			return 0;
		}
	}
	/* Never found it. */
	return -1;
}


void print_slot_bank(uint bloc, uint off) {
	uint slot;
	uint leaf;
	uint bank;
	uint simm;

	if (addr2sbs(bloc, off, &slot, &bank, &simm))
		loprintf("UNKNOWN BANK!\n");

	leaf = (bank > 3) ? 1 : 0;
	bank &= 3;

	loprintf("slot %a, leaf %d, bank %d (%c)\n",
		slot, leaf, bank, bank_letter(leaf, bank));
}


int addr2sbs(uint bloc, uint offset, uint *slot, uint *bank, uint *simm)
{
    unsigned int mem_slots;
    int s, l, b;
    int index;
    short enable, simm_type, i_factor, i_position;
    uint base_bloc;

    mem_slots = memory_slots();

    for (s = 0; s < EV_MAX_SLOTS; s++) {
	if (mem_slots & (1 << s)) {
#ifdef DEBUG
	    loprintf("Board %b is memory\n", s);
#endif
	    /* It's a memory board, do leaf loop */
	    for (l = 0; l < MC3_NUM_LEAVES; l++) {
#ifdef DEBUG
		loprintf("leaf %b\n", l);
#endif
	        for (b = 0; b < MC3_BANKS_PER_LEAF; b++) {
#ifdef DEBUG
		    loprintf("bank %b\n", l);
#endif
		/* Nonzero means enabled */
		    /* nonzero == enabled */
		    enable = (read_reg(s, MC3_BANKENB) & MC3_BENB(l, b));
#ifdef DEBUG
		    loprintf("Enable = %x\n", enable);
#endif
		    simm_type = read_reg(s, MC3_BANK(l, b, BANK_SIZE));
#ifdef DEBUG
		    loprintf("Type = %x\n", simm_type);
#endif
		    i_factor = read_reg(s, MC3_BANK(l, b, BANK_IF));
		    i_position = read_reg(s, MC3_BANK(l, b, BANK_IP));
		    base_bloc = read_reg(s, MC3_BANK(l, b, BANK_BASE));
#ifdef DEBUG
		    loprintf("Base bloc = %x\n", base_bloc);
#endif
		    if (enable && in_bank(bloc, offset, base_bloc, i_factor,
					i_position, simm_type)) {
			/* We have a winner! */
			*slot = s;
			/* Banks are in order within the slots */
#ifdef DEBUG
			loprintf("Winner is slot %a, leaf %d, bank %d\n",
						s, l, b);
#endif
			*bank = b + (l * MC3_BANKS_PER_LEAF);
			/* Check which simm it's in:
			 * 	Each SIMM holds an eighth of cache line
			 *  so divide by 128/8 bytes (2^(7-3))
			 */
			*simm = (offset >> 4) & 0x3;
			return 0;
#ifdef DEBUG
		    } else {
			loprintf("Disabled!\n");
#endif
		    } /* if enable... */
		}
	    }
	} /* If mem_slots... */
    } /* for s */

    return -1;
}

#if 0
main()
{
	int result;

	/* mem_size in blocs */
	if (result = pod_address_test(0x100)) {
		fail_test();
	} else {
		pass_test();
	}
}
#endif
