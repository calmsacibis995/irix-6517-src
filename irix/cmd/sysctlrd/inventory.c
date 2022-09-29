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
#include <sys/cpu.h>
#include <syslog.h>
#include <stdio.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/everest.h>

/*
 * Forward declarations.
 */
static void 	check_ip(evbrdinfo_t*, int*);
static void 	check_mc3(evbrdinfo_t*, int*);
static void 	check_io4(evbrdinfo_t*, int*);
static char* 	bank_name(unchar);
static char  	bank_pos(int);
char*		board_name(unchar);
char*		ioa_name(unchar, unchar);
char*		evdiag_msg(unchar);

static char *bank_letter = "ACEGBDFH";

/*
 * check_inventory(evcfginfo_t *evconfig)
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
 *
 */

void
check_inventory(evcfginfo_t *evconfig)
{
    uint slot;
    evbrdinfo_t *brd;
    int changed = 0;

    /* Now iterate through all the slots looking for problems. */
    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
	
	brd = &(evconfig->ecfg_board[slot]);

	/* If there is a difference between the type and inventory value,
	 * do appropriate checking.
	 */
	if (brd->eb_type == EVTYPE_EMPTY) {
	    
	    if (brd->eb_inventory != EVTYPE_EMPTY) {
		syslog(LOG_NOTICE,
		"*** Slot %d previously contained an %s board but is empty", 
		       slot, board_name(brd->eb_inventory));
		changed = 1;
	    }
	} else {
	    /* If a slot isn't empty, we check to see if it contains
	     * the same board as it used to.
	     */
	    if (brd->eb_type != brd->eb_inventory) {
		syslog(LOG_NOTICE,
	"*** Slot %d previously contained an %s board but now an %s board.\n", 
			slot, board_name(brd->eb_inventory),
			       board_name(brd->eb_type));
		changed = 1;
	    }

	    /* Check for problems with diagnostics.
	     */
	    if (brd->eb_diagval != EVDIAG_PASSED) {
		syslog(LOG_ERR, "*** The %s board in slot %d failed diagnostics.\n", 
		       board_name(brd->eb_type), brd->eb_slot);
		syslog(LOG_ERR, "***   Reason: %s\n", evdiag_msg(brd->eb_diagval));
		changed = 1;
		continue;
	    }

	    /* Check to see if the board is still enabled.  If it is,
	     * we also check the units for information.
	     */
	    if (!brd->eb_enabled) {
		syslog(LOG_NOTICE, "*** The %s board in slot %d is DISABLED\n", 
		       board_name(brd->eb_type), brd->eb_slot);
	 	changed = 1;	
	    } else {
		if (brd->eb_type == brd->eb_inventory) {
		    switch (EVCLASS(brd->eb_type)) {
		      case EVCLASS_CPU:
			check_ip(brd, &changed);
			break;
		      case EVCLASS_MEM:
			check_mc3(brd, &changed);
			break;
		      case EVCLASS_IO:
			check_io4(brd, &changed);
			break;
		      default:
			break;
		    }
	  	}
	    }
	}    		/* end if/else */
    }    	/* end for(slot) loop */

}

