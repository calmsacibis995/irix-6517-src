/***********************************************************************\
*	File:		epc_config.c					*
*									*
*	Contains the code to do simple initialization of all of the	*
*	IO4s and EPCS in the system and find the master EPC.  If an	*
*	error occurs during initialization, we continue on to the 	*
*	next IO4 and hope it works.					*
*									*
\***********************************************************************/

#ident "$Revision: 1.21 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evintr.h>
#include <setjmp.h>
#include "ip21prom.h"
#include "prom_intr.h"
#include "prom_externs.h"
#include "pod_iadefs.h"
#include "pod_failure.h"

static int initialize_io4(evbrdinfo_t*, uint);
static int find_epc(evbrdinfo_t*);


/*
 * io4_initmaster()
 *  	Finds the IO4 in the highest slot and sets it up as the
 *	master IO4 by configuring its EPC.  At this point, we
 *	don't care about any of the other IO4s, so we just ignore them. 
 *	
 * Parameters:
 *	evcfg -- the Everest configuration information data structure.
 *	   On return, this data structure will be updated with the
 *	   configuration information for the master IO4.
 * Returns:
 *	0 if the master IO4 was found and configured correctly,
 *	EVDIAG_NOEPC1 if an error occurred.
 */

int
io4_initmaster(evcfginfo_t *evcfg)
{
    int slot;			/* Start with the largest legal slot */
    int winnum;			/* Window numbers */
    int diagval = 0;		/* Diagval for board */
    int master_slot = -1;	/* Master IO board's slot number (or -1) */
    int epcioa = 0;		/* Master EPC ioa (or 0) */

    /* If we're booting off of the second IO4, we need to numer
     * it with 1, but it's antisocial to leave the first one
     * unmapped so we number it 7.
     */
    if (evcfg->ecfg_debugsw & VDS_2ND_IO4)
	winnum = 7;
    else
	winnum = 1;

    /*
     * Find the master IO4 by searching until we find the IO4 board
     * in the highest numbered slot (unless VDS_2ND_IO4 is set).
     */ 
    evcfg->ecfg_epcioa = 0;
    for (slot = EV_MAX_SLOTS - 1; slot >= 0; slot--) {
	if (evcfg->ecfg_board[slot].eb_type == EVTYPE_IO4) {
	    evcfg->ecfg_board[slot].eb_slot = slot;

	    /* Set IO4 board rev to IA chip type and rev */
	    evcfg->ecfg_board[slot].eb_rev = 
				EV_GET_CONFIG(slot, IO4_CONF_REVTYPE) &
				(IO4_TYPE_MASK | IO4_REV_MASK);

	    if (winnum == 1) {
		master_slot = slot;
		if (!(evcfg->ecfg_debugsw & VDS_NO_DIAGS))
			diagval = check_io4(winnum, slot);
		else
			diagval = 0;
	    } else {
		diagval = 0;
	    }

	    if (diagval == 0) {
		diagval = initialize_io4(&(evcfg->ecfg_board[slot]), winnum);
		if (!diagval && (winnum == 1) &&
				(!(evcfg->ecfg_debugsw & VDS_NO_DIAGS)))
		    diagval = check_iaram(winnum, slot);
	    }

	    if (diagval == 0) {
		/* Try and find the master EPC */
		if (winnum == 1) {
		    epcioa = find_epc(&(evcfg->ecfg_board[slot]));
		    if (epcioa && !(evcfg->ecfg_debugsw & VDS_NO_DIAGS))
		        evcfg->ecfg_board[slot].eb_ioarr[epcioa].ioa_diagval =
					pod_check_epc(slot, epcioa, winnum);
		}
		winnum++;

		/* If we're booting off of the second IO4, we need to numer
		 * it with 1, but it's antisocial to leave the first one
		 * unmapped so we number it 7.
		 */
		if (evcfg->ecfg_debugsw & VDS_2ND_IO4)
		    if (winnum >= 8)
			winnum = 1;
	    } else {
		evcfg->ecfg_board[slot].eb_diagval = diagval;
	    } 
	} 
    }

    evcfg->ecfg_epcioa = epcioa;

    if (master_slot == -1)
	return EVDIAG_NOIO4;

    if (evcfg->ecfg_board[master_slot].eb_diagval)
	return evcfg->ecfg_board[master_slot].eb_diagval;

    return (epcioa ? evcfg->ecfg_board[master_slot].eb_ioarr[epcioa].ioa_diagval
		   : EVDIAG_NOEPC);
}


