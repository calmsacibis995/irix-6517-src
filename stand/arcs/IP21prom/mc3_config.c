/***********************************************************************\
*	File:		mc3_config.c					*
*									*
*	Contains various routines which are used to configure and 	*
*	probe the MC3 Memory Controller boards in an Everest system.	*
*									*
\***********************************************************************/

#ident "$Revision: 1.49 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/evconfig.h> 
#include <sys/EVEREST/evintr.h> 
#include <sys/EVEREST/nvram.h>
#include <setjmp.h>
#include "pod_iadefs.h"
#include "pod_failure.h"
#include "prom_memory.h"
#include "prom_intr.h"
#include "prom_externs.h"
#include "ip21prom.h"


extern void	print_slot_bank(uint, uint);
extern void	gcache_parity(void);
extern void	xlate_ertoip(__uint64_t value);
extern int	disable_slot_bank(uint, uint, evbnkcfg_t *[], int, uint);


/*
 * Macros for referencing bank addresses
 */

#define SET_BANKREG(_slot, _b, _reg, _value) \
    EV_SET_CONFIG(_slot, \
                  MC3_BANK(((_b & 0x4) >> 2), (_b & 0x3), _reg), (_value))

#define NUM_LEAVES(_x) 	((_x) / MC3_BANKS_PER_LEAF)
#define BPOS(_l, _b) 	(((_l) << 2) + (_b))

#define SUCCEED	1
#define FAIL	0

#define READ		0	
#define WRITE		1

	
/* Array used to find the actual size of a particular
 * memory bank.  The size is in blocks of 256 bytes, since
 * the memory banks base address drops the least significant
 * 8 bits to fit the base addr in one word.
 */
unsigned long MemSizes[] = {
    0x10000, 	/* 0 = 16 megs   */
    0x40000,   	/* 1 = 64 megs   */
    0x80000,	/* 2 = 128 megs  */
    0x100000,	/* 3 = 256 megs  */
    0x100000,	/* 4 = 256 megs  */
    0x400000,	/* 5 = 1024 megs */
    0x400000,	/* 6 = 1024 megs */
    0x0		/* 7 = no SIMM   */
};


/* get_marev(int slot)
 *	Calculates the revision number of the MA chip in the specified slot.
 */
int get_marev(int slot)
{
	int revision;

	revision = (EV_GET_CONFIG(slot, MC3_REVLEVEL) & 0xf) << 4;
	/* revision[7:4] == MC3_REVLEVEL[3:0]		*/
	revision |= ((EV_GET_CONFIG(slot, MC3_BISTRESULT) >> 16) & 0xf);
	/* revision[3:0] == MC3_BISTRESULT[19:16]	*/

	return revision;

	/* MA rev "1" -> 0.  */
	/* MA rev "2" -> 16. */
	/* MA rev "3" -> 19. */
}


/*
 * build_interleave()
 *	Tries to configure the array of banks such that
 *	all of the interleaves are of the size given in
 *	cap.  This is done by treating the banks array as an
 *	array whose x axis corresponds to leaf numbers and whose
 *	y axis corresponds to banks.  
 *
 *		L0B0 L1B1 L0B1 L1B1 L0B2 L1B2
 *	       +----+----+----+----+----+----+
 *	Bank 0 |    |    |    |    |    |    |
 *	       |----+----+----+----+----+----|
 *	Bank 1 |    |    |    |    |    |    |
 *	       |----+----+----+----+----+----|
 *	Bank 2 |    |    |    |    |    |    |
 *	       |----+----+----+----+----+----|
 *	Bank 3 |    |    |    |    |    |    |
 *	       +----+----+----+----+----+----+
 *
 *	We iterate in row-major order, from left to right and
 *	from top to bottom, doing first all of banks 0, then all
 *	banks 1, etc.  We start by marking the first bank as
 *	used and then scan forward in the array, adding the other
 *	leaves as appropriate.  Each time we find a matching bank,
 *	we add it to the current interleave and continue scanning.
 *	If we can't build an interleave of the size requested by cap,
 *	we do one of two things.  If we're using a stable algorithm,
 *	we simply return, since the stable algorithm resets the used
 *	flags every time the algorithm starts.  If we're using the
 *	optimal algorithm, we deallocate only the current unsuccessful
 *	interleave, because any all of the other interleaves built in
 *	this pass are optimal.
 *	
 * Parameters:
 *	num_banks -- the number of banks in the bank array.
 *	bank -- the array of banks to be configured
 *	cap -- the interleave factor to be used as a goal.
 *	stable -- Flag indicating whether we should use a stable algorithm.
 * Returns:
 *	SUCCEED if all of the banks can be configured with an
 *	  interleave factor of either cap or 1,
 *	FAIL otherwise.
 */