static void
check_ip(evbrdinfo_t *brd, int *changed)
{
    int unit;
    evcpucfg_t *cpu;

    for (unit = 0; unit < 4; unit++) {
	cpu = &brd->eb_cpuarr[unit];

	/* Skip empty slices */
	if (cpu->cpu_diagval == EVDIAG_NOTFOUND) {
	    if (cpu->cpu_inventory  != EVDIAG_NOTFOUND) {
		syslog(LOG_NOTICE,
			"*** Slice %d on the %s in slot %d isn't visible\n",
			unit, board_name(brd->eb_type), brd->eb_slot);
		*changed = 1;
	    }
	    continue;
	}

	if (cpu->cpu_diagval != EVDIAG_PASSED) {
	    syslog(LOG_ERR, "*** Slice %d on the %s in slot %d failed diagnostics\n",
		   unit, board_name(brd->eb_type), brd->eb_slot);
	    syslog(LOG_ERR, "***   Reason: %s\n", evdiag_msg(cpu->cpu_diagval));
	    *changed = 1;
	}

	/*
	 * The IP21 prom has a bug that causes it to mark the CPU slices that
	 * don't exit on all IP21 boards (slices 2 & 3) as populated.
	 * This hack keeps us from printing bogus DISABLED messages.
	 */
	if ((brd->eb_type == EVTYPE_IP21) && (unit >= 2))
	    continue;

	if (!cpu->cpu_enable) 
	    syslog(LOG_NOTICE, "*** Slice %d on the %s in slot %d is DISABLED\n",
		   unit, board_name(brd->eb_type), brd->eb_slot); 
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
	    syslog(LOG_NOTICE, "*** Bank %c on the MC3 in slot %d has changed.\n",
		   bank_pos(unit), brd->eb_slot);
	    syslog(LOG_NOTICE, "    Bank used to be %s and now is %s\n",
		   bank_name(bnk->bnk_inventory), bank_name(bnk->bnk_size));
	    *changed = 1;
	}

	if (bnk->bnk_diagval != EVDIAG_PASSED) {
	    syslog(LOG_ERR, "*** Bank %c on the MC3 in slot %d failed diagnostics.\n",
		   bank_pos(unit), brd->eb_slot);
	    syslog(LOG_ERR, "***   Reason: %s\n", evdiag_msg(bnk->bnk_diagval));
	    *changed = 1;
	}

	if (!bnk->bnk_enable) 
	   syslog(LOG_ERR, "*** Bank %c on the MC3 in slot %d is DISABLED.\n",
		  bank_pos(unit), brd->eb_slot); 
    }
}


