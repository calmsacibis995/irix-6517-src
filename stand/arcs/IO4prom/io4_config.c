/***********************************************************************\
*	File:		io4_config.c					*
*									*
*	Contains initialization code for the IO4's in the system.	*
*	Right now, this file does little more than switch on 		*
*	the adapters and properly determine the master ID for		*
*	FCI devices.  If we need to do more extensive initialization,	*
*	we can do it here.						*
*									*
\***********************************************************************/

#ident "$Revision: 1.11 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/fchip.h>

/*
 * io4_config()
 *	Iterates through all of the IO4 boards in the system and
 *	switches on the IO adapters.
 */

int
io4_config()
{
    int slot;			/* Start with the largest legal slot */
    int num_ios = 0;		/* Number of IO4 boards found */
    int i;			/* IOA index */
    int adaps = 0;
    int	fchipFcgVid = 0, 	/* Virtual index into everror arrays for each type */
        fchipVmeccVid = 0,
        fchipHippiVid = 0,
        dangVid = 0, scsiVid = 0, 
        epcVid = 0, hipVid = 0;



    for (slot = EV_MAX_SLOTS - 1; slot >= 0; slot--) {
	
	/* Skip non-IO boards */
	if (EVCLASS(BOARD(slot)->eb_type) != EVCLASS_IO)
	    continue;

	/* 
  	 * Iterate through all of the IO adapters, and set virtual ID for the everror
	 * structure.
	 */
	for (i = 1; i < IO4_MAX_PADAPS; i++) {
	    evioacfg_t *ioa = &(BOARD(slot)->eb_ioarr[i]);
	    __psunsigned_t swin = SWIN_BASE(BOARD(slot)->eb_io.eb_winnum, i);

	    /* Switch on the adapter */
	    if (ioa->ioa_type != IO4_ADAP_NULL) {
		adaps |= (1 << i);
		EV_SET_CONFIG(slot, IO4_CONF_ADAP, adaps);
	    }
	
	    switch (ioa->ioa_type) {
	      case IO4_ADAP_EPC:
		ioa->ioa_virtid = epcVid++;
		break;

	      case IO4_ADAP_SCSI:
	      case IO4_ADAP_SCIP:
		ioa->ioa_virtid = scsiVid++;
		break;

	      case IO4_ADAP_FCHIP:
		/* HW Bug workaround, revision 3 of the Fchip does not respond to
		 * PIO at all if the FCI clock is absent (ie, nothing is connected to the FCI).
		 * So we better use badaddr to access the Fchip in case it doesn't respond,
		 * and skip everything else.
		 */
		if (badaddr((long long *)(swin + FCHIP_VERSION_NUMBER),4)) {
			ioa->ioa_type = IO4_ADAP_NULL;
			break;
		}

		/* Reset the FCI and read the master ID */
		store_double_lo((long long *)(swin + FCHIP_SW_FCI_RESET), 0);
		ioa->ioa_subtype = load_double_lo((long long *)(swin + FCHIP_MASTER_ID));
		/*
		 * based on the subtype, figure out which master fci to associate with.
		 */
		switch (ioa->ioa_subtype) {
		case IO4_ADAP_FCG:
		    ioa->ioa_virtid = fchipFcgVid++;
		    break;
		case IO4_ADAP_VMECC:
		    ioa->ioa_virtid = fchipVmeccVid++;
		    break;
		case IO4_ADAP_HIPPI:
		    ioa->ioa_virtid = fchipHippiVid++;
		    break;
		default:
		    break;	
		}
		break;
	      case IO4_ADAP_NULL:
		break;
	      case IO4_ADAP_DANG:
		/* Just set virtual ID */
		ioa->ioa_virtid = dangVid++;
		break;
	
	      default:
		printf("*** WARNING: Unrecognized IOA type: slot %d, ioa %d\n",
		       slot, i);
		break;
	    }
	} 

	num_ios++;
    }

    return num_ios; 
}

