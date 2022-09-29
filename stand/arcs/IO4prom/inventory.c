/***********************************************************************\
*	File:		inventory.c					*
*									*
*	This file contains the code for manipulating the hardware	*
*	inventory information.  It includes routines which read and	*
*	write the NVRAM inventory as well as code for checking the	*
*	validity of the current hardware state.				*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/nvram.h>

/*
 * Useful macros and definitions.
 */
#define INV_OFFBOARD(_b) \
    (NVOFF_INVENTORY + ((_b) * INV_SIZE))
#define INV_OFFUNIT(_b, _u) \
    (INV_OFFBOARD(_b) + INV_UNITOFF + ((_u) * INV_UNITSIZE))

#define INV_READ(_b, _f) \
    get_nvreg(INV_OFFBOARD(_b) + (_f))
#define INV_READU(_b, _u, _f) \
    get_nvreg(INV_OFFUNIT(_b, _u) + (_f))

#define INV_WRITE(_b, _f, _v) \
    set_nvreg(INV_OFFBOARD(_b) + (_f), (_v))
#define INV_WRITEU(_b, _u, _f, _v) \
    set_nvreg(INV_OFFUNIT(_b, _u) + (_f), (_v))

/*
 * External and Forward declarations
 */
char nvchecksum(void);
char *ioa_name(unchar,unchar);
char *board_name(unchar);
char *evdiag_msg(unchar);
void sysctlr_message(char *);

extern	void update_checksum(void);

/*
 * Forward declarations.
 */
static void 	check_ip(evbrdinfo_t*, int*, char *, int);
static void 	check_mc3(evbrdinfo_t*, int*);
static void 	check_io4(evbrdinfo_t*, int*);
static int  	parse_args(int, char**, int);
static void 	switch_ip(int, char*, int, char*, int);
static void 	switch_mc3(int, char*, int);
static void 	switch_io4(int, char*, int);
static char* 	bank_name(unchar);
static char  	bank_pos(int);

static char *bank_letter = "ACEGBDFH";

/*
 * write_inventory()
 * 	Writes the contents of the inventory fields in EVCFGINFO
 *	into the NVRAM.  If the reinitialize parameter is non-zero,
 *	this routine will actually try to reinitialize the NVRAM
 *	to rational default values (in particular, we only touch the
 *	enable fields if reinitialize is set).  Otherwise, it will 
 *	just transfer the fields into the NVRAM from the evconfig data 
 *	structure.  
 */

int
write_inventory(unsigned reinitialize) 
{
    uint slot, u;
    evbrdinfo_t *brd;
    int check;

    /* Invalidate the inventory just in case we crash while writing it */
    if (set_nvreg(NVOFF_INVENT_VALID, 0) == -1) {
	printf("Error in write_inventory: could not invalidate inventory\n");
	return -1;
    }

    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
	brd  = &(EVCFGINFO->ecfg_board[slot]);

        /* Unify the inventory and type values */
	brd->eb_inventory = brd->eb_type;

	check = 0;
	check += INV_WRITE(slot, INV_TYPE, brd->eb_type);
	check += INV_WRITE(slot, INV_REV, brd->eb_rev);
	check += INV_WRITE(slot, INV_DIAGVAL, 0);

	/* Only touch the ENABLE values if we're reinitializing. 
 	 * In all other cases the enable and disable commands will
	 * take care of it.
	 */
	if (reinitialize) {
	    check += INV_WRITE(slot, INV_ENABLE, 1);
	}	

	if (check != 0) {
	    printf("Error in write_inventory: could not update board %d\n",
		   slot);
	}

	/* Now do board-type specific writes */
	for (u = 0; u < INV_MAXUNITS; u++) {
	    unchar type = 0;

	    switch (brd->eb_type & EVCLASS_MASK) {
	      case EVCLASS_CPU:
		if (u < EV_MAX_CPUS_BOARD) {
		    evcpucfg_t *cpu = &brd->eb_cpuarr[u];
		    type = cpu->cpu_inventory = cpu->cpu_diagval;
		} else
		    type = 0;
		break;

	      case EVCLASS_MEM:
		type = brd->eb_banks[u].bnk_size;
		brd->eb_banks[u].bnk_inventory = brd->eb_banks[u].bnk_size;
		break;

	      case EVCLASS_IO:
		type = brd->eb_ioarr[u].ioa_type;
		brd->eb_ioarr[u].ioa_inventory = brd->eb_ioarr[u].ioa_type;
		break;
	    }

	    check = 0;	
	    check += INV_WRITEU(slot, u, INVU_TYPE, type);
	    check += INV_WRITEU(slot, u, INVU_DIAGVAL, 0);
	    if (reinitialize)
		check += INV_WRITEU(slot, u, INVU_ENABLE, 1);

	    if (check != 0) {
		printf("write_inventory error: update brd %d, unit %d failed\n",
		       slot, u);
		return -1;
	    }
	}    /* end for(unit) loop */ 
    } 	/* end for(slot) loop */

    /* Validate the Inventory data */
    if (set_nvreg(NVOFF_INVENT_VALID, INVENT_VALID) == -1) {
	printf("Error in write_inventory: could not revalidate inventory\n");
	return -1;
    }

    /* Rechecksum the NVRAM */
    update_checksum();

    return 0;
}


