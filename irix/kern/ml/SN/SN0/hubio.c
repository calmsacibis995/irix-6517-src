/**************************************************************************
 *                                                                        *
 *                  Copyright (C) 1995 Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.110 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/dmamap.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/xtalk/xwidget.h>

#include "sn0_private.h"
#include <sys/SN/addrs.h>
#include <sys/SN/agent.h>
#include <sys/SN/intr.h>
#include <sys/SN/xtalkaddrs.h>
#include <sys/SN/klconfig.h>
#include <sys/xtalk/xwidget.h>
#if defined (HUB_II_IFDR_WAR)
#include <sys/PCI/bridge.h>
hubreg_t 	global_kick_icmr;
#endif

#ifdef BRIDGE_B_DATACORR_WAR
#include <sys/PCI/pciio.h>
#include <sys/ql.h>
#include <sys/PCI/pcibr.h>
#include <sys/PCI/pcibr_private.h>
extern int bridge_rev_b_data_check_disable;
#endif

#include "../error_private.h"

extern xtalk_provider_t hub_provider;


/* Error Handling */

/*
 * hub_ioerror_local
 *	An error happened while accessing the hub local registers.
 * 	(Pretty rare in normal situations, but common in sable
 *	error injection mode. This could even go away later!!).
 *
 * 	ioerror structure has information about the <cpu, node> that
 *	did the access. Check its permissions, and take appropriate
 *	action.
 */
/*ARGSUSED*/
hub_ioerror_local(
	vertex_hdl_t	hub_v,
	int		error_code,
	ioerror_mode_t	mode,
	ioerror_t	*ioerror)
{
	hubreg_t	protect;
	nasid_t		errnasid;
	nasid_t		srcnasid;
	hubreg_t	hubaddr;
	/*
	 * We should come here only for PIO errors. There should never 
	 * be a DMA error for Hub IO space!!.
	 */
	ASSERT((error_code == PIO_READ_ERROR)||(error_code == PIO_WRITE_ERROR));

	IOERROR_DUMP("hub_ioerror_local", error_code, mode, ioerror);

	/*
	 * Nothing to do in case of probing or user level error
	 */
	if ((mode == MODE_DEVPROBE)  || (mode == MODE_DEVUSERERROR))
		return IOERROR_HANDLED;

	if (mode == MODE_DEVREENABLE) {
		/* 
		 * No re-enabling is needed as part of Hub register access
		 * error handling. 
		 */
		return IOERROR_HANDLED;
	}

	/* 
	 * Check if the <srccpu, srcnode> combo has permission to read/write
	 * to <targnode>. If it does, print an error message, cleanup,
	 * and return. Otherwise, panic.
	 */
	
	
	errnasid = COMPACT_TO_NASID_NODEID(IOERROR_GETVALUE(ioerror,errnode));
	srcnasid  = COMPACT_TO_NASID_NODEID(IOERROR_GETVALUE(ioerror,srcnode));

	if (IOERROR_FIELDVALID(ioerror, hubaddr))
		hubaddr = IOERROR_GETVALUE(ioerror, hubaddr);
	else	hubaddr = 0;

	protect = REMOTE_HUB_L(errnasid, PI_CPU_PROTECT);

	if (protect & (1ULL << srcnasid)){
		/*
		 * Hmm, Hub "srcnasid" is enabled to access registers 
		 * on "errnasid". This has to be error injection
		 * that's coming here. 
		 * If in sable mode, return. Otherwise panic.
		 */
#ifdef	SABLE
		return IOERROR_HANDLED;
#else	/* !SABLE */
		cmn_err(CE_WARN, 
		"Error accessing Hub Register %x on node (%d/%d) from (%d/%d), with access permission\n",
		hubaddr,
		errnasid, IOERROR_GETVALUE(ioerror,errnode), srcnasid, IOERROR_GETVALUE(ioerror,srcnode));
		ASSERT(0);
		kl_panic("Hub Register Access error");
#endif	/* SABLE */
		/*NOTREACHED*/
	}

	cmn_err(CE_WARN, 
		"Error in accessing hub register 0x%x on node (%d/%d) by cpu %d on Node (%d/%d): No permission\n",
		IOERROR_GETVALUE(ioerror,hubaddr), errnasid, IOERROR_GETVALUE(ioerror,errnode),
		srcnasid, IOERROR_GETVALUE(ioerror,srcnode));
	ASSERT(0);
	kl_panic("Hub Register Access Permission error");
	/*NOTREACHED*/

}

/*
 * Get the xtalk provider function pointer for the
 * specified hub.
 */

/*ARGSUSED*/
int
hub_xp_error_handler(
	vertex_hdl_t 	hub_v, 
	nasid_t		nasid, 
	int		error_code, 
	ioerror_mode_t	mode, 
	ioerror_t	*ioerror)
{
	/*REFERENCED*/
	hubreg_t	iio_imem;

	/*
	 * Before walking down to the next level, check if
	 * the I/O link is up. If it's been disabled by the 
	 * hub ii for some reason, we can't even touch the
	 * widget registers.
	 */
	iio_imem = REMOTE_HUB_L(nasid, IIO_IMEM);

	if (!(iio_imem & (IIO_IMEM_B0ESD|IIO_IMEM_W0ESD))){
		/* 
		 * IIO_IMEM_B0ESD getting set, indicates II shutdown
		 * on HUB0 parts.. Hopefully that's not true for 
		 * Hub1 parts..
		 *
		 *
		 * If either one of them is shut down, can't
		 * go any further.
		 */
		return IOERROR_XTALKLEVEL;
	}

	/* hand the error off to the switch or the directly
	 * connected crosstalk device.
	 */
	return xtalk_error_handler(nodepda->basew_xc,
				   error_code, mode, ioerror);
}

/*
 * hub_error_devenable
 *	Re-enable the device causing error. 
 *	This will be invoked from the lower layer, trying to re-enable
 *	a specific device.
 *	xconn_vhdl -> is the xwidget to which the error device is attached.
 *	error_code -> error found while traversing from top.
 *
 *	For PIO errors, this routine is responsible for cleaning up
 *	any cruft left in the PRBs.
 *	For DMA Write error, we need to figure out a way to reenable
 *	the device that did the bad DMA.
 *
 */