#define MARK(_x) 	marked |= (1 << (_x)) 
#define NOT_MARKED(_x) !(marked & (1 << (_x)))

static int
build_interleave(int num_banks, evbnkcfg_t *bank[], int cap, int stable)
{
    int b, lf;		/* Current bank index */
    int cb, cl;		/* The bank and leaf being checked. */
    int ilv_size;	/* Size of the interleave built thus far */
    int	prev_bank;	/* Index of previously touched bank */
    evbnkcfg_t *bnk;	/* Pointer to first bank in current interleave */
    evbnkcfg_t *cbnk;	/* Pointer to bank currently being checked */
    int marked;		/* Bit set of used leaves */
    int startleaf;	/* Lower index for traversing checked leaves */
    int failed = 0;	/* Flag indicating that all banks aren't interleaved */
 
    DPRINTF("  Cap is %d, num_banks = %d\n", cap, num_banks);

    /* If we're doing a stable interleave erase all of our previous
     * configuration attempts.  If we're doing a optimal interleave,
     * we leave our previous attempts alone, since they are perfectly
     * valid.
     */
    if (stable)
	for (b = 0; b < num_banks; b++) 
	    bank[b]->bnk_used = bank[b]->bnk_count = 0;

    /* Now we actually iterate through all of the banks trying to interleave
     * them.  If we get through to the bottom of this loop, we know that
     * we interleaved all of the banks in the system.
     */ 
    for (b = 0; b < 4; b++) {
	for (lf = 0; lf < NUM_LEAVES(num_banks); lf++) {
	    bnk = bank[BPOS(lf, b)];

	    /* Skip empty, disabled, and already used banks */
	    if (bnk->bnk_used || bnk->bnk_size == MC3_NOBANK
		|| !bnk->bnk_enable) {
		DPRINTF("Skipping leaf %d bank %d\n", lf, b);
		DPRINTF("  Bank size: %d Bank used: %d\n", 
			 bnk->bnk_size, bnk->bnk_used);
		continue;
	    }

	    DPRINTF("   Trying to interleave leaf %d bank %d, size %d\n", 
		    lf, b, bnk->bnk_size);

	    /* Build the rest of the interleave chain by scanning
	     * through the rest of table table looking for banks
	     * of the required size.  
	     */
	    ilv_size = 1;
	    prev_bank = BPOS(lf, b);
	    bnk->bnk_used = 1;
	    marked = 0;
	    MARK(lf);

	    /* Scan the array looking for banks to add to the interleave.
	     * Since we should have already used any banks which preceed
 	     * the base bank, we don't rescan the entire array.
	     */
	    startleaf = lf+1;
	    for (cb = b; cb < 4 && ilv_size < cap; cb++) {
		for (cl = startleaf; 
		     cl < NUM_LEAVES(num_banks) && ilv_size < cap; 
		     cl++) {

		    cbnk = bank[BPOS(cl,cb)];

	            /* Add the leaf being checked to the current interleave 
		     * if the bank being checked hasn't been used, is of the 
		     * correct size, is enabled, and doesn't occupy the same 
		     * leaf as a bank which is already in the interleave.
	             */
		    if (!(cbnk->bnk_used) && 
		       (MemSizes[cbnk->bnk_size] == MemSizes[bnk->bnk_size]) && 
		       cbnk->bnk_enable && 
		       NOT_MARKED(cl)) {

			DPRINTF("    Adding leaf %d bank %d to interleave\n", 
				cl, cb);
			bank[prev_bank]->bnk_next = BPOS(cl, cb);
			cbnk->bnk_next = 0xff;
			cbnk->bnk_used = 1;
			ilv_size++;
			prev_bank = BPOS(cl, cb);
			MARK(cl);
		    } else {
			DPRINTF("    ...L%dB%d isn't going to work.\n", cl, cb);
		    }
		}    /* end for cl */

		startleaf = 0;
	    }    /* end for cb */

	    if (ilv_size != cap) {
	
		/* If we're doing an optimal interleave, we need to clean
	         * up this botched attempt so that the banks can be reused
		 * in a later try with a different cap.
		 */
	        if (!stable) {
		    DPRINTF("  Failed to interleave memory start at %d\n", b);
		    cbnk = bnk;
		    for (cb = 0; cb < ilv_size; cb++) {
			cbnk->bnk_count = 0;
			cbnk->bnk_used = 2;	/* Touched but not allocated */
			cbnk = bank[cbnk->bnk_next];	
		    }

		    failed = 1;
		} else {
		    return FAIL;
		}
	    } else {
		bnk->bnk_count = ilv_size;
	    }
	}    /* end for lf */
    }    /* end for b */

    if (failed) {
	DPRINTF("Couldn't build an interleave at some point\n");

	/* Clean up any unusable banks */
	for (cb = 0; cb < num_banks; cb++)
	    if (bank[cb]->bnk_used == 2)
		bank[cb]->bnk_used = 0;

	    return FAIL;
    } else {
	DPRINTF("  Successfully built interleave\n");
	return SUCCEED;
    }
}