/*
 * check_inventory()
 *	Looks for eccentricities in the configuration information
 *	and prints appropriate warnings.  It uses the following
 *	rules: 
 *	  1). 	If there is a difference between the type and inventory
 *		fields, a warning is displayed.  In particular, if something
 *		appeared to have disappeared, the system screams.
 *
 *	  2).	If the enable field is 0, the types match, and diagval is
 *		non-zero, the board failed some kind of diagnostic and a 
 *		warning is printed.
 *	
 *	  3).	If the enable field is zero, the types match, and diagval is
 *		non-zero, the board was disabled in software for some reason.
 *	
 *	  4). 	If diagval is non-zero, but the enable bit is set, a non-fatal
 *		problem was detected.
 */

void
check_inventory()
{
    uint slot;
    evbrdinfo_t *brd;
    int changed = 0;

    p_clear();
    p_printf("Checking hardware inventory...\n");
    sysctlr_message("Checking inventory...");

    /* Don't bother running this code if it doesn't look like the
     * inventory is valid.  Instead, reinitialize the inventory. 
     */
    if (get_nvreg(NVOFF_INVENT_VALID) != INVENT_VALID) {
	printf("WARNING: hardware inventory is invalid.  Reinitializing...\n");
	write_inventory(1);
    }

    /* Now iterate through all the slots looking for problems. */
    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
	
	brd = &(EVCFGINFO->ecfg_board[slot]);

	/* If there is a difference between the type and inventory value,
	 * do appropriate checking.
	 */
	if (brd->eb_type == EVTYPE_EMPTY) {
	    
	    if (brd->eb_inventory != EVTYPE_EMPTY) {
		printf("*** slot %d previously contained an %s board\n", 
		       slot, board_name(brd->eb_inventory));
		printf("    but now appears to be empty.\n");
		changed = 1;
	    }
	} else {
	    /* If a slot isn't empty, we check to see if it contains
	     * the same board as it used to.
	     */
	    if (brd->eb_type != brd->eb_inventory) {
		printf("*** slot %d previously contained an %s board\n",
			slot, board_name(brd->eb_inventory));
		printf("    but now contains an %s board.\n", 
	 	       board_name(brd->eb_type));
		changed = 1;
	    }

	    /* Check for problems with diagnostics.
	     */
	    if (brd->eb_diagval != EVDIAG_PASSED) {
		printf("*** The %s board in slot %d failed diagnostics.\n", 
		       board_name(brd->eb_type), brd->eb_slot);
		printf("***   Reason: %s\n", evdiag_msg(brd->eb_diagval));
		changed = 1;
		continue;
	    }

	    /* Check to see if the board is still enabled.  If it is,
	     * we also check the units for information.
	     */
	    if (!brd->eb_enabled) {
		printf("*** The %s board in slot %d is DISABLED\n", 
		       board_name(brd->eb_type), brd->eb_slot);
		/* 
		 * changed = 1;
		 * Donot prompt for <CR> if a board was explicitly disabled
		 */
	    } else {
		if (brd->eb_type == brd->eb_inventory) {
		    switch (brd->eb_type) {
		      case EVTYPE_IP19:
			check_ip(brd, &changed, "IP19", 4);
			break;
		      case EVTYPE_IP21:
			check_ip(brd, &changed, "IP21", 2);
			break;
		      case EVTYPE_IP25:
			check_ip(brd, &changed, "IP25", 2);
			break;
		      case EVTYPE_MC3:
			check_mc3(brd, &changed);
			break;
		      case EVTYPE_IO4:
			check_io4(brd, &changed);
			break;
		      default:
			break;
		    }
	  	}
	    }
	}    		/* end if/else */
    }    	/* end for(slot) loop */

    /* Wait for a carriage return */
    if (changed) {
	char *nonstop = getenv("nonstop");
	if (nonstop && *nonstop != '1') {
	    char buff[128];
	    p_cursoff();
	    p_printf("\nPress <Enter> to continue");
	    gets(buff);
	}
    }
}