/*ARGSUSED*/
int
hub_error_devenable(vertex_hdl_t xconn_vhdl, int devnum, int error_code)
{
	xwidget_info_t winfo = xwidget_info_get(xconn_vhdl);
	xwidgetnum_t widget  = xwidget_info_id_get(winfo);
	vertex_hdl_t hubv    = xwidget_info_master_get(winfo);
	hubinfo_t 	hubinfo;
	nasid_t		nasid;
	iprte_a_t	iprte;		/* PIO read table entry format */
	
	hubinfo_get(hubv, &hubinfo);
	nasid = hubinfo->h_nasid;

	switch(error_code) {
	case PIO_READ_ERROR:
		/* 
		 * Release PIO read table entry made sticky.
		 */
		iprte.entry = REMOTE_HUB_L(nasid, IIO_WIDPRTE(widget));
		if (iprte.iprte_valid) {
			/*
			 * Ideally, we are required to check if the PRTE
			 * belongs to this error, and cleanup only then.
			 * Let's not bother about it now..
			(iprte.iprte_addr == 
				(IOERROR_GETVALUE(ioerror,hubaddr) >> IPRTE_ADDRSHFT)))
			*/

			/* Release this PIO read table entry */

			REMOTE_HUB_S(nasid, IIO_IPDR, (widget | IIO_IPDR_PND));
			/* 
			 * Hub clears the IIO_IPDR_PND bit on freeing 
			 * this entry.
			 * Assert checks that it's cleared.  
			 * Hitting this assert
			 * indicates looping on this bit is needed. 
			 */
			ASSERT(! (REMOTE_HUB_L(nasid, IIO_IPDR) & 
							IIO_IPDR_PND));
		}

		return IOERROR_HANDLED;
	case PIO_WRITE_ERROR:
		/*
		 * No cleanup in case of PIO write error. Just return.
		 */
		return IOERROR_HANDLED;

	default:
		ASSERT(0);	/* TBD */
		return IOERROR_HANDLED;
	}
}

/*
 * Hub IO error handling.
 *
 *	Gets invoked for different types of errors found at the hub. 
 *	Typically this includes situations from bus error or due to 
 *	an error interrupt (mostly generated at the hub).
 */