/*
 * interleave()
 *	Configures the memory banks contained in the banks array
 *	using one of a couple different memory configuration algorithms.
 *	Currently, the following algorithms are supported:
 *		INTLV_ONEWAY -- Use 1-way interleaving for all banks.
 *		INTLV_STABLE -- Use an interleaving technique which provides
 *		   the most stable latency times.  This technique insures that
 *		   all memory will have roughly the same latencies, but it
 *		   sacrifices the spedups of potentially larger interleaves.
 *		INTLV_OPTIMAL -- Use a technique which will always construct
 *		   the largest interleaves possible.  This will produce the
 *		   fastest memory configuration, but the response time variance
 *		   will increase.
 * 
 * Parameters:
 *	num_banks -- the actual number of banks in the bank array
 *	banks -- the initializes banks array
 *	intlv_type -- Which of three interleave methods to use.
 * Returns:
 *	the most significant 32 bits of a 40 bit physical memory
 *	space value.
 */

static unsigned
interleave(int num_banks, evbnkcfg_t *banks[], int intlv_type)
{
    int cap = 16;	/* Start trying to build 16-way interleaves */
    int stable;		/* Flag indicating we should use stable interleaving */
    int b;		/* Bank loop index */
    int base; 		/* Base address for bank interleaving */
    int	ilf;		/* Interleave factor */
    int ilp;		/* Interleave position */
    int i,j;		/* Random index variable */
    int curr_bank;	/* Index of current bank in bank array */
    int prev_bank;	/* previous bank */
    int memdex;		/* Index into memory size table */
    int num_index;	/* Number of interleaves in index array */
    unchar index[64]; /* Array of indices to be sorted by size */
 
    /* Set the cap to 1 to force 1-way interleaving. */
    switch (intlv_type) {
      case INTLV_ONEWAY:
	cap = 1;
	stable = 1;
	break;

      case INTLV_STABLE:
	cap = 8;
	stable = 1;
	break;

      case INTLV_OPTIMAL:
	cap = 8;
	stable = 0;
	break;

      default:
	loprintf("interleave: Bad type argument\n");
	return 0;
    }
 
    /* Configure the memory.  We start by trying to interleave
     * the memory with the largest interleave value possible.
     * If this fails, we try the next smallest interleave factor,
     * and so on. Since a one-way interleave will always work,
     * this loop must eventually terminate.  If we are doing an
     * optimal interleave, we  
     */
    while (build_interleave(num_banks, banks, cap, stable) == FAIL) {
	cap >>= 1;

	/* This should never happen */
	if (cap == 0) return FAIL;
    }

    /* At this point, all of the memory in the banks array should
     * be fully configured using the largest stable interleaving
     * possible.  We now go through all of the banks and actually
     * program the configuration registers with the appropriate
     * values.  Because the base address of a memory bank must
     * be an integral multiple of the bank size, we start with
     * the largest bank sizes first and work our way down.
     */

    base = BASE_ADDR;

    /* Create an array of the indices of the first banks of all of the
     * interleaves.
     */
    num_index = 0;
    for (b = 0; b < num_banks; b++) 
	if (banks[b]->bnk_count) 
	   index[num_index++] = b;

    /* Sort the indices so that the interleaves are ordered by
     * decreasing interleave size (biggest interleave is first).
     */
    for (i = 0; i < num_index-1; i++) {
	unchar temp, max;
	unsigned size_max, size_j;
 
	max = i;
        size_max = MemSizes[banks[index[max]]->bnk_size] * 
			banks[index[max]]->bnk_count;	
	for (j = i+1; j < num_index; j++) {
	   size_j   = MemSizes[banks[index[j]]->bnk_size] * 
			banks[index[j]]->bnk_count;
	   if (size_j > size_max) {
		max = j;
		size_max = size_j;
	   }
	}

	temp = index[max]; index[max] = index[i]; index[i] = temp;
    }

    /* Finally, we iterate through the sorted array of interleaves
     * and actually configure them appropriately.
     */ 
    for (b = 0; b < num_index; b++) {
 
	/* Figure out interleave factor */
	ilf = 0;
	i = banks[index[b]]->bnk_count;

	while (i >>= 1) ilf++;

	/* Configure all the banks for this interleave */
	curr_bank = index[b];
	for (ilp = 0; ilp < banks[index[b]]->bnk_count; ilp++) {
	    DPRINTF("Config bank %d, base %d, ilp %d, ilf %d\n",
		     curr_bank, base, ilp, ilf);
	    SET_BANKREG(banks[curr_bank]->bnk_slot, curr_bank, BANK_BASE, 
			base);
	    SET_BANKREG(banks[curr_bank]->bnk_slot,curr_bank,BANK_IF, ilf);
	    SET_BANKREG(banks[curr_bank]->bnk_slot,curr_bank,BANK_IP, ilp);

	    /* Stuff calculated info into banks structure for the
	     * benefit of diags and other test routines.
	     */
	    banks[curr_bank]->bnk_ip = ilp;
	    banks[curr_bank]->bnk_bloc = base;
	    prev_bank = curr_bank;
	    curr_bank = banks[curr_bank]->bnk_next;
	    banks[prev_bank]->bnk_if = ilf; 
	}

	memdex = banks[index[b]]->bnk_size;
	base += (MemSizes[memdex] * banks[index[b]]->bnk_count);
    }

    return base;
}