static void
check_ip(evbrdinfo_t *brd, int *changed, char *name, int maxcpus)
{
    int unit;
    evcpucfg_t *cpu;

    for (unit = 0; unit < maxcpus; unit++) {
	cpu = &brd->eb_cpuarr[unit];

	/* Skip empty slices */
	if (cpu->cpu_diagval == EVDIAG_NOTFOUND) {
	    if (cpu->cpu_inventory  != EVDIAG_NOTFOUND) {
		printf("***\tSlice %d on the %s in slot %d isn't visible\n",
		       unit, name, brd->eb_slot);
		*changed = 1;
	    }
	    continue;
	} else if (((EVCFGINFO->ecfg_snum[0] == 'S') && 
		    (EVCFGINFO->ecfg_snum[1] == '9')) &&
		   (cpu->cpu_vpid != cpuid())) {
	    /*
	     * Do not allow more that 1 CPU on a memphis system.
	     */
	    sysctlr_message("*** Invalid CPU conf\n");
	    printf("***\t Invalid CPU Configuration\n");
	    for ( ; ; ) 
		;
	}

	if (cpu->cpu_diagval != EVDIAG_PASSED) {
	    printf("***\tSlice %d on the %s in slot %d failed diagnostics\n",
		   unit, name, brd->eb_slot);
	    printf("***\t  Reason: %s\n", evdiag_msg(cpu->cpu_diagval));
	    *changed = 1;
	}

	if (!cpu->cpu_enable) 
	    printf("***\tSlice %d on the %s in slot %d is DISABLED\n",
		   unit, name, brd->eb_slot); 
    }
}


static void
check_mc3(evbrdinfo_t *brd, int *changed)
{
    int unit;
    evbnkcfg_t *bnk;

    for (unit = 0; unit < 8; unit++) {
	bnk = &brd->eb_banks[unit];

	/* Skip empty banks */
	if (bnk->bnk_size == MC3_NOBANK && bnk->bnk_inventory == MC3_NOBANK)
	    continue;

	if (bnk->bnk_size != bnk->bnk_inventory) {
	    printf("***\tBank %c on the MC3 in slot %d has changed.\n",
		   bank_pos(unit), brd->eb_slot);
	    printf("***\t  Bank used to be %s and now is %s\n",
		   bank_name(bnk->bnk_inventory), bank_name(bnk->bnk_size));
	    *changed = 1;
	}

	if (bnk->bnk_diagval != EVDIAG_PASSED) {
	    printf("***\tBank %c on the MC3 in slot %d failed diagnostics.\n",
		   bank_pos(unit), brd->eb_slot);
	    printf("***\t  Reason: %s\n", evdiag_msg(bnk->bnk_diagval));
	    *changed = 1;
	}

	if (!bnk->bnk_enable) 
	   printf("***\tBank %c on the MC3 in slot %d is DISABLED.\n",
		  bank_pos(unit), brd->eb_slot); 
    }
}