int
hub_ioerror_handler(
	vertex_hdl_t 	hub_v, 
	int		error_code,
	ioerror_mode_t	mode,
	ioerror_t	*ioerror)
{
	hubinfo_t 	hinfo; 		/* Hub info pointer */
	nasid_t		nasid;
	int		retval;
	/*REFERENCED*/
	iprte_a_t	iprte;		/* PIO read table entry format */
	/*REFERENCED*/
	hubreg_t	erraddr;

	IOERROR_DUMP("hub_ioerror_handler", error_code, mode, ioerror);

	hubinfo_get(hub_v, &hinfo);

	if (!hinfo){
		/* Print an error message and return */
		goto end;
	}
	nasid = hinfo->h_nasid;

	switch(error_code) {

	case PIO_READ_ERROR:
		/* 
		 * Cpu got a bus error while accessing IO space.
		 * hubaddr field in ioerror structure should have
		 * the IO address that caused access error.
		 */

		/*
		 * Identify if  the physical address in hub_error_data
		 * corresponds to small/large window, and accordingly,
		 * get the xtalk address.
		 */

		/*
		 * Evaluate the widget number and the widget address that
		 * caused the error. Use 'vaddr' if the address is a 
		 * KSEG1 address. This is typically true either during probing
		 * or a kernel driver getting into trouble. 
		 * Otherwise, use paddr to figure out widget details
		 * This is typically true for user mode bus errors while
		 * accessing I/O space.
		 */
		 if (IS_KSEG1(IOERROR_GETVALUE(ioerror,vaddr))){
		    /* 
		     * If neither in small window nor in large window range,
		     * outright reject it.
		     */
		    if (NODE_SWIN_ADDR(nasid, (paddr_t)IOERROR_GETVALUE(ioerror,vaddr))){
			iopaddr_t	hubaddr = IOERROR_GETVALUE(ioerror,hubaddr);
			xwidgetnum_t	widgetnum = SWIN_WIDGETNUM(hubaddr);
			iopaddr_t	xtalkaddr = SWIN_WIDGETADDR(hubaddr);
			/* 
			 * differentiate local register vs IO space access
			 */
			IOERROR_SETVALUE(ioerror,widgetnum,widgetnum);
			IOERROR_SETVALUE(ioerror,xtalkaddr,xtalkaddr);


		    } else if (NODE_BWIN_ADDR(nasid, (paddr_t)IOERROR_GETVALUE(ioerror,vaddr))){
			/* 
			 * Address corresponds to large window space. 
			 * Convert it to xtalk address.
			 */
			int		bigwin;
			hub_piomap_t    bw_piomap;
			xtalk_piomap_t	xt_pmap;
			iopaddr_t	hubaddr;
			xwidgetnum_t	widgetnum;
			iopaddr_t	xtalkaddr;

			hubaddr = IOERROR_GETVALUE(ioerror,hubaddr);

			/*
			 * Have to loop to find the correct xtalk_piomap 
			 * because the're not allocated on a one-to-one
			 * basis to the window number.
			 */
			for (bigwin=0; bigwin < HUB_NUM_BIG_WINDOW; bigwin++) {
				bw_piomap = hubinfo_bwin_piomap_get(hinfo,
								    bigwin);

				if (bw_piomap->hpio_bigwin_num ==
				    (BWIN_WINDOWNUM(hubaddr) - 1)) {
					xt_pmap = hub_piomap_xt_piomap(bw_piomap);
					break;
				}
			}

			ASSERT(xt_pmap);

			widgetnum = xtalk_pio_target_get(xt_pmap);
			xtalkaddr = xtalk_pio_xtalk_addr_get(xt_pmap) + BWIN_WIDGETADDR(hubaddr);

			IOERROR_SETVALUE(ioerror,widgetnum,widgetnum);
			IOERROR_SETVALUE(ioerror,xtalkaddr,xtalkaddr);

			/* 
			 * Make sure that widgetnum doesnot map to hub 
			 * register widget number, as we never use
			 * big window to access hub registers. 
			 */
			ASSERT(widgetnum != HUB_REGISTER_WIDGET);
		    }
		} else if (IOERROR_FIELDVALID(ioerror,hubaddr)) {
			iopaddr_t	hubaddr = IOERROR_GETVALUE(ioerror,hubaddr);
			xwidgetnum_t	widgetnum;
			iopaddr_t	xtalkaddr;
			if (BWIN_WINDOWNUM(hubaddr)){
				int 	window = BWIN_WINDOWNUM(hubaddr) - 1;
				hubreg_t itte;
				itte = (hubreg_t)HUB_L(IIO_ITTE_GET(nasid, window));
				widgetnum =  (itte >> IIO_ITTE_WIDGET_SHIFT) & 
						IIO_ITTE_WIDGET_MASK;
				xtalkaddr = (((itte >> IIO_ITTE_OFFSET_SHIFT) &
					IIO_ITTE_OFFSET_MASK) << 
					     BWIN_SIZE_BITS) +
					BWIN_WIDGETADDR(hubaddr);
			} else {
				widgetnum = SWIN_WIDGETNUM(hubaddr);
				xtalkaddr = SWIN_WIDGETADDR(hubaddr);
			}
			IOERROR_SETVALUE(ioerror,widgetnum,widgetnum);
			IOERROR_SETVALUE(ioerror,xtalkaddr,xtalkaddr);
		} else {
			IOERROR_DUMP("hub_ioerror_handler", error_code, 
						mode, ioerror);
			IOERR_PRINTF(cmn_err(CE_NOTE, 
				"hub_ioerror_handler: Invalid address passed"));

			return IOERROR_INVALIDADDR;
		}


		if (IOERROR_GETVALUE(ioerror,widgetnum) == HUB_REGISTER_WIDGET) {
			/* 
			 * Error in accessing Hub local register
			 * This should happen mostly in SABLE mode..
			 */
			retval = hub_ioerror_local(hub_v, error_code, 
							mode, ioerror);
		} else {
			retval = hub_xp_error_handler(
				hub_v, nasid, error_code, mode, ioerror);

		}

		IOERR_PRINTF(cmn_err(CE_NOTE,
			"hub_ioerror_handler:PIO_READ_ERROR return: %d",
				retval));

		break;

	case PIO_WRITE_ERROR:
		/*
		 * This hub received an interrupt indicating a widget 
		 * attached to this hub got a timeout. 
		 * widgetnum field should be filled to indicate the
		 * widget that caused error.
		 *
		 * NOTE: This hub may have nothing to do with this error.
		 * We are here since the widget attached to the xbow 
		 * gets its PIOs through this hub.
		 *
		 * There is nothing that can be done at this level. 
		 * Just invoke the xtalk error handling mechanism.
		 */
		if (IOERROR_GETVALUE(ioerror,widgetnum) == HUB_REGISTER_WIDGET) {
			retval = hub_ioerror_local(hub_v, error_code,
							mode, ioerror);
		} else {
			retval = hub_xp_error_handler(
				hub_v, nasid, error_code, mode, ioerror);
		}
		break;
	
	case DMA_READ_ERROR:
		/* 
		 * DMA Read error always ends up generating an interrupt
		 * at the widget level, and never at the hub level. So,
		 * we don't expect to come here any time
		 */
		ASSERT(0);
		retval = IOERROR_UNHANDLED;
		break;

	case DMA_WRITE_ERROR:
		/*
		 * DMA Write error is generated when a write by an I/O 
		 * device could not be completed. Problem is, device is
		 * totally unaware of this problem, and would continue
		 * writing to system memory. So, hub has a way to send
		 * an error interrupt on the first error, and bitbucket
		 * all further write transactions.
		 * Coming here indicates that hub detected one such error,
		 * and we need to handle it.
		 *
		 * Hub interrupt handler would have extracted physaddr, 
		 * widgetnum, and widgetdevice from the CRB 
		 *
		 * There is nothing special to do here, since gathering
		 * data from crb's is done elsewhere. Just pass the 
		 * error to xtalk layer.
		 */
		retval = IOERROR_UNHANDLED;
		break;
	
	default:
		ASSERT(0);
		return IOERROR_BADERRORCODE;
	
	}
	
	/*
	 * If error was not handled, we may need to take certain action
	 * based on the error code.
	 * For e.g. in case of PIO_READ_ERROR, we may need to release the
	 * PIO Read entry table (they are sticky after errors).
	 * Similarly other cases. 
	 *
	 * Further Action TBD 
	 */
end:	
	if (retval == IOERROR_HWGRAPH_LOOKUP) {
		/*
		 * If we get errors very early, we can't traverse
		 * the path using hardware graph. 
		 * To handle this situation, we need a functions
		 * which don't depend on the hardware graph vertex to 
		 * handle errors. This break the modularity of the
		 * existing code. Instead we print out the reason for
		 * not handling error, and return. On return, all the
		 * info collected would be dumped. This should provide 
		 * sufficient info to analyse the error.
		 */
		printf("Unable to handle IO error: hardware graph not setup\n");
	}

	return retval;
}


