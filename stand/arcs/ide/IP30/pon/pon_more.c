#ident	"ide/IP30/pon/pon_more.c: $Revision: 1.33 $"

#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/xtalk/xwidget.h>
#include <uif.h>
#include <pon.h>
#include <gfxgui.h>
#include <libsk.h>
#include <libsc.h>

static int
pon_dimms(int testonly)
{
	int		bank;
	unsigned int	bankerr;
	int		errcount = 0;
	heartreg_t	mem_gap = 0;

	for (bank = 0; bank < 4; bank++) {
		if (HEART_PIU_K1PTR->h_memcfg.bank[bank] & HEART_MEMCFG_VLD)
			mem_gap |= 0x1 << bank;

		bankerr = (simmerrs >> bank * 2) & BAD_ALLDIMMS;

		/* won't catch the case where both DIMMs in a bank are bad */
		if (bankerr == BAD_ALLDIMMS || bankerr == 0)
			continue;

		errcount++;
		if (!testonly)
			msg_printf(LOG,"\tCheck or replace:  DIMM S%d\n",
				bank * 2 + (2 - bankerr) + 1);
	}

	/* this won't catch the case where the last N banks of DIMM are bad */
	if (mem_gap & mem_gap + 1) {
		errcount++;
		if (!testonly)
			msg_printf(LOG,
				"\tMemory should be installed contiguously "
				"starting with DIMM socket S1\n");
	}
	
	return errcount;
}