static void
check_io4(evbrdinfo_t *brd, int *changed)
{
    int unit;
    evioacfg_t *ioa;

    for (unit = 1; unit < 8; unit++) {
	ioa = &brd->eb_ioarr[unit];

	/* Skip empty IOAS */
	if (ioa->ioa_type == IO4_ADAP_NULL && 
	    ioa->ioa_inventory == IO4_ADAP_NULL)
	    continue;

	if (ioa->ioa_type != ioa->ioa_inventory) {
	    printf("***\t IOA %d on the IO4 in slot %d has changed\n",
		   unit, brd->eb_slot);
	    printf("***\t   IOA used to be %s and now is %s\n", 
		   ioa_name(ioa->ioa_inventory, 0), 
		   ioa_name(ioa->ioa_type, ioa->ioa_subtype));
	    *changed = 1;
	}

	if (ioa->ioa_diagval != EVDIAG_PASSED) {
	    printf("***\tThe %s at ioa %d on the IO4 in slot %d failed diags.\n",
		   ioa_name(ioa->ioa_type, ioa->ioa_subtype), unit, 
	           brd->eb_slot);
	    printf("***\t  Reason: %s\n", evdiag_msg(ioa->ioa_diagval));
	    *changed = 1;
	}

	if (!ioa->ioa_enable)
	    printf("***\tThe %s at ioa %d on the IO4 in slot %d is DISABLED\n",
		   ioa_name(ioa->ioa_type, ioa->ioa_subtype), unit, 
		   brd->eb_slot);
    }
}


/*
 * bank_name()
 *	Returns a string corresponding to the bank name.
 */
static char*
bank_name(unchar type)
{
    switch(type) {
        case 0:			return "16 Megabytes";
        case 1:			return "64 Megabytes";
        case 2:			return "128 Megabytes";
        case 3:			return "256 Megabytes";
        case 4:			return "256 Megabytes";
        case 5:			return "1 Gigabyte";
        case 6:			return "1 Gigabyte";
        case 7:			return "empty";
        default:		return "Unknown";
    }
}


/*
 * bank_pos()
 *	Returns the letter corresponding to a bank's unit number.
 */
char
bank_pos(int unit)
{

    if (unit < 0 || unit > 7)
	return '?';
    else
	return bank_letter[unit];
}


/*
 * enable_cmd()
 *	Syntax: enable [-R] [-y] [slot] [unit]
 *	Enables a board or unit in a specified slot.  The -R switch
 *	causes the command to enable recursively; enable -R 1 will
 * 	try to enable all the units of the board in slot 1.  The -y
 *	says to answer all interactive queries with a yes. 
 */

#define ENABLE  1
#define DISABLE 0

int
enable_cmd(int argc, char **argv)
{
    /* Just return if the command completed successfully */
    if (parse_args(argc, argv, 1) == 0) {
	update_checksum();
	return 0;
    }

    /* If the command failed, dump out the syntax information */
    printf("Enable syntax: enable SLOT [UNIT]\n");
    printf("   The enable command allows one to re-enable a slot or unit\n");
    printf("   on a slot which has previously been disabled.  Re-enabling\n");
    printf("   does not take effect immediately; the machine must be\n");
    printf("   rebooted.\n");
    return 0;
}


/*
 * disable_cmd()
 *	Syntax: disable [-R] [-y] [slot] [unit]
 *	Disables a board or unit in a specified slot.  The -R switch 
 *	causes the command to disable recursively; enable -R 1 will
 *	try to disable all the units of the board in slot 1.  The -y
 * 	says to answer all interactively queries with a yes.
 */