void 
crbx(nasid_t nasid, void (*pf)(char *, ...))
{
  int ind;
  icrba_t crb_a;
  icrbb_t crb_b;
  icrbc_t crb_c;
  icrbd_t crb_d;
  char imsgtype[3];
  char imsg[12];
  char reqtype[6];
  /*REFERENCED*/
  char *xtsize_string[] = {"DW", "QC", "FC", "??"};
  char initiator[6];

  pf("\nNASID %d:\n", nasid);
  pf("   ________________CRB_A____________   _________________CRB_B_______________________________  __________CRB_C__________  ___CRB_D___\n");
  pf("   S S E E L                             C    S     I                      A           I S S  P P B           S   B      T C      T \n");
  pf("   B B R R N M X S T                   B O S  R   U M  I       I     E     C   R   H   N T T  R R T           U   A R    O T C    M \n");
  pf("   T T R R U A E I N             V I   T H I  C   O T  M       N     X     K   E A O   T L L  C P E           P   R R G  V X T    O \n");
  pf("   E E O C C R R D U             L O   E T Z  N   L Y  S       I     C     C   S C L W V I I  N S O           P   O Q B  L T X    U \n");
  pf("   0 1 R D E K R N M  ADDRESS    D W   # R E  D   D P  G       T     L     T   P K D B N B N  T C P PA_BE     L   P D R  D V T    T \n");
  pf("   - - - - - - - - -  -------    - -   - - -- -   - -  -       -     -     -   - - - - - - -  - - - ------    -   - - -  - - -    - \n");
  for (ind = 0; ind < 15; ind++) {
    crb_a.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_A(ind));
    crb_b.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_B(ind));
    crb_c.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_C(ind));
    crb_d.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_D(ind));
    switch (crb_b.icrbb_field_s.imsgtype) {
    case 0:  
      strcpy(imsgtype, "XT");
      /*
       * In the xtalk fields, Only a few bits are valid.
       * The valid fields bit mask is 0xAF i.e bits 7, 5, 3..0 are
       * the only valid bits. We can ignore the values in the
       * other bit. 
       */
      switch (crb_b.icrbb_field_s.imsg & 0xAF) {
      case 0x00: strcpy(imsg, "PRDC   "); break;
      case 0x20: strcpy(imsg, "BLKRD  "); break;
      case 0x02: strcpy(imsg, "PWRC   "); break;
      case 0x22: strcpy(imsg, "BLKWR  "); break;
      case 0x06: strcpy(imsg, "FCHOP  "); break;
      case 0x08: strcpy(imsg, "STOP   "); break;
      case 0x80: strcpy(imsg, "RDIO   "); break;
      case 0x82: strcpy(imsg, "WRIO   "); break;
      case 0xA2: strcpy(imsg, "PTPWR  "); break;
      default:   sprintf(imsg, "??0x%02x?",crb_b.icrbb_field_s.imsg); break;
      } 
      break;
    case 1:  
      strcpy(imsgtype, "BT"); 
      switch (crb_b.icrbb_field_s.imsg) {
      case 0x01: strcpy(imsg, "BSHU   "); break;
      case 0x02: strcpy(imsg, "BEXU   "); break;
      case 0x04: strcpy(imsg, "BWINV  "); break;
      case 0x08: strcpy(imsg, "BZERO  "); break;
      case 0x10: strcpy(imsg, "BDINT  "); break;
      case 0x20: strcpy(imsg, "INTR   "); break;
      case 0x40: strcpy(imsg, "BPUSH  "); break;
      default:   sprintf(imsg, "??0x%02x?",crb_b.icrbb_field_s.imsg); break;
      } 
      break;
    case 2:  
      strcpy(imsgtype, "LN"); 
      switch (crb_b.icrbb_field_s.imsg) {
      case 0x61: strcpy(imsg, "VRD    "); break;
      case 0x62: strcpy(imsg, "VWR    "); break;
      case 0x64: strcpy(imsg, "VXCHG  "); break;
      case 0xe9: strcpy(imsg, "VRPLY  "); break;
      case 0xea: strcpy(imsg, "VWACK  "); break;
      case 0xec: strcpy(imsg, "VXACK  "); break;
      case 0xf9: strcpy(imsg, "VERRA  "); break;
      case 0xfa: strcpy(imsg, "VERRC  "); break;
      case 0xfb: strcpy(imsg, "VERRCA "); break;
      case 0xfc: strcpy(imsg, "VERRP  "); break;
      case 0xfd: strcpy(imsg, "VERRPA "); break;
      case 0xfe: strcpy(imsg, "VERRPC "); break;
      case 0xff: strcpy(imsg, "VERRPCA"); break;
      case 0x00: strcpy(imsg, "RDSH   "); break;
      case 0x01: strcpy(imsg, "RDEX   "); break;
      case 0x02: strcpy(imsg, "READ   "); break;
      case 0x03: strcpy(imsg, "RSHU   "); break;
      case 0x04: strcpy(imsg, "REXU   "); break;
      case 0x05: strcpy(imsg, "UPGRD  "); break;
      case 0x06: strcpy(imsg, "RLQSH  "); break;
      case 0x08: strcpy(imsg, "IRSHU  "); break;
      case 0x09: strcpy(imsg, "IREXU  "); break;
      case 0x0a: strcpy(imsg, "IRDSH  "); break;
      case 0x0b: strcpy(imsg, "IRDEX  "); break;
      case 0x0c: strcpy(imsg, "IRMVE  "); break;
      case 0x0f: strcpy(imsg, "INVAL  "); break;
      case 0x10: strcpy(imsg, "PRDH   "); break;
      case 0x11: strcpy(imsg, "PRDI   "); break;
      case 0x12: strcpy(imsg, "PRDM   "); break;
      case 0x13: strcpy(imsg, "PRDU   "); break;
      case 0x14: strcpy(imsg, "PRIHA  "); break;
      case 0x15: strcpy(imsg, "PRIHB  "); break;
      case 0x16: strcpy(imsg, "PRIRA  "); break;
      case 0x17: strcpy(imsg, "PRIRB  "); break;
      case 0x18: strcpy(imsg, "ODONE  "); break;
      case 0x2f: strcpy(imsg, "LINVAL "); break;
      case 0x30: strcpy(imsg, "PWRH   "); break;
      case 0x31: strcpy(imsg, "PWRI   "); break;
      case 0x32: strcpy(imsg, "PWRM   "); break;
      case 0x33: strcpy(imsg, "PWRU   "); break;
      case 0x34: strcpy(imsg, "PWIHA  "); break;
      case 0x35: strcpy(imsg, "PWIHB  "); break;
      case 0x36: strcpy(imsg, "PWIRA  "); break;
      case 0x37: strcpy(imsg, "PWIRB  "); break;
      case 0x38: strcpy(imsg, "GFXWS  "); break;
      case 0x40: strcpy(imsg, "WB     "); break;
      case 0x41: strcpy(imsg, "WINV   "); break;
      case 0x50: strcpy(imsg, "GFXWL  "); break;
      case 0x51: strcpy(imsg, "PTPWR  "); break;
      case 0x80: strcpy(imsg, "SACK   "); break;
      case 0x81: strcpy(imsg, "EACK   "); break;
      case 0x82: strcpy(imsg, "UACK   "); break;
      case 0x83: strcpy(imsg, "UPC    "); break;
      case 0x84: strcpy(imsg, "UPACK  "); break;
      case 0x85: strcpy(imsg, "AERR   "); break;
      case 0x86: strcpy(imsg, "PERR   "); break;
      case 0x87: strcpy(imsg, "IVACK  "); break;
      case 0x88: strcpy(imsg, "WERR   "); break;
      case 0x89: strcpy(imsg, "WBEAK  "); break;
      case 0x8a: strcpy(imsg, "WBBAK  "); break;
      case 0x8b: strcpy(imsg, "DNACK  "); break;
      case 0x8c: strcpy(imsg, "SXFER  "); break;
      case 0x8d: strcpy(imsg, "PURGE  "); break;
      case 0x8e: strcpy(imsg, "DNGRD  "); break;
      case 0x8f: strcpy(imsg, "XFER   "); break;
      case 0x90: strcpy(imsg, "PACK   "); break;
      case 0x91: strcpy(imsg, "PACKH  "); break;
      case 0x92: strcpy(imsg, "PACKN  "); break;
      case 0x93: strcpy(imsg, "PNKRA  "); break;
      case 0x94: strcpy(imsg, "PNKRB  "); break;
      case 0x95: strcpy(imsg, "GFXCS  "); break;
      case 0x96: strcpy(imsg, "GFXCL  "); break;
      case 0x97: strcpy(imsg, "PTPCR  "); break;
      case 0x98: strcpy(imsg, "WIC    "); break;
      case 0x99: strcpy(imsg, "WACK   "); break;
      case 0x9a: strcpy(imsg, "WSPEC  "); break;
      case 0x9b: strcpy(imsg, "RACK   "); break;
      case 0x9c: strcpy(imsg, "BRMVE  "); break;
      case 0x9d: strcpy(imsg, "DERR   "); break;
      case 0x9e: strcpy(imsg, "PRERR  "); break;
      case 0x9f: strcpy(imsg, "PWERR  "); break;
      case 0xa0: strcpy(imsg, "BINV   "); break;
      case 0xa3: strcpy(imsg, "WTERR  "); break;
      case 0xa4: strcpy(imsg, "RTERR  "); break;
      case 0xa7: strcpy(imsg, "SPRPLY "); break;
      case 0xa8: strcpy(imsg, "ESPRPLY"); break;
      case 0xb0: strcpy(imsg, "PRPLY  "); break;
      case 0xb1: strcpy(imsg, "PNAKW  "); break;
      case 0xb2: strcpy(imsg, "PNKWA  "); break;
      case 0xb3: strcpy(imsg, "PNKWB  "); break;
      case 0xc0: strcpy(imsg, "SRESP  "); break;
      case 0xc1: strcpy(imsg, "SRPLY  "); break;
      case 0xc2: strcpy(imsg, "SSPEC  "); break;
      case 0xc4: strcpy(imsg, "ERESP  "); break;
      case 0xc5: strcpy(imsg, "ERPC   "); break;
      case 0xc6: strcpy(imsg, "ERPLY  "); break;
      case 0xc7: strcpy(imsg, "ESPEC  "); break;
      case 0xc8: strcpy(imsg, "URESP  "); break;
      case 0xc9: strcpy(imsg, "URPC   "); break;
      case 0xca: strcpy(imsg, "URPLY  "); break;
      case 0xcc: strcpy(imsg, "SXWB   "); break;
      case 0xcd: strcpy(imsg, "PGWB   "); break;
      case 0xce: strcpy(imsg, "SHWB   "); break;
      case 0xd8: strcpy(imsg, "BRDSH  "); break;
      case 0xd9: strcpy(imsg, "BRDEX  "); break;
      case 0xda: strcpy(imsg, "BRSHU  "); break;
      case 0xdb: strcpy(imsg, "BREXU  "); break;
      default:   sprintf(imsg, "??0x%02x?",crb_b.icrbb_field_s.imsg); break;
      } 
      break;
    case 3:  
      strcpy(imsgtype, "CR"); 
      switch (crb_b.icrbb_field_s.imsg) {
      case 0x00: strcpy(imsg, "NOP"); break;
      case 0x01: strcpy(imsg, "WAKEUP"); break;
      case 0x02: strcpy(imsg, "TIMEOUT"); break;
      case 0x04: strcpy(imsg, "EJECT"); break;
      case 0x08: strcpy(imsg, "FLUSH"); break;
      default:   sprintf(imsg, "??0x%02x?",crb_b.icrbb_field_s.imsg); break;
      } 
      break;
    default:
      break;
    } 
    switch (crb_b.icrbb_field_s.reqtype) {
      case  0:  strcpy(reqtype, "PRDC "); break;
      case  1:  strcpy(reqtype, "BLKRD"); break;
      case  2:  strcpy(reqtype, "DXPND"); break;
      case  3:  strcpy(reqtype, "EJPND"); break;
      case  4:  strcpy(reqtype, "BSHU "); break;
      case  5:  strcpy(reqtype, "BEXU "); break;
      case  6:  strcpy(reqtype, "RDEX "); break;
      case  7:  strcpy(reqtype, "WINV "); break;
      case  8:  strcpy(reqtype, "PIORD"); break;
      case  9:  strcpy(reqtype, "PIOWR"); break; /* Warning. spec is wrong */
      case 16:  strcpy(reqtype, "WB   "); break;
      case 17:  strcpy(reqtype, "DEX  "); break;
      default:  sprintf(reqtype, "?0x%02x",crb_b.icrbb_field_s.reqtype); break; 
    } 
    switch(crb_b.icrbb_field_s.initator) {
      case 0: strcpy(initiator, "XTALK"); break;
      case 1: strcpy(initiator, "HubIO"); break;
      case 2: strcpy(initiator, "LgNet"); break;
      case 3: strcpy(initiator, "CRB  "); break;
      case 5: strcpy(initiator, "BTE_1"); break;
      default: sprintf(initiator, "??%d??", crb_b.icrbb_field_s.initator); break;
    }
    pf("%x: %x %x %x %x %x %x %x %x %02x %010lx %x %x   ",
	   ind,
	   crb_a.icrba_fields_s.stall_bte0,       /*   1  */ 
	   crb_a.icrba_fields_s.stall_bte1,       /*   1  */ 
	   crb_a.icrba_fields_s.error,       /*   1  */ 
	   crb_a.icrba_fields_s.ecode,       /*   3  */ 
	   crb_a.icrba_fields_s.lnetuce,     /*   1  */ 
	   crb_a.icrba_fields_s.mark,        /*   1  */ 
	   crb_a.icrba_fields_s.xerr,        /*   1  */ 
	   crb_a.icrba_fields_s.sidn,        /*   4  */ 
	   crb_a.icrba_fields_s.tnum,        /*   5  */ 
	   crb_a.icrba_fields_s.addr<<3,     /*  38  */ 
	   crb_a.icrba_fields_s.valid,       /*   1  */ 
	   crb_a.icrba_fields_s.iow);        /*   1  */
    pf("%x %x %s %03x %x %s %s %s %s ",
	   crb_b.icrbb_field_s.btenum,       /*   1  */ 
	   crb_b.icrbb_field_s.cohtrans,     /*   1  */ 
	   xtsize_string[crb_b.icrbb_field_s.xtsize],       /*   2  */ 
	   crb_b.icrbb_field_s.srcnode,      /*   9  */ 
	   crb_b.icrbb_field_s.useold,       /*   1  */ 
	   imsgtype,
	   imsg,       
	   initiator,
	   reqtype);
    pf("%03x %x %x %x %x %x %x %x  ",
	   crb_b.icrbb_field_s.ackcnt,       /*  11  */ 
	   crb_b.icrbb_field_s.resp,         /*   1  */ 
	   crb_b.icrbb_field_s.ack,          /*   1  */ 
	   crb_b.icrbb_field_s.hold,         /*   1  */ 
	   crb_b.icrbb_field_s.wb_pend,      /*   1  */ 
	   crb_b.icrbb_field_s.intvn,        /*   1  */ 
	   crb_b.icrbb_field_s.stall_ib,     /*   1  */ 
	   crb_b.icrbb_field_s.stall_intr);  /*   1  */
	   
    pf("%x %x %x %09x %03x %x %x %x  %x %x %04x %02x\n",
	   crb_c.icrbc_field_s.pricnt,       /*   4  */
	   crb_c.icrbc_field_s.pripsc,       /*   4  */
	   crb_c.icrbc_field_s.bteop,        /*   1  */
	   crb_c.icrbc_field_s.push_be,      /*  34  */
	   crb_c.icrbc_field_s.suppl,        /*  11  */
	   crb_c.icrbc_field_s.barrop,       /*   1  */
	   crb_c.icrbc_field_s.doresp,       /*   1  */
	   crb_c.icrbc_field_s.gbr,          /*   1  */
	   
	   crb_d.icrbd_field_s.toutvld,      /*   1  */
	   crb_d.icrbd_field_s.ctxtvld,      /*   1  */
	   crb_d.icrbd_field_s.context,      /*  15  */
	   crb_d.icrbd_field_s.timeout);     /*   8  */
  }

}