/*
 * activate_memory()
 * 	Switches on all of the configured banks of memory. We
 *	do this here to ensure that everything is configured 
 *	before the memory starts responding to requests.
 * Parameters:
 *	bpos -- the number of banks in the banks array.
 *	bank -- the array containing the actual information about the 
 *	         banks configured in the system.
 * Returns:
 *	Nothing.
 */

void
activate_memory(int bpos, evbnkcfg_t *bank[])
{
    int b = 0;			/* Current bank index */
    int ldex, bdex;		/* Leaf and bank indexes */
    int bank_enable;		/* Bank enable information */
    int slot;			/* Slot of MC3 currently being enabled */

    while (b < bpos) {

	bank_enable = 0;
	slot = bank[b]->bnk_slot;
	for (ldex = 0; ldex < MC3_NUM_LEAVES; ldex++) {
	    for (bdex = 0; bdex < MC3_BANKS_PER_LEAF; bdex++, b++) {
		if (bank[b]->bnk_size != MC3_NOBANK &&
		    			bank[b]->bnk_enable)
		    bank_enable |= MC3_BENB(ldex, bdex);
	    }
	}		

	EV_SET_CONFIG(slot, MC3_BANKENB, bank_enable);
    }
}


/*
 * check_banks()
 *	Checks all of the configured banks in an attempt to determine
 *	whether the memory actually spans all the space it is supposed
 *	to.  To do this, it examines the base bloc, interleave factor and
 *	bank size and then strides through memory writing to major
 *	addresses.  If an exception occurs, it catches it and deals
 *	with it appropriately.
 * Parameters:
 *	bpos   -- Number of banks in array.
 *	bank   -- the array of pointer to bank info structures.
 *	action -- indicates whether we're writing or reading 
 * Returns:
 *	-1 if something fails horribly, 0 otherwise.
 */