static int
ioa_changed(int old_ioa, int new_ioa)
{
	if (old_ioa == new_ioa)
		return 0;

	/* Unix changes the F Chip IOA into the sub ioa.  To this code,
	 *	all F chips must be equal.
	 */
	if ((old_ioa == IO4_ADAP_FCHIP) &&
	    ((new_ioa == IO4_ADAP_FCG) || ((new_ioa == IO4_ADAP_VMECC))))
		return 0;
	else
		return 1;
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

	if (ioa_changed(ioa->ioa_inventory, ioa->ioa_type)) {
	    syslog(LOG_NOTICE, "*** IOA %d on the IO4 in slot %d has changed\n",
		   unit, brd->eb_slot);
	    syslog(LOG_NOTICE, "*** IOA used to be %s and now is %s\n", 
		   ioa_name(ioa->ioa_inventory, 0), 
		   ioa_name(ioa->ioa_type, ioa->ioa_subtype));
	    *changed = 1;
	}

	if (ioa->ioa_diagval != EVDIAG_PASSED) {
	    syslog(LOG_ERR,
		"*** The %s at ioa %d on the IO4 in slot %d failed diags.\n",
		   ioa_name(ioa->ioa_type, ioa->ioa_subtype), unit, 
	           brd->eb_slot);
	    syslog(LOG_ERR, "***   Reason: %s\n", evdiag_msg(ioa->ioa_diagval));
	    *changed = 1;
	}

	/* We can't display messages about disabled F chips.  If there's
	 * nothing connected, Unix disables teh F chip automatically.
	 * If there's something else connected, the info is accurate.
	 */
	if (!ioa->ioa_enable && (ioa->ioa_type != IO4_ADAP_FCHIP))
	    syslog(LOG_NOTICE,
		"*** The %s at ioa %d on the IO4 in slot %d is DISABLED\n",
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
 * Include the diagnostic value strings.
 */

#include <sys/EVEREST/diagval_strs.i>

/*
 * board_name()
 *      Returns a string pointer to the name of the board whose
 *      type value was passed.
 */
char*
board_name(unchar type)
{
    switch(type) {
        case EVTYPE_WEIRDCPU:   return "Unknown CPU-type";
        case EVTYPE_IP19:       return "IP19";
        case EVTYPE_IP21:       return "IP21";
	case EVTYPE_IP25:	return "IP25";
        case EVTYPE_WEIRDMEM:   return "Unknown Memory-type";
        case EVTYPE_MC3:        return "MC3";
        case EVTYPE_WEIRDIO:    return "Unknown IO-type";
        case EVTYPE_IO4:        return "IO4";
        case EVTYPE_UNKNOWN:    return "Unknown";
	case EVTYPE_EMPTY:	return "Empty";
        default:                return "Unrecognized boardtype";
    }
}


/*
 * ioa_name
 *      Returns a string pointer to the name of the I/O adapter
 *      whose type value is passed.
 */
char*
ioa_name(unchar type, unchar subtype)
{
    switch (type) {
	case IO4_ADAP_SCSI:     	return "S1 SCSI controller";
        case IO4_ADAP_EPC:      	return "EPC Peripheral controller";
	case IO4_ADAP_FCHIP:
	    switch (subtype) {
	        case IO4_ADAP_FCG:     	return "FCI Graphics adapter";
                case IO4_ADAP_VMECC:   	return "VME adapter";
	        default:		return "Flat cable interconnect";
	    }
        case IO4_ADAP_NULL:     	return "Empty";
        default:                	return "Unknown adapter type";
    }
}

/*
 * evdiag_msg()
 *	Returns a pointer to a user-readable diagval message.
 */

char*
evdiag_msg(unchar diagcode)
{
    int num_entries;
    int i;

    num_entries = sizeof(diagval_map) / sizeof(diagval_t);

    for (i = 0; i < num_entries; i++) {
        if (diagval_map[i].dv_code == diagcode)
            return diagval_map[i].dv_msg;
    }

    return "Unknown diagnostic code";
}


int check_ccrev(evcfginfo_t *evconfig)
{
    int slot, slice;
    int enabled = -1;
    int minccrev = 0xffff;
    int maxccrev = 0;
    int error = 0;
    evbrdinfo_t *brd;
    char cpulist[36 * 4];
    char one_cpu[5];

    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
    /* Look at all enabled CPUs. */
	brd = &evconfig->ecfg_board[slot];

	if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_CPU)
		continue;

	/* At this point, we're an enabled CPU */
	if (enabled == -1) {
	    enabled = (brd->eb_cpu.eb_brdflags &
			EV_BFLAG_PIGGYBACK);
	} else if (enabled != (brd->eb_cpu.eb_brdflags &
			EV_BFLAG_PIGGYBACK)) {
		error++;
	}

	if (brd->eb_cpu.eb_ccrev < minccrev)
	    minccrev = brd->eb_cpu.eb_ccrev;
	if (brd->eb_cpu.eb_ccrev > maxccrev)
	    maxccrev = brd->eb_cpu.eb_ccrev;
    }

    if (minccrev == 0) {
	printf("WARNING: The IO4 Flash PROM on this system is out of date.\n");
	return -2;
    }

    if (enabled == -1) {
	printf("ERROR: No CPUs found.\n");
	return -1;
    }

    /* Report status */
    if (!error) {
	printf("Piggyback reads are currently %s.\n",
				enabled ? "enabled" : "disabled");
	if (!enabled && (minccrev < 2) && (maxccrev >= 2)) {
	    printf("  Some processors are capable of piggyback reads but are not enabled");
	}
	return error;
    }

    printf("ERROR: Piggyback reads are enabled on some CPUs but not others.\n");

    for (enabled = 0; enabled <= 1; enabled++) {
	cpulist[0] = '\0';

	for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
	    brd = &evconfig->ecfg_board[slot];

	    if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_CPU)
		    continue;

	    for (slice = 0; slice < EV_MAX_CPUS_BOARD; slice++) {
		if (!brd->eb_cpuarr[slice].cpu_enable ||
			 (brd->eb_cpuarr[slice].cpu_diagval == EVDIAG_NOTFOUND))
		    continue;

		/* At this point, we're an enabled CPU */
		if (enabled == (brd->eb_cpu.eb_brdflags &
				EV_BFLAG_PIGGYBACK)) {
		    sprintf(one_cpu, "%d ", brd->eb_cpuarr[slice].cpu_vpid);
		    strcat(cpulist, one_cpu);
		}
	    }
	}

	printf("  %s on CPUs %s\n", enabled ? "Disabled" : "Enabled",
			cpulist);
    }

    return error;
}