int
disable_cmd(int argc, char **argv)
{
    /* Just return if the command completed successfully */
    if (parse_args(argc, argv, 0) == 0) {
	update_checksum();
        return 0;
    }

    /* If the command failed, dump out the syntax information */
    printf("Disable syntax: disable SLOT [UNIT]\n");
    printf("   The disable command allows one to disable a board or unit\n");
    printf("   on a board.  Disabling a board or board unit does not take\n");
    printf("   effect until the machine is rebooted.\n");
    return 0;
}

 
static int 
parse_args(int argc, char **argv, int swtch)
{
    char *slotstr;			/* Pointer to slot string */
    char *unitstr;			/* Pointer to unit string - if exists*/
    int slot  = -1;			/* Actual slot number */
    evbrdinfo_t *brd;			/* Pointer to board to be disabled */

    switch (argc) {
    case 1:
	printf("Error: A slot number must be given as a parameter.\n");
	return -1;

    case 2:
	slotstr = argv[1];
	unitstr = NULL;
	break;

    case 3:
	slotstr = argv[1];
	unitstr = argv[2];
	break;

    default:
	printf("Error: Too many arguments.\n");
	return -1;
    } 
    /* Scan out slot number */
    if (*atob(slotstr, &slot)) {
	printf("Error: '%s' is not a valid slot number\n", slotstr);
	return -1;
    }

    if (slot < 0 || slot > EV_MAX_SLOTS) {
	printf("Error: slot value must be between 1 and %d\n", slot, 
	       EV_MAX_SLOTS);
	return 0;
    }

    /* Skip empty boards in the system */
    brd = BOARD(slot);
    if (brd->eb_type == EVTYPE_EMPTY) {
	printf("Slot %d is empty.  Cannot %s it.\n",
	       slot, ((swtch == ENABLE) ? "enable" : "disable"));
	return 0;
    }

    switch (brd->eb_type) {
    case EVTYPE_IP19:
	switch_ip(slot, unitstr, swtch, "IP19", 4);
	break;

    case EVTYPE_IP21:
	switch_ip(slot, unitstr, swtch, "IP21", 2);
	break;

    case EVTYPE_IP25:
	switch_ip(slot, unitstr, swtch, "IP25", 4);
	break;

    case EVTYPE_MC3:
	switch_mc3(slot, unitstr, swtch); 
	break;

    case EVTYPE_IO4:
	switch_io4(slot, unitstr, swtch); 
	break;

    default:
	printf("WARNING: Unknown board type in slot %d\n", slot);
	break;
    }

    return 0;
}