/*
 * This routine should only be called when no DMA is in progress.
 */
void
hub_merge_clean(nasid_t nasid)
{
	hubreg_t icmr, active_bits;
	int crb_num;

	icmr = REMOTE_HUB_L(nasid, IIO_ICMR);

	active_bits = (icmr & IIO_ICMR_PC_VLD_MASK) >> IIO_ICMR_PC_VLD_SHFT;
	
	for (crb_num = 0; crb_num < IIO_NUM_CRBS; crb_num++) {
		/* Test all CRBs */
		if (active_bits & (1 << crb_num)) {
			icrba_t crba;

			/*
			 * XXX - Figure out how to make the casting work so
			 * we don't have to circumvent the macros to load it.
			 */
			crba = *(icrba_t *)REMOTE_HUB_ADDR(nasid, IIO_ICRB_A(crb_num));
			*(volatile int *)PHYS_TO_K0(((crba.a_addr) << 3));
			crba = *(icrba_t *)REMOTE_HUB_ADDR(nasid, IIO_ICRB_A(crb_num));
		}
	}
}

#if defined (HUB_II_IFDR_WAR)

int hubio_ifdr_skip = 0;
int hub_ifdr_war_freq = 1;

void
hub_init_ifdr_war(cnodeid_t cnode)
{
	hubii_ifdr_t ifdr;

	nasid_t nasid = COMPACT_TO_NASID_NODEID(cnode);
	NODEPDA(cnode)->hub_ifdr_ovflw_count = 0;
	NODEPDA(cnode)->hub_ifdr_val = 0x28;
	NODEPDA(cnode)->hub_ifdr_check_count = hub_ifdr_war_freq;
	ifdr.hi_ifdr_value = REMOTE_HUB_L(nasid, IIO_IFDR);	
	if (ifdr.hi_ifdr_fields.ifdr_maxrp > NODEPDA(cnode)->hub_ifdr_val) {
		if (cnode == 0)
		    cmn_err(CE_NOTE, "HUB IFDR value not correctly set. Please upgrade your prom");
		ifdr.hi_ifdr_fields.ifdr_maxrp = NODEPDA(cnode)->hub_ifdr_val;
		/*
		 * Note: there is max_rp and max_rq. We are doing a non
		 * atomic RMW. Big chances for screw up.
		 * we should do this in the prom.
		 */
		REMOTE_HUB_S(nasid, IIO_IFDR, ifdr.hi_ifdr_value);
	}
}