int
pon_morediags(void)
{
	static char *ip30bad="\n\tCheck or replace:  System board (IP30)\n";
	static char *scsict="SCSI %s controller diagnostic   *FAILED*\n";
	static char *scsidc="%sSCSI device/cable diagnostic      *FAILED*\n";
	char *cp;
	char diagmode = (cp = getenv("diagmode")) ? *cp : 0;
	int failflags = 0, scsirc = 0;
	int scsirc0, scsirc1;
	int ontty;
	static char *ponmsg = "Running power-on diagnostics...";
	int port;
	xbow_t *xbow = XBOW_K1PTR;

	/* indicate that system is executing diags by turning on the amber */
	ip30_set_cpuled(LED_AMBER);

	/*
	 * if console is set to the terminal, try to get the message printed
	 * quickly since pon_graphics takes a long time.
	 */
	cp = getenv("console");
	if (ontty = (cp && *cp == 'd'))
		pongui_setup(ponmsg, 0);

	*Reportlevel = NO_REPORT;

#ifdef PROM
	failflags |= pon_graphics();
#endif /* PROM */

	/* If graphics diagnostic fails, then give an immediate audio
	 * indication of failure.
	 */
	if (failflags & (FAIL_BADGRAPHICS | FAIL_ILLEGALGFXCFG)) {
	    play_hello_tune(2);
	    wait_hello_tune();
	}

	*Reportlevel = diagmode == 'v' ? VRB : NO_REPORT;

	if (!ontty)
		pongui_setup(ponmsg, 1);	/* prints power on message */

	if (pon_dimms(1)) 
		failflags |= FAIL_MEMORY;

#if !defined(MFG_DBGPROM)
	/* The damn nics are not programmed on most P1 systems */
	if (getenv("nicok") == 0)
		failflags |= pon_nic();

	if (scsirc0 = pon_scsi(0)) {
		scsirc |= 1;
		if (scsirc0 & 0xffff)
			failflags |= FAIL_SCSIDEV;
		if (scsirc0 & 0x100000)
			failflags |= FAIL_SCSICTRLR;
	}

	if (scsirc1 = pon_scsi(1)) {
		scsirc |= 2;
		if (scsirc1 & 0xffff)
                        failflags |= FAIL_SCSIDEV;
		if (scsirc1 & 0x100000)
			failflags |= FAIL_SCSICTRLR;
	}

	/* If both controllers are ok and both disks have no scsi devices
	 * (only set if !diskless), callout a cabling problem.
	 */
	if ((failflags & FAIL_SCSICTRLR) == 0) {
		if ((scsirc0 & 0x10000) && (scsirc1 & 0x10000))
			failflags |= FAIL_SCSIDEV;
	}

	if (pon_enet())
		failflags |= FAIL_ENET;

	/* set pass/fail for keyboard/mouse */
	failflags |= pon_ioc3_pckm(); 
#endif	/* !MFG_DBGPROM */

	/* check all widget links */
	for (port = BRIDGE_ID; port >= HEART_ID; port--) {
		xbowreg_t	link_aux_status;

		/* clear link status too */
		link_aux_status = xbow->xb_link(port).link_aux_status;
		xbow->xb_link(port).link_status_clr;

		/* if the code gets to here, HEART and BRIDGE should be ok */
		if (port == BRIDGE_ID || port == HEART_ID) {
			((widget_cfg_t *)K1_MAIN_WIDGET(port))->w_control |=
				WIDGET_CLR_RLLP_CNT | WIDGET_CLR_TLLP_CNT;
			continue;
		}

		if (!(link_aux_status & XB_AUX_STAT_PRESENT))	/* no widget */
			continue;

		if (xtalk_probe(xbow, port))
			continue;

		failflags |= FAIL_WIDGET;
		if (link_aux_status & XB_AUX_LINKFAIL_RST_BAD)
			msg_printf(LOG,"XIO Slot Reset Failure                   *FAILED*\n\n");
		else
			msg_printf(LOG,"XIO Slot Link Failure                    *FAILED*\n\n");
		msg_printf(0,"\n\tCheck or replace:  XIO device %d\n", port);
	}

	/* print messages corresponding to failed diagnostics */
	if (failflags) {
		msg_printf(0,"\n");

		if (failflags & FAIL_MEMORY) {
			msg_printf(LOG,"Memory diagnostic                          *FAILED*\n\n");
			pon_dimms(0);
		}
		if (failflags & FAIL_BADGRAPHICS) {
			msg_printf(LOG,"Graphics diagnostic                        *FAILED*\n");
			msg_printf(0,"\n\tCheck or replace:  Graphics board\n");
		}
		if (failflags & FAIL_ILLEGALGFXCFG) {
			msg_printf(LOG,"Graphics diagnostic                        *FAILED*\n");
			msg_printf(0,"\n\tMXx/SSx graphics option must "
				     "be installed in XIO slots A and D,\n"
				     "\ttowards the interior of the "
				     "workstation.\n");
		}
		if (failflags & FAIL_CACHES) {
			msg_printf(LOG,"Cache diagnostic                           *FAILED*\n");
			msg_printf(0,"\n\tCheck or replace:  CPU module\n");
		}
		if (failflags & FAIL_NIC_CPUBOARD) {
			msg_printf(LOG,"CPU board NIC diagnostic                   *FAILED*\n");
			msg_printf(0,"\n\tCheck or replace:  CPU module\n");
		}
		if (failflags & FAIL_NIC_BASEBOARD) {
			msg_printf(LOG,"System board NIC diagnostic                *FAILED*\n");
			msg_printf(0,ip30bad);
		}
		if (failflags & FAIL_NIC_FRONTPLANE) {
			msg_printf(LOG,"Front plane NIC diagnostic                 *FAILED*\n");
			msg_printf(0,"\n\tCheck or replace:  Front plane\n");
		}
		if (failflags & FAIL_NIC_PWR_SUPPLY) {
			msg_printf(LOG,"Power supply NIC diagnostic                *FAILED*\n");
			msg_printf(0,"\n\tCheck:  Power supply\n");
		}
		if (failflags & FAIL_NIC_EADDR) {
			msg_printf(LOG,"System ID NIC diagnostic                   *FAILED*\n");
			msg_printf(0,"\n\tCheck or replace:  System ID NIC\n");
		}
		if (failflags & FAIL_PCCTRL) {
                	msg_printf(LOG,"Keyboard/mouse controller diagnostic       *FAILED*\n");
			msg_printf(0,ip30bad);
        	}
	        if (failflags & FAIL_PCKBD) {
                	msg_printf(LOG,"Keyboard diagnostic                        *FAILED*\n");
                	msg_printf(0,"\n\tCheck or replace: keyboard and cable\n");
        	}
        	if (failflags & FAIL_PCMS) {
                	msg_printf(LOG,"Mouse diagnostic                           *FAILED*\n");
                	msg_printf(0,"\n\tCheck or replace: mouse and cable\n");
        	}

		if (failflags & FAIL_SCSICTRLR) {
			if (scsirc & 1) msg_printf(LOG,scsict,"Internal (0)");
			if (scsirc & 2) msg_printf(LOG,scsict,"External (1)");
			msg_printf(0,ip30bad);
		}
		if (failflags & FAIL_SCSIDEV) {
			if (scsirc & 1) msg_printf(LOG,scsidc, "Internal ");
			if (scsirc & 2) msg_printf(LOG,scsidc, "External ");
			msg_printf(0,"\n\tCheck or replace:  Disk, Floppy, CDROM, or SCSI Cable\n");
		}
		if (failflags & FAIL_ENET) {
			msg_printf(LOG,"Ethernet diagnostic     	           *FAILED*\n");
			msg_printf(0,ip30bad);
		}
		
		msg_printf(0,"\n");
	}

	if (ontty)
		failflags &= ~FAIL_NOGRAPHICS;

	/* set led color according to test results */
	ip30_set_cpuled(failflags ? LED_AMBER : LED_GREEN);

	failflags &= ~FAIL_NOGRAPHICS;

	pongui_cleanup();

	return failflags;
}