int
check_banks(int num_banks, evbnkcfg_t *bank[], uint action)
{
    uint	b;		/* Bank index */
    uint 	i;		/* Interleave index */
    uint	j;		/* Index for toggling between beginning and
				   end of an interleave */
    uint	k;		/* Index for cruising through line */
    uint	count = 1;	/* Counter value */
    ulong	bsize;		/* Current banksize */
    ulong	bloc;		/* Current bloc value */
    jmp_buf	fault_buf;	/* Status buffer */
    uint	*prev_fault;	/* Previous fault handler address */
    uint	result;		/* Result from memory readback */
    uint	end_o_blk;	/* Last bloc of an interleave */
    int		ertoip;		/* contents of ertoip register on buserr */
    volatile uint	addr_bloc;	/* Bloc of address being tested */
    volatile uint	addr_off;	/* Offset of address being tested */
    volatile uint	addr_valid;	/* The addr_* numbers are valid */

    for (b = 0; b < num_banks; b++) {
	
	addr_valid = 0;	
	/* Skip uninteresting banks */
	if (bank[b]->bnk_count == 0 || bank[b]->bnk_size == MC3_NOBANK)
	    continue; 

	/*
	 * Now we calculate the number of banks in this interleave
	 * and the size of each bank.
	 */
	bsize = MemSizes[bank[b]->bnk_size];
	bloc = bank[b]->bnk_bloc;

	/* If an exception occurs, return to the following block */
	if (setfault(fault_buf, &prev_fault)) {
	    restorefault(prev_fault);
	    loprintf("\n*** Bus error occurred while checking mem board in slot %d\n",
							bank[b]->bnk_slot);
	    gcache_parity();
	    if (ertoip = load_double_lo((long long *) EV_ERTOIP))
		xlate_ertoip(ertoip);
	    loprintf("    EPC %x CAUSE %x BADVADDR %x\n", _epc(0, 0),
						_cause(0, 0), _badvaddr(0, 0));
	    if (addr_valid) {
		loprintf("    Error in ");
		print_slot_bank(addr_bloc, addr_off);
		if (disable_slot_bank(addr_bloc, addr_off, bank, num_banks,
							EVDIAG_MC3TESTBUSERR))
		    return EVDIAG_MC3DOUBLEDIS;
	    }
	    sc_disp(EVDIAG_MC3TESTBUSERR);
	    return EVDIAG_MC3TESTBUSERR;
	}

	/*
 	 * We now write/read a count value into the first word of the first
	 * line of each of the memory interleaves.  When we check throug
	 * this code by reading back we'll detect any overlapping interleaves.
	 */

	/* Calculate the last address of the last bloc of the interleave */ 
	end_o_blk = (bsize - 1) * (1 << bank[b]->bnk_if);

	for (i = 0; i < (1 << bank[b]->bnk_if); i++) {

	    for (j = 0; j <= end_o_blk; j += end_o_blk) {

    DPRINTF("b = %d, i = %d, j = 0x%x, count = %d, action = %d\n",
		b, i, j, count, action); 
		for (k = 0; k < 128; k += 4) {
		    /* Read or write as desired */
		    addr_bloc = bloc + j;
		    addr_off = (i << 7) + k;
		    addr_valid = 1;
		    /* avoid touching locore addresses cached since we touch them uncached */
		    if (action == WRITE) {
			if ((addr_bloc << 8) + addr_off >= 0x4000)
				u64sw(addr_bloc, addr_off, count);
		    } else {
			if ((addr_bloc << 8) + addr_off >= 0x4000)
				result = u64lw(addr_bloc, addr_off);
			else
				result = count;

			if (result != count) {
			    loprintf("\n*** MC3 readback error at address: 0x%x00 + 0x%x\n    ",
						addr_bloc, addr_off);
			    print_slot_bank(addr_bloc, addr_off);
			    if (disable_slot_bank(addr_bloc, addr_off, bank,
						num_banks,
						EVDIAG_BANK_FAILED))
				return EVDIAG_MC3DOUBLEDIS;
			    sc_disp(EVDIAG_BANK_FAILED); 
			    return EVDIAG_BANK_FAILED;
			}
			u64sw(addr_bloc, addr_off, 0);
		    }
		    addr_valid = 0;
		    count++;
		} 	/* end for k loop */
	    } 	  /* end for j loop */
	}    /* end for i loop */

	restorefault(prev_fault);
    }
    return 0;
}