int
hub_ifdr_check_value(cnodeid_t cnode)
{
	hubii_ifdr_t ifdr;
	nasid_t nasid = COMPACT_TO_NASID_NODEID(cnode);

	if (hubio_ifdr_skip)
	    return 0;

	ifdr.hi_ifdr_value = REMOTE_HUB_L(nasid, IIO_IFDR);
	if (ifdr.hi_ifdr_fields.ifdr_maxrp > NODEPDA(cnode)->hub_ifdr_val) 
	    return 1;

	return 0;
}

#endif

int wait_exceeded;
hubreg_t last_reqqd_val, last_repqd_val;



#if defined (HUB_II_IFDR_WAR)
int
hub_ifdr_fix_value(cnodeid_t cnode)
{
	hubii_ifdr_t ifdr;
	hubcore_reqqd_t crqqd;
	hubcore_repqd_t crpqd;
	nasid_t nasid = COMPACT_TO_NASID_NODEID(cnode);
	int max_wait = 100;

	ifdr.hi_ifdr_value = REMOTE_HUB_L(nasid, IIO_IFDR);

	if (ifdr.hi_ifdr_fields.ifdr_maxrp > 0x40) {
		return -1;
	}
	NODEPDA(cnode)->hub_ifdr_ovflw_count++;

	/*
	 * Make sure the CORE registers show use count of 0 for
	 * the fifo we are about to program.
	 */
	while (--max_wait) {
		crqqd.hc_reqqd_value = REMOTE_HUB_L(nasid,CORE_REQQ_DEPTH);
		crpqd.hc_repqd_value = REMOTE_HUB_L(nasid,CORE_REPQ_DEPTH);
		if ((crqqd.hc_reqqd_fields.reqqd_ioq_rq == 0) &&
		    (crpqd.hc_repqd_fields.repqd_ioq_rp == 0))
		    break;
	}
	if (!max_wait) {
		wait_exceeded++;
		last_reqqd_val = crqqd.hc_reqqd_value;
		last_repqd_val = crpqd.hc_repqd_value;
	}
	ifdr.hi_ifdr_value = REMOTE_HUB_L(nasid, IIO_IFDR);
	ifdr.hi_ifdr_fields.ifdr_maxrp = NODEPDA(cnode)->hub_ifdr_val;
	/*
	 * Note: there is max_rp and max_rq. We are doing a non
	 * atomic RMW. Big chances for screw up if IO not
	 * quiesced.
	 */
	REMOTE_HUB_S(nasid, IIO_IFDR, ifdr.hi_ifdr_value);

	return 1;
}

