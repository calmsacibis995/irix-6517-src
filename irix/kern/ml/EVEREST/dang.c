/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident 	"$Revision: 1.18 $"

#include <sys/types.h>
#include <sys/invent.h>
#include <sys/immu.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/io4.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/evintr.h>
/* macro used to get DANG regs as values instead of index into a 64bit array */
#define	DANG_REG_ADDRESS	1
#include <sys/EVEREST/dang.h>


dang_t	dang[EV_MAX_DANGADAPS];	/* Keep track of each DANG chip */
int	num_dangs = 0;		/* Number of dangs found in the system */

dangid_t dang_id[DANG_MAX_INDX]; /* Array used by lboot/drivers	*/

void
dang_init(unchar slot, unchar padap, unchar window)
{
	__psunsigned_t 	swin;
	uint		ver;

	if (num_dangs >= EV_MAX_DANGADAPS) {
		arcs_printf("Too many dang adapters. Max is %d \r\n",
				EV_MAX_DANGADAPS);
		arcs_printf("Ignoring dang adapter in Slot %d padap: %d\r\n",
				slot, padap);
		return;
	}

	/*
	 * Do software initialization of data structures
	 * associated with this DANG chip.
	 */
	dang[num_dangs].slot      = (char)slot;
	dang[num_dangs].adapter   = (char)padap;
	dang[num_dangs].window    = (char)window;
	dang[num_dangs].swin      = SWIN_BASE(window, padap);


	/*
	 * Do hardware init of the DANG chip.
	 * - set upper and middle GIO address registers.
	 * - read the other side of GIO to figure out the type of graphics
	 *   out there.
	 * We can't initialize everything here, the graphics or network
     	 * driver needs to decide on the setting of interrupts, ..etc.
	 */
	swin = dang[num_dangs].swin;
	EV_GET_REG(swin + DANG_PIO_ERR_CLR);	/* Clear any PIO error */
	EV_GET_REG(swin + DANG_DMAM_ERR_CLR);	/* Clear DMA master error */
	EV_GET_REG(swin + DANG_DMAS_ERR_CLR);	/* Clear DMA slave error */
	EV_GET_REG(swin + DANG_DMAM_COMPLETE_CLR); /* Clear DMAM complete */

	/* Set up GIO address register to some value */
	EV_SET_REG(swin + DANG_UPPER_GIO_ADDR, 7);
	EV_SET_REG(swin + DANG_MIDDLE_GIO_ADDR, 0);


	ver = (EV_GET_REG(swin + DANG_VERSION_REG)
		     & DANG_VERSION_MASK) >> DANG_VERSION_SHFT; 
	dang_id[DANG_SLOT2INX(slot, padap)] = 
			(ver >= DANG_VERSION_B) ? dang_probegio(num_dangs) : 0;

	num_dangs++;	
}	

/*
 * Given the logical ID of the DANG chip, return the virtual address
 * of a small window (64Kb) that can access the DANG chip.
 */
void *
dang_swin(int id)
{
	ASSERT(id < num_dangs);
	return((void *)dang[id].swin);
}

void
dang_setgiotype(int dang_num, int giobus_type)
{
	EV_SET_REG(dang[dang_num].swin + DANG_GIO64, 
		(giobus_type == PIOMAP_GIO64) ? 1 : 0);
}

/*
 * Given the logical ID of the DANG chip, return the window number
 * to access the DANG chip.
 */
int
dang_window(int id)
{
	ASSERT(id < num_dangs);
	return((int)dang[id].window);
}
int
dang_slot(int id)
{
	ASSERT(id < num_dangs);
	return((int)dang[id].slot);
}


int
dang_adapter(int id)
{
	ASSERT(id < num_dangs);
	return((int)dang[id].adapter);
}

/* OLD value was 0x2000 == 320/250 usecs at 25/33 Mhz 
 * NEW value 0x20000 == 5120/4000 usecs at 25/33 Mhz 
 */
#define	GIO_PROBE_TIMEOUT	0x20000

/*
 * Try to read a word from the given GIO address.
 * If read succeeds, return the value read. Otherwise, return zero.
 * Assumes NO GIO device has a valid id of zero
 */