/*
 * mc3_config()
 * 	Top-level memory configuration function.  Given a 
 * 	a base address (in terms of a page frame number), this 
 *	routine will configure all memory boards in the system.
 *
 * Parameters:
 *	cfginfo  -- Initialized Everest config info structure.
 *	no_intlv -- If set, an interleave factor of 1 (no interleaving)
 *		    will be used.  Useful for working around bugs.
 * Returns:
 *	Zero on success, EVDIAG_BIST_FAILED if the BIST fails on any bank,
 *			 EVDIAG_NO_MEM if we cannot configure any memory.
 */
uint
mc3_config(evcfginfo_t *cfginfo)
{
#if SABLE
    /* there's only 8mb of mem in sable. this code will not work. */
    cfginfo->ecfg_memsize = 8 * 1024 * 1024;
    return 0;
#else
    int		slot;		/* The slot index */
    int		leaf;		/* Leaf index */
    int		bank;		/* Bank index */
    int		bpos = 0;	/* position of currently initialized bank */
    int		type;		/* Board type check */
    evbnkcfg_t	*banks[EV_MAX_MC3S * MC3_NUM_BANKS];
    int		retval = 0;	/* Return value: nonzero == error */
    int		c;		/* Character read from bist request */
    int		marev;		/* Current board  MA chip rev level */
    int		min_marev = 999;/* Minimum MA chip rev level */
    uint	size = 0;	/* Total memsize (in blocs) */
 
    /*
     * Scan Everest bus looking for MC3 cards. 
     * Initialize them with base values when found and
     * add their banks to the appropriate lists.
     */

    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
	if (cfginfo->ecfg_board[slot].eb_type == EVTYPE_MC3) {

	    /* Make sure this really is a memory board */
	    type = EV_GET_CONFIG(slot, MC3_TYPE);
	    if (type != MC3_TYPE_VALUE)
		continue;

	    marev = get_marev(slot);

	    /* Set MC3 board rev to MA chip rev. */
	    cfginfo->ecfg_board[slot].eb_rev = marev;

	    if (marev < min_marev)
		min_marev = marev;
	    EV_SET_CONFIG(slot, MC3_ACCESS, 0x2 | (!getendian()) );
	    EV_SET_CONFIG(slot, MC3_MEMERRINT, MC3ERR_VECTOR);
	    EV_SET_CONFIG(slot, MC3_EBUSERRINT, MC3ERR_VECTOR);
	    EV_SET_CONFIG(slot, MC3_BANKENB, 0x0);
	    for (leaf = 0; leaf < MC3_NUM_LEAVES; leaf++) {
		for (bank = 0; bank < MC3_BANKS_PER_LEAF; bank++) {
		    DPRINTF("  Configuring leaf %d, bank %d...", leaf, bank);
		    
		    banks[bpos] = 
			&(cfginfo->ecfg_board[slot].eb_banks[4*leaf + bank]);
		    banks[bpos]->bnk_size = EV_GET_CONFIG(slot,
					      MC3_BANK(leaf, bank,BANK_SIZE));
		    banks[bpos]->bnk_slot = slot;
		    DPRINTF(" Addr: %x Size: %d bpos: %d\n", 
			banks[bpos], banks[bpos]->bnk_size, bpos);

		    if (banks[bpos]->bnk_enable == 0) 
			DPRINTF("  Disabling bank.\n");
		    else 
			size += MemSizes[banks[bpos]->bnk_size];
			
		    bpos++;
		}
	    } 		/* end for leaf */
	} 	/* end if */
    } 	/* end for */

    if (! bpos) {
	DPRINTF("\n*** CONFIGURATION FAILED: No memory banks were found.\n");
	return EVDIAG_NO_MEM;
    }

    /* If we find that we don't have enough memory to load the IO4 PROM
     
esent
     * but have been disabled.  The rationale here is that if a user has
     * mistakenly disabled too much memory the machine will still be able
     * to come up.
     */
    if (size < 0x20000) {
	int reenabled = 0;

	for (bank = 0; bank < bpos; bank++) {
	    if ((banks[bank]->bnk_size != MC3_NOBANK) && 
					       !(banks[bank]->bnk_enable)) {
		reenabled = 1;
		banks[bank]->bnk_enable = 1;
		banks[bank]->bnk_diagval = EVDIAG_MEMREENABLED;
	    }
	}

        if (reenabled)
	    loprintf("\n*** Not enough memory was enabled.  Enabling all banks.");
    }

    /* Determine which configuration algorithm to employ */
    if (nvram_okay() && (get_nvreg(NVOFF_FASTMEM) == '1')) {
	loprintf("\n    Using 'fastmem' interleave algorithm.\n");
	type = INTLV_OPTIMAL;
    } else {
	loprintf("\n    Using standard interleave algorithm.\n");
	type = INTLV_STABLE;
    }

    if (!(cfginfo->ecfg_debugsw & VDS_NOMEMCLEAR)) {
	/*
	* Run the BIST on demand. (only if rev. 0 MA chips are present
	*/
	if (min_marev < 16) {
	    loprintf("\n\nBIST? (y/n)  ");
	    pod_flush(); 
	    c = pod_getc();
	    pod_putc(c);
	} else {
	    c = 'y';	/* Always run BIST on machines without MA2s (rev 1). */
	}

	if (c == 'y') {
	    loprintf("Running built-in memory test...");
	    sysctlr_message("Running BIST..");
	    retval = pod_check_rawmem(bpos, banks) ? EVDIAG_BIST_FAILED : 0;
	} else {
	    loprintf("\nSkipping BIST\n");
	    retval = 0;
	}
    }

    sysctlr_message("Configuring memory.."); 
    while (cfginfo->ecfg_memsize = interleave(bpos, banks, type)) {
	int tmp_retval;
	sysctlr_message("Testing memory...");
	activate_memory(bpos, banks);
	if (!(cfginfo->ecfg_debugsw & (VDS_NO_DIAGS | VDS_NOMEMCLEAR))) {
		/*
		 * Run a simple check to insure that the basic bank
		 * arrangement is complete.
		 */
		if (tmp_retval = check_banks(bpos, banks, WRITE)) {
		    loprintf("*** Bank check write pass failed\n");
		    retval = tmp_retval;
		}
		
		if (tmp_retval = check_banks(bpos, banks, READ)) {
		    loprintf("*** Bank check read pass failed\n");
		    retval = tmp_retval;
		} else {
		    break;
		}

		/* If this happens, we may be infinite-looping */
		if (retval == EVDIAG_MC3DOUBLEDIS)
		    return retval;
	} else {
		break;
	} /* If diags are on */
	loprintf("NOTE: Reconfiguring memory.\n");
    } /* while */

    if (!cfginfo->ecfg_memsize) {
	loprintf("\n*** CONFIGURATION FAILED: No operational memory was found\n");
	return EVDIAG_NO_MEM;
    }

    if (!(cfginfo->ecfg_debugsw & (VDS_NO_DIAGS | VDS_NOMEMCLEAR))) {
	if (pod_check_mem(cfginfo->ecfg_memsize))
	    retval = EVDIAG_BAD_ADDRLINE;
    }
    return retval;