#endif /* HUBII_IFDR_WAR */


#if defined (HUB_II_IFDR_WAR)

lock_t	global_kick_lock;
lock_t	kick_mask_lock;

void
hub_setup_kick(cnodeid_t cnode)
{
	if (cnode == 0) {
		spinlock_init(&global_kick_lock, "global_kick_lock");
		spinlock_init(&kick_mask_lock, "kick_mask_lock");
	}

	spinlock_init(&(NODEPDA(cnode)->hub_dmatimeout_kick_lock), "dma_kick_lock");

	/* For now, say we have IO that needs kicking. */
	NODEPDA(cnode)->hub_dmatimeout_need_kick = 1;

	/* Say that we're ready for a kick now. */
	NODEPDA(cnode)->hub_dmatimeout_kick_count = 0;

}



/*
 * hub_dmatimeout_kick
 *	Hub 1 has a bug in CRB timeout, where the timeout counts upto
 * 	max and then stops. So, all further transactions in the CRB
 *	are marked as error, since  the timeout is already over.
 *	This routine should be called by all drivers that wish to 
 *	do DMA, just prior to doing their DMA. This routine then goes
 *	and fixes the timeout such that driver DMA transactions are 
 *	not flagged as timeout error by the HUB CRB.
 *	Hopefully this is only a bug in HUB1
 */

int xwidget_disable_dma(vertex_hdl_t);
int xwidget_reenable_dma(vertex_hdl_t);

#define	PCI_FREEZE_GNT	(1 << 6)

volatile cpumask_t entermask;
volatile int release;
volatile cpuid_t kicker;
volatile cpuid_t kicking;
extern int graph_done;

void
kick_action(void)
{
	int s;
	extern void chkdebug(void);

	bte_wait_for_xfer_completion(private.p_bte_info);

	/* Tell the master we're here. */
	s = mutex_spinlock(&kick_mask_lock);
	CPUMASK_CLRB(entermask, cpuid());
	mutex_spinunlock(&kick_mask_lock, s);

	/* Wait to be released */
	while (!release)
		chkdebug();
}