int
dang_doprobe(int iadap, int type, __psunsigned_t ioaddr)
{
	volatile __psunsigned_t	addr;
	__psunsigned_t	swin;
	uint		probe_val = 0;

	swin	= dang[iadap].swin; 

	/* Setup the needed registers */
	EV_SET_REG(swin + DANG_UPPER_GIO_ADDR, GIO_UPPER(ioaddr));

	EV_SET_REG(swin + DANG_MIDDLE_GIO_ADDR, GIO_MIDDLE(ioaddr));

	/* Disable error interrupt */
	EV_SET_REG(swin + DANG_INTR_ENABLE, DANG_IRS_CERR);

	dang_setgiotype(iadap, type);

	/* Set MAX GIO timeout to some value */
	EV_SET_REG(swin + DANG_MAX_GIO_TIMEOUT, GIO_PROBE_TIMEOUT);

	addr =  swin + GIO_SWIN_OFFSET +
		GIO_SWIN_SLOT(ioaddr) + GIO_SWIN_BOARD(ioaddr);

	probe_val = *(unsigned int *)addr; 


#ifdef	DANG_DEBUG
	/* showconfig is not set by this time...*/
arcs_printf("dang probe: addr: %x ioaddr: %x probed val = 0x%x, Probing ", 
	addr, ioaddr, probe_val);
#endif

	if (EV_GET_REG(swin + DANG_INTR_STATUS) & DANG_IRS_GIOTIMEOUT){
		EV_SET_REG(swin + DANG_CLR_TIMEOUT, DANG_IRS_GIOTIMEOUT);
		probe_val = 0;
	}
#ifdef	DANG_DEBUG
	arcs_printf("%s\r\n", ((probe_val == 0) ? "Failed" : "Success"));
#endif

	return probe_val;
}

#define	GIO_GFX		LIO_ADDR
#define	GIO_SLOT0_BASE	LIO_ADDR+LIO_GFX_SIZE
#define	GIO_SLOT1_BASE	LIO_ADDR+LIO_GIO_SIZE
/*
 * dang_probegio:
 * Probe the GIO bus attached to Dang adapter 'iadap' and check if a graphics
 * or a network device is attached there. 
 * Algorithm:
 * Set the GIO type to GIO32.
 * 	Probe at address GIO_SLOT0_ADDR to check for  any network device. 
 *	If not, probe at GIO_GFX to check for a graphic device
 *	If not, probe at GIO_SLOT1_BASE for any other device..
 *	If success, return value of probe read..
 *	else return 0
 *
 */
int
dang_probegio(int iadap)
{
	uint		val;
	if (((val = dang_doprobe(iadap, PIOMAP_GIO32, GIO_SLOT0_BASE)) == 0) &&
	    ((val = dang_doprobe(iadap, PIOMAP_GIO32, GIO_GFX)) == 0) &&
	    ((val = dang_doprobe(iadap, PIOMAP_GIO32, GIO_SLOT1_BASE)) == 0))
		return 0;

	return val;
}
#undef	GIO_GFX
#undef	GIO_SLOT0_BASE
#undef	GIO_SLOT1_BASE


/*
 * Identify a Ebus interrupt level, and connect the interrupt handler passed
 * to that level 
 */

static struct dang_interrupts {
	char	level;
	char	alloc;
} dang_intrs[EV_MAX_DANGADAPS] = {
		(EVINTR_LEVEL_DANG_GIO+0), 0,
		(EVINTR_LEVEL_DANG_GIO+1), 0,
		(EVINTR_LEVEL_DANG_GIO+2), 0,
		(EVINTR_LEVEL_DANG_GIO+3), 0,
		(EVINTR_LEVEL_DANG_GIO+4), 0,
		(EVINTR_LEVEL_DANG_GIO+5), 0,
		(EVINTR_LEVEL_DANG_GIO+6), 0,
		(EVINTR_LEVEL_DANG_GIO1+0), 0,
		(EVINTR_LEVEL_DANG_GIO1+1), 0,
		(EVINTR_LEVEL_DANG_GIO1+2), 0,
		(EVINTR_LEVEL_DANG_GIO1+3), 0,
		(EVINTR_LEVEL_DANG_GIO1+4), 0
};

/*
 * Return:
 *	Level attached if success, 0 otherwise.
 */
int
dang_intr_conn(evreg_t *dang_reg, int dest, EvIntrFunc func, void *arg)
{
	int	i;

	for (i = 0; i < EV_MAX_DANGADAPS; i++)
		if (dang_intrs[i].level && (dang_intrs[i].alloc == 0)){
			evintr_connect( dang_reg, dang_intrs[i].level,
					SPLDEV, dest, func, arg);
			dang_intrs[i].alloc = 1;
			return	dang_intrs[i].level;
		}
	
	return 0;
}