#endif
}

int
mc3_reconfig(evcfginfo_t *cfginfo, int intlv_type) {
    int		bpos = 0;	/* position of currently initialized bank */
    int		type;		/* Board type check */
    evbnkcfg_t	*banks[EV_MAX_MC3S * MC3_NUM_BANKS];
    int		slot;		/* The slot index */
    int		bank;		/* Bank index */
    int		leaf;		/* Leaf index */

    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
	/* Make sure this really is a memory board */
	type = cfginfo->ecfg_board[slot].eb_type & EVCLASS_MASK;
	if (type != EVCLASS_MEM)
	    continue;

	for (leaf = 0; leaf < MC3_NUM_LEAVES; leaf++) {
	    for (bank = 0; bank < MC3_BANKS_PER_LEAF; bank++) {
		DPRINTF("  Configuring leaf %d, bank %d...", leaf, bank);
		banks[bpos] = 
		    &(cfginfo->ecfg_board[slot].eb_banks[4*leaf + bank]);
		bpos++;
	    }
	}
    }

    if (cfginfo->ecfg_memsize = interleave(bpos, banks, intlv_type)) {
	activate_memory(bpos, banks);
    } else {
	loprintf("\n*** WARNING: No operational memory was found\n");
	return EVDIAG_NO_MEM;
    }

    return 0;
}


/*
 * mc3_clear_err
 *	Clears the MC3's error registers.  This isn't done 
 *	automatically by the config routines because the OS
 *	may want to examine these registers in certain circumstances.
 */

void
mc3_clear_err()
{
}