int
hub_dmatimeout_kick(cnodeid_t cnode, int spin)
{
	nasid_t		nasid;
	volatile __uint32_t  wrf;
	int		i, j;
	bridge_t	*bridgep;
	int		s, t;
	/*REFERENCED*/
	hubreg_t	kick_icmr;
	/*REFERENCED*/
	hubreg_t	before;
	/*REFERENCED*/
	hubreg_t	after;
	static int	first = 1;
	__int64_t	timeout_time;
	hubreg_t	old_val;

	volatile unsigned int hub_timeout;
	int	fatal = 0;

	nasid = COMPACT_TO_NASID_NODEID(cnode);

	if (spin) {
		/* Called from BTE code. */
		t = mutex_spinlock(&global_kick_lock);
	} else {
		/* Called from clock code */
		if (!(t = mutex_spintrylock(&global_kick_lock)))
			return 1;
	}

	/* We're now at splhi. */

	if (first && !graph_done && (cnode == 0)) {
		int wid = WIDGETID_GET(KL_CONFIG_CH_CONS_INFO(master_nasid)->memory_base);
		NODEPDA(cnode)->bridge_base[wid] = NODE_SWIN_BASE(nasid, wid);

		first = 0;
	}

	release = 0;

	kicker = cpuid();

	CPUMASK_CLRALL(entermask);

	for (i = 0; i < maxcpus; i++) {
		if ((i != cpuid()) && (pdaindr[i].CpuId != -1))
			CPUMASK_SETB(entermask, i);
	}

	for (i = 0; i < maxcpus; i++) {
		if (CPUMASK_TSTB(entermask, i)) {
			kicking = i;
			cpuaction(i, (cpuacfunc_t)kick_action, A_NOW);
		}
	}

	kicking = -1;

	bte_wait_for_xfer_completion(private.p_bte_info);

	/* 30 second timeout */
	timeout_time = absolute_rtc_current_time() + (30 * USEC_PER_SEC);

	while (CPUMASK_IS_NONZERO(entermask) && absolute_rtc_current_time() < timeout_time)
		;

	if (absolute_rtc_current_time() >= timeout_time) {
		release = 1;
		for (i = 0; i < maxcpus; i++) {
			if (CPUMASK_TSTB(entermask, i))
			    cmn_err(CE_NOTE, "CPU %d didn't arrive in kick_action.", i);
		}
		cmn_err(CE_PANIC, "Timeout/IFDR kick failed.  CPUs didn't all arrive.");
	}

	s = splerr();
	before = GET_LOCAL_RTC;
	/*
	 * Action time..
	 *
	 * First stop bridge from doing any further DMAs or interrupts.
	 */
	for (i = 0; i < 16; i++) {
		if (bridgep = (bridge_t *)NODEPDA(cnode)->bridge_base[i]) {
			NODEPDA(cnode)->b_int_enable[i] = 
			    bridgep->b_int_enable;
			NODEPDA(cnode)->b_arb[i] = bridgep->b_arb;
			bridgep->b_int_enable = 0;
			bridgep->b_arb =
			    PCI_FREEZE_GNT | NODEPDA(cnode)->b_arb[i];

			for (j = 0; j < 8; j++)
			    wrf = bridgep->b_wr_req_buf[j].reg;
		}
	}

	us_delay(5);

	hub_merge_clean(nasid);

	us_delay(2);

	kick_icmr = REMOTE_HUB_L(nasid, IIO_ICMR);

#if defined (HUB_II_IFDR_WAR)
	old_val = REMOTE_HUB_L(nasid, IIO_IFDR);
	fatal = hub_ifdr_fix_value(cnode);
#endif

	for (i = 0; i < 16; i++) {
		bridgep = (bridge_t *)NODEPDA(cnode)->bridge_base[i];
		if (bridgep) {
			bridgep->b_arb = (bridgereg_t)NODEPDA(cnode)->b_arb[i];
			bridgep->b_int_enable = 
			    (bridgereg_t)NODEPDA(cnode)->b_int_enable[i];
		}
	}

	after = GET_LOCAL_RTC;

	release = 1;
	kicker = -1;

	us_delay(2);

	splx(s);

	mutex_spinunlock(&global_kick_lock, t);

#if defined (HUB_II_IFDR_WAR)
	if (fatal > 0) {
		cmn_err(CE_NOTE|CE_PHYSID, 
			"HUB II_IFDR (0x%x) fixed. Value was 0x%x",
			REMOTE_HUB_L(nasid, IIO_IFDR), old_val);
	}
	else if (fatal < 0) {
		cmn_err(CE_PANIC|CE_PHYSID, 
			"HUB II_IFDR (0x%x) overflowed. Fatal",
			REMOTE_HUB_L(nasid, IIO_IFDR));
	}
#endif	

#if DEBUG
	if (kick_icmr & IIO_ICMR_CRB_VLD_MASK) {
		global_kick_icmr = kick_icmr;
		LOCAL_HUB_S(PI_CPU_NUM, 10);
		LOCAL_HUB_S(PI_CPU_NUM, after - before);
		LOCAL_HUB_S(PI_CPU_NUM, kick_icmr);
		LOCAL_HUB_S(PI_CPU_NUM, kick_icmr & IIO_ICMR_CRB_VLD_MASK);
while (1);
/* NOTREACHED */
	}
	ASSERT(!(kick_icmr & IIO_ICMR_CRB_VLD_MASK));

#endif
	return 0; /* Success */

}


#endif /* HUB_II_IFDR_WAR */


extern void hub_setup_prb(nasid_t, int, int, int);

void
hub_widget_reset(vertex_hdl_t hubv, xwidgetnum_t widget)
{
	cnodeid_t cnode;
	nasid_t nasid;
	__psunsigned_t addr;
	iprte_a_t	iprte;		/* PIO read table entry format */
	int i, s;

	cnode = master_node_get(hubv);
	if (cnode == CNODEID_NONE) return;
	nasid = COMPACT_TO_NASID_NODEID(cnode);

	/*
	 * Disable interrupts. Do we need a lock here?
	 */
	s = splhi();

	for (i = 0; i < IIO_NUM_PRTES; i++) {
	    iprte.entry = REMOTE_HUB_L(nasid, IIO_PRTE(i));
	    if (iprte.iprte_fields.valid) {
		addr = (iprte.iprte_fields.addr << 3);
		if (WIDGETID_GET(addr) == widget) {
		    REMOTE_HUB_S(nasid, IIO_PRTE(i), 0);
		    REMOTE_HUB_S(nasid, IIO_IPDR, (i | IIO_IPDR_PND));
		    ASSERT_ALWAYS(! (REMOTE_HUB_L(nasid, IIO_IPDR) & 
				     IIO_IPDR_PND));
		}
	    }		
	}
	hub_setup_prb(nasid, widget, 3, HUB_PIO_FIRE_N_FORGET);
	
	splx(s);
}