static void
switch_ip(int slot, char *unitstr, int swtch, char *brdname, int maxcpus)
{
    evbrdinfo_t *brd = BOARD(slot);
    evcpucfg_t *cpu;
    int unit, startunit, endunit;

    /* Convert the unit string into a slice number */
    if (unitstr != NULL) {
	if (*atob(unitstr, &unit)) {
	    printf("Error: slice should be an integral value, 0<=slice<=%d\n",
			maxcpus);
	    return;
	}
    } else {
	unit = -1;
    }
 
    if (unit == -1) {
	if (swtch == ENABLE && brd->eb_diagval != EVDIAG_PASSED) {
	    printf("*** Warning: The %s board in slot %d failed diagnostics\n",
		   brdname, slot);
	    printf("***   Reason: %s\n", evdiag_msg(brd->eb_diagval));
	}

	INV_WRITE(slot, INV_ENABLE, swtch);
	brd->eb_enabled = swtch;
	startunit = 0; 
	endunit = maxcpus;

    } else if (unit >= maxcpus) {
	printf("Error: %s boards have only %d processor slices.\n",
			brdname, maxcpus);
	return;
    } else {
	startunit = unit;
	endunit   = unit+1;
	/* If enabling a part of the board, enable the board too */
	if ((swtch == ENABLE) && (INV_READ(slot, INV_ENABLE) != ENABLE)){
	    INV_WRITE(slot, INV_ENABLE, swtch);
	    brd->eb_enabled = swtch;
	}
    }  

    for (unit = startunit; unit < endunit; unit++) { 
	cpu = &(BOARD(slot)->eb_cpuarr[unit]);

	/* Check to see whether the given slice actually exists */

	if (cpu->cpu_diagval == EVDIAG_NOTFOUND)
	    printf("WARNING: Slice %d of the %s in slot %d isn't present\n",
		   unit, brdname, slot);
	
	if (swtch == DISABLE) {
	    unsigned spnum = 
		load_double_lo((long long *)EV_SPNUM) & EV_SPNUM_MASK;

	    if (spnum != MPCONF[cpu->cpu_vpid].phys_id && 
		cpu->cpu_diagval != EVDIAG_NOTFOUND &&
		cpu->cpu_enable) {
		launch_slave(cpu->cpu_vpid, (void (*)(int))EV_PROM_FLASHLEDS, 
			     0, 0, 0, 0);
	    }

	    cpu->cpu_enable = 0;
	} else {
	    
	    if (cpu->cpu_diagval != EVDIAG_NOTFOUND && 
		cpu->cpu_diagval != EVDIAG_PASSED) {
		printf("Slice %d on %s slot %d failed diagnostics\n",
			unit, brdname, slot);
	    } else {
		cpu->cpu_enable = 1;
	    }
	}

	INV_WRITEU(slot, unit, INVU_ENABLE, swtch);

	printf("%s slice %d on the %s board in slot %d\n", 
	   ((swtch == ENABLE) ? "Enabled" : "Disabled"), unit, brdname, slot); 
    }

    printf("Changes will take effect when the machine is power-cycled.\n");
}


static void
switch_mc3(int slot, char *unitstr, int swtch)
{
    evbrdinfo_t *brd = BOARD(slot);
    evbnkcfg_t *bnk;
    int startunit, endunit;
    int unit;

    /* Convert the bank letter into an actual bank number */
    if (unitstr != NULL) {
	char uc = *unitstr & 0x5f;
	for (unit = 0; unit < 8; unit++)
	    if (uc == bank_letter[unit])
		break;
	if (unit >= 8) {
	    printf("Error: Banks are specified as a letter (A through H)\n");
	    return;
	}
    } else unit = -1;
	
    if (unit == -1) {
	if (swtch == ENABLE && brd->eb_diagval != EVDIAG_PASSED)
	    printf("Warning: The MC3 board in slot %d failed diagnostics\n",
		   slot);

	INV_WRITE(slot, INV_ENABLE, swtch);
	brd->eb_enabled = swtch;

	startunit = 0;
	endunit = MC3_NUM_BANKS;
    } else if (unit > 7) {
	printf("MC3 boards only have 8 banks.\n");
	return;
    } else {
	startunit = unit;
	endunit   = unit + 1;
	/* If enabling a part of the board, enable the board too */
	if ((swtch == ENABLE) && (INV_READ(slot, INV_ENABLE) != ENABLE)){
	    INV_WRITE(slot, INV_ENABLE, swtch);
	    brd->eb_enabled = swtch;
	}
    }

    for (unit = startunit; unit < endunit; unit++) {
	bnk = &(BOARD(slot)->eb_banks[unit]);
	if (bnk->bnk_size == MC3_NOBANK) 
	    printf("NOTE: Bank %c on the MC3 board in slot %d is empty.\n",
		    bank_pos(unit), slot);

	if (swtch == DISABLE) {
		bnk->bnk_enable = 0;
	} else {

	    if (bnk->bnk_size != MC3_NOBANK && 
                bnk->bnk_diagval != EVDIAG_PASSED) {
		printf("Bank %c on the MC3 board in slot %d failed diags.\n", 
		       bank_pos(unit), slot); 
	    } else { 
		bnk->bnk_enable = 1;
	    }
	}
      
	INV_WRITEU(slot, unit, INVU_ENABLE, swtch);

	printf("%s bank %c of the MC3 board in slot %d\n",
	       ((swtch == ENABLE) ? "Enabled" : "Disabled"), bank_pos(unit), 
	       slot);
    }
    printf("Changes will take effect when the machine is reset.\n");
}