/*
 * initialize_io4()
 * 	Examines the IO adaptors of the IO4 in the given slot
 * 	and initializes the IO4 configuration registers appropriately.
 */

static int
initialize_io4(evbrdinfo_t *io4info, uint window)
{
    int iodev[2];		/* The I/O device configuration registers */ 
    int i;			/* IOA index */
    int value;			/* IO adapter value */
    int slot;			/* Slot containing this window */
    unsigned int adaps = 0;	/* Adapter activation mask */
    jmp_buf fault_buf;		/* Jump buffer for returning from exceptions */
    unsigned *old_buf;		/* Previous fault handler buffer */

    /* Setup the slot and window information fields */
    slot = io4info->eb_slot;
    io4info->eb_io.eb_winnum = window;

    if (setfault(fault_buf, &old_buf)) {
        return EVDIAG_CFGBUSERR;
    }

    /* Set the window bases, the endianess, and some interrupt info */
    EV_SET_CONFIG(slot, IO4_CONF_LW,  window);
    EV_SET_CONFIG(slot, IO4_CONF_SW, window);
    EV_SET_CONFIG(slot, IO4_CONF_ENDIAN, !getendian());
    EV_GET_CONFIG_NOWAR(slot, IO4_CONF_IBUSERRORCLR);
    EV_GET_CONFIG_NOWAR(slot, IO4_CONF_EBUSERRORCLR);
    EV_SET_CONFIG(slot, IO4_CONF_INTRMASK, 0x2);
    EV_SET_CONFIG(slot, IO4_CONF_INTRVECTOR, IO4ERR_VECTOR);
    
    /*
     * Get information about the IO adapaters and buiild
     * up the configuration mask appropriately.
     */

    iodev[0] = EV_GET_CONFIG(slot, IO4_CONF_IODEV0);
    iodev[1] = EV_GET_CONFIG(slot, IO4_CONF_IODEV1);
   
    for (i = 1; i < IO4_MAX_PADAPS; i++) {
	value = (iodev[i>>2]  >> (8 * (i & 0x3))) & 0xff;

	/* Switch on iff it's an EPC */
	if (value == IO4_ADAP_EPC)
	    adaps |= (1 << i); 

	io4info->eb_ioarr[i].ioa_type = value;
	io4info->eb_ioarr[i].ioa_virtid = 0;		
    } 

    EV_SET_CONFIG(slot, IO4_CONF_ADAP, adaps);

    restorefault(old_buf); 
    return 0;
}


/*
 * find_epc()
 *	Examines the given IO4 board and tries to initialize
 *	the EPC on it.  If it fails, it prints a message and
 *	returns 0.  Otherwise, it returns the padap number.
 */

static int
find_epc(evbrdinfo_t *io4info)
{
    uint i;			/* Index variable for iterating thru padaps */
    uint window;		/* Window of this IO4 */
    jmp_buf fault_buf; 		/* Jump buffer for returning from exceptions */
    unsigned *old_buf;		/* Previous jump buffer */

    sysctlr_message("Initing EPC...");
    window = io4info->eb_io.eb_winnum;
    for (i = 1; i < IO4_MAX_PADAPS; i++) {
	if (io4info->eb_ioarr[i].ioa_type == IO4_ADAP_EPC) {

	    if (setfault(fault_buf, &old_buf)) {
		sysctlr_message("EPC init failed");
		restorefault(old_buf);
		return 0;
	    }

	    EPC_PROMSET(window,i, EPC_IERR, 0x1000000);
	    EPC_PROMGET(window,i, EPC_IERRC);
	    EPC_PROMSET(window,i, EPC_PRSTCLR, 0x3ff);

	    /* Set the EPC IOA value in the BSR */
	    set_BSR((i << BS_EPCIOA_SHFT) | get_BSR());

	    restorefault(old_buf);
	    return i;
	}
    }

    restorefault(old_buf);
    return 0;
}

 
/*
 * master_epc_adap()
 * 	Looks up the PADAP of the EPC on the master IO4 and 
 * 	returns it.  The padap is stashed in the BSR.
 */

__scunsigned_t
master_epc_adap(void)
{
    return ((get_BSR() & BS_EPCIOA_MASK) >> BS_EPCIOA_SHFT);
}