static void
switch_io4(int slot, char *unitstr, int swtch)
{
    evbrdinfo_t *brd = BOARD(slot);
    evioacfg_t *ioa;
    int startunit, endunit;
    int unit;

    /* Convert the unit string into an IOA */
    if (unitstr != NULL) {
	if (*atob(unitstr, &unit)) {
	    printf("The IOA must be a number between 1 and 7\n");
	    return;
	}
    } else
	unit = -1;

    if (unit == -1) {
	if (swtch == ENABLE && brd->eb_diagval != EVDIAG_PASSED)
	    printf("Warning: The IO4 board in slot %d failed diagnostics\n",
		   slot);

	INV_WRITE(slot, INV_ENABLE, swtch);
 	brd->eb_enabled = swtch;

	startunit = 1;
	endunit = 7;
    } else if (unit == 0 || unit > 7) {
	printf("Error: Illegal IOA number: %d\n", unit);
	return;
    } else {
	if (brd->eb_ioarr[unit].ioa_type == IO4_ADAP_NULL) 
	    printf("WARNING: IBUS slot %d on the IO4 in slot %d is empty.\n",
		   unit, slot);

	startunit = unit;
	endunit = unit + 1;
	/* If enabling a part of the board, enable the board too */
	if ((swtch == ENABLE) && (INV_READ(slot, INV_ENABLE) != ENABLE)){
	    INV_WRITE(slot, INV_ENABLE, swtch);
	    brd->eb_enabled = swtch;
	}
    }

    for (unit = startunit; unit < endunit; unit++) {
	ioa = &(BOARD(slot)->eb_ioarr[unit]);

	if (swtch == DISABLE) {
	    ioa->ioa_enable = 0;
	} else {
	    if (ioa->ioa_type != IO4_ADAP_NULL && 
		ioa->ioa_diagval != EVDIAG_PASSED) {
		printf("The %s in IOA %d on the IO4 in slot %d failed diags.\n",
		   ioa_name(ioa->ioa_type, ioa->ioa_subtype), unit, slot); 
	    } else {
		ioa->ioa_enable = swtch;
	    }
	}

	INV_WRITEU(slot, unit, INVU_ENABLE, swtch);

	printf("%s IO adapter %d (%s) on the IO4 in slot %d\n",
	       ((swtch == ENABLE) ? "Enabled" : "Disabled"), 
	       unit, ioa_name(ioa->ioa_type, ioa->ioa_subtype), slot);
    }
    printf("Changes will take effect when the machine is reset.\n");
}
		
	
/*
 * The update command rewrites the current state of the machine
 * into the inventory nvram.  It ignores the enable/disable hardware
 * state, since in many cases the diagnostics may have only temporarily
 * set the disable bits.  
 */

/*ARGSUSED*/
int
update_cmd(int argc, char *argv)
{
    (void)write_inventory(1);

    return 0;
}


/*
 * get_cpuinfo()
 *	Returns a pointer to the cpu cfg structure given
 *	the physical ID of the cpu.
 */

static evcpucfg_t* 
get_cpuinfo(int phys_id)
{
    unsigned slot = (phys_id & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
    unsigned proc = (phys_id & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;
    evbrdinfo_t *brd;

    if (slot >= EV_MAX_SLOTS) {
	printf("get_cpuinfo: Bogus slotnum %d\n", slot);
	return NULL;
    }

    if (proc > EV_MAX_CPUS_BOARD) {
	printf("get_cpuinfo: Bogus procnum %d\n", proc);
	return NULL;
    }

    brd = &(EVCFGINFO->ecfg_board[slot]);
    if (EVCLASS(brd->eb_type) != EVCLASS_CPU) {
	printf("get_cpuinfo: slot %d doesn't contain a cpu board\n", slot);
	return NULL;
    }

    return &(brd->eb_cpuarr[proc]);
}	

     
/*
 * fix_vpids()
 * 	The kernel assumes (incorrectly) that the processor whose
 * 	vpid is zero is both the master processor and the processor
 * 	which is currently active.  This is completely bogus, but
 * 	we deal with it by reordering the array until an active
 * 	processor is in slot zero.  Note that this routine _must_
 * 	be called early in the initialization cycle, since lots of
 * 	other data structures (particularly the config tree) are
 * 	built using the information from here.
 */

void
fix_vpids(void)
{
    evcpucfg_t *cpu;
    int pid;

    if ((cpu = get_cpuinfo(MPCONF[0].phys_id)) == NULL)
	return;
	
    while (! cpu->cpu_enable) {
	mpconf_t temp_mpconf = MPCONF[0];

	/* Shift everything down a slot */
	pid = 1;
	while (MPCONF[pid].mpconf_magic == MPCONF_MAGIC &&
	       MPCONF[pid].virt_id == pid) {
	    evcpucfg_t *tmp_cpu = get_cpuinfo(MPCONF[pid].phys_id);
	    tmp_cpu->cpu_vpid--;
	    MPCONF[pid].virt_id--;
	    MPCONF[pid-1] = MPCONF[pid];
	    pid++;
	}

	/* Put the disabled processor at the end of the array */
	pid--;
	MPCONF[pid] = temp_mpconf;
	MPCONF[pid].virt_id = pid;
	cpu = get_cpuinfo(MPCONF[pid].phys_id);
	cpu->cpu_vpid = pid;	

	/* Now check the new cpu at index 0 to ensure it is enabled */
	if ((cpu = get_cpuinfo(MPCONF[0].phys_id)) == NULL)
	    return;
    }
}


void
set_diagval_disable(int vpid, int diagval)
{
    int spnum, slot, proc;

    spnum = MPCONF[vpid].phys_id;
    slot = (spnum & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
    proc = (spnum & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

    EVCFGINFO->ecfg_board[slot].eb_cpuarr[proc].cpu_diagval = diagval;
    EVCFGINFO->ecfg_board[slot].eb_cpuarr[proc].cpu_enable = 0;
}


int dump_earoms(void)
{
#if IP19
    int i, cpus;

    cpus = 0;
    i = EV_MAX_CPUS;
  
    while (i && !cpus) {
	if (MPCONF[i].phys_id != NULL)
	    cpus = i;
	i--;
    }
 
    printf(" EAROM checksums:");
    for (i = 0; i <= cpus; i++) {
	if (!(i % 16))
		printf("\n  ");
	if (MPCONF[i].phys_id != NULL)
		printf("%04x ", MPCONF[i].earom_cksum);
	else
		printf("     ");
    }
    printf("\n");
#endif
    return 0;
}


void
compare_checksums()
{
#if IP19
    int 	i, cpus;
    int 	dump_checksums = 0;

    printf("Comparing EAROM checksums...\n");

    cpus = 0;
    i = EV_MAX_CPUS;
  
    while (i && !cpus) {
	if (MPCONF[i].phys_id != NULL)
	    cpus = i;
	i--;
    }
 
    for (i = 0; i <= cpus; i++) {
	if (MPCONF[i].virt_id == i) {
	    if (MPCONF[i].stored_cksum) {
		if (MPCONF[i].stored_cksum != MPCONF[i].earom_cksum) {
		    printf("Disabling CPU %d due to bad EAROM checksum.\n", i);
		    set_diagval_disable(i, EVDIAG_EAROM_CKSUM);
		}
	    } else {
		    dump_checksums = 1;
	    }
	}
    }

    if (dump_checksums)
	dump_earoms();
#endif
}
