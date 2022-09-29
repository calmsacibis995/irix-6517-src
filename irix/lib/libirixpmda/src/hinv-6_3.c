/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986,1988, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.1 $"

/*
 * Hardware inventory command
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <invent.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sgigsc.h>
#include <net/raw.h>
#include <net/if.h>
#include <sys/fddi.h>
#include <sys/if_xpi.h>
#include <sys/if_ep.h>
#include <sys/sbd.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evmp.h>
#include <sys/syssgi.h>

/*
 * Include the diagnostic value strings.
 */
#include <sys/EVEREST/diagval_strs.i>

#define MAXTABLESIZE 4096
#define ALL (-1)
/* 
 * make sure backward compatible (char to long change); we may need to expand this 
 * when we start to support more 255 cpus
 */
#define ALL_8BIT (ALL & 0xFF) 

static int sflag;
static int verbose = 0;
static int new_mem = 0;

static int ngfxbds;

static char *argv0;

static evcfginfo_t evcfg;
static int cpuid_to_slot[EV_MAX_CPUS];
static int last_slot = -1;
static int first_loop = 1;

/*
 * Class name mnemonics for -c option:
 * This list is order dependent on the class name definitions in invent.h
 * When adding a new inventory class, add an entry to this table.
 */
char *classes[] = {
	"processor",
	"disk",
	"memory",
	"serial",
	"parallel",
	"tape",
	"graphics",
	"network",
	"scsi",
	"audio",
	"iobd",
	"video",
	"bus",
	"misc",
	"compression",
	"vscsi",
	"display",
	"unconnected scsi",
	"pci",
	"pci no driver",
	0
};


struct  {
	char *mn;
	int class;
	int type;
} searchtypes[] = {
	"fpu",		INV_PROCESSOR,	INV_FPUCHIP,
	"cpu",		INV_PROCESSOR,	INV_CPUCHIP,
	"dcache",	INV_MEMORY,	INV_DCACHE,
	"icache",	INV_MEMORY,	INV_ICACHE,
	"memory",	INV_MEMORY,	INV_MAIN_MB,
	"qic",		INV_TAPE,	INV_SCSIQIC,
	"a2",		INV_AUDIO,	INV_AUDIO_A2,
	"dsp",		INV_AUDIO,	INV_AUDIO_HDSP,
	0,		0,		0
};

struct  {
	char *mn;
	int class;
	int type;
	int cont;
} searchdevs[] = {
	"cdsio",	INV_SERIAL,	INV_CDSIO,	ALL,
	"aso",		INV_SERIAL,	INV_ASO_SERIAL,	ALL,

	"ec",		INV_NETWORK,	INV_NET_ETHER,	INV_ETHER_EC,
	"et",		INV_NETWORK,	INV_NET_ETHER,	INV_ETHER_ET,
	"ee", 		INV_NETWORK,	INV_NET_ETHER,	INV_ETHER_EE,
	"enp",		INV_NETWORK,	INV_NET_ETHER,	INV_ETHER_ENP,
	"fxp",		INV_NETWORK,	INV_NET_ETHER,	INV_ETHER_FXP,
	"ep",		INV_NETWORK,	INV_NET_ETHER,	INV_ETHER_EP,
	
	"hy",		INV_NETWORK,	INV_NET_HYPER,	INV_HYPER_HY,
	
	"ipg",		INV_NETWORK,	INV_NET_FDDI,	INV_FDDI_IPG,
	"rns",		INV_NETWORK,	INV_NET_FDDI,	INV_FDDI_RNS,
	"xpi",		INV_NETWORK,	INV_NET_FDDI,	INV_FDDI_XPI,
	"fv",		INV_NETWORK,	INV_NET_TOKEN,	INV_TOKEN_FV,
	"gtr",		INV_NETWORK,	INV_NET_TOKEN,	INV_TOKEN_GTR,
	"mtr",		INV_NETWORK,	INV_NET_TOKEN,	INV_TOKEN_MTR,

	"atm",		INV_NETWORK,	INV_NET_ATM,	INV_ATM_GIO64,

	0,		0,		0
};

struct search_args {
	int	class;
	int	type;
	int	cont;
	int	unit;
};


static int dkippmode(inventory_t *inv);
static void cpu_display(unsigned int rev);
static void fpu_display(unsigned int rev);

static void get_extended_info();
static void display_all_cpus();
static void display_cpubrd(evbrdinfo_t *, int);
static void membrd_display_extended(int);


/*
 * Display hardware inventory item
 *
 * This procedure knows how to display every kind of inventory item.
 * New types of items should be added here.
 *
 */
/* ARGSUSED */
int
display_item(inventory_t *pinvent, void *ptr)
{
	int options, cpunum;
	struct search_args *sa = ptr;
	int i;

	switch(pinvent->inv_class) {
	case INV_MEMORY:
		switch(pinvent->inv_type) {
		case INV_MAIN:
			if (!new_mem) {
				printf("Main memory size: %ld Mbytes\n",
					((pinvent->inv_state+0x1000)/0x100000));
			}
			break;
		case INV_MAIN_MB:
			new_mem = 1;
			printf("Main memory size: %ld Mbytes",
				pinvent->inv_state);
			if (pinvent->inv_unit)
				printf(", %d-way interleaved\n", pinvent->inv_unit);
			else
				printf("\n");
			/* hack to display additional memory info */
			if (verbose) {
				int slot;
				for (slot = 0; slot < EV_MAX_SLOTS; slot++)
					membrd_display_extended(slot);
			}
			break;
		case INV_DCACHE:
			printf("Data cache size: %ld Kbytes\n",
				pinvent->inv_state/1024);
			break;
		case INV_ICACHE:
			printf("Instruction cache size: %ld Kbytes\n",
				pinvent->inv_state/1024);
			break;
		case INV_SDCACHE:
			if (pinvent->inv_state < 1024*1024) {
				printf("Secondary data cache size: %ld Kbytes\n",
					pinvent->inv_state/1024);
			} else if (pinvent->inv_state == 1024*1024) {
				printf("Secondary data cache size: 1 Mbyte\n");
			} else {
				printf("Secondary data cache size: %ld Mbytes\n",
					pinvent->inv_state/(1024*1024));
			}
			break;
		case INV_SICACHE:
			if (pinvent->inv_state < 1024*1024) {
				printf("Secondary instruction cache size: %ld Kbytes\n",
					pinvent->inv_state/1024);
			} else if (pinvent->inv_state == 1024*1024) {
				printf("Secondary instruction cache size: 1 Mbyte\n");
			} else {
				printf("Secondary instruction cache size: %ld Mbytes\n",
					pinvent->inv_state/(1024*1024));
			}
			break;
		case INV_SIDCACHE:
			/*
			 * -1 in unit means all cpus have same sized 
			 * secondary cache - support for mixed caches
			 * on EVERESTs.
			 * If there is only one record for SIDCACHE, it means
			 * everyone is the same. This is for backward compatibility.	
			 */
			cpunum = -1;
			if (pinvent->inv_next &&
			    (pinvent->inv_unit & ALL_8BIT) != ALL_8BIT)
				cpunum = pinvent->inv_controller;
			if (pinvent->inv_state < 1024*1024) {
				printf("Secondary unified instruction/data cache size: %ld Kbytes",
					pinvent->inv_state/1024);
			} else if (pinvent->inv_state == 1024*1024) {
				printf("Secondary unified instruction/data cache size: 1 Mbyte");
			} else {
				printf("Secondary unified instruction/data cache size: %ld Mbytes",
					pinvent->inv_state/(1024*1024));
			}
			if (cpunum == -1)
				printf("\n");
			else
				printf(" on Processor %d\n", cpunum);
			break;
		case INV_PROM:
			printf("FLASH PROM version %d.%d\n", 
				pinvent->inv_controller, pinvent->inv_unit);
			break;
		}
		break;
	case INV_PROCESSOR:
		switch(pinvent->inv_type) {
		/*
		 * CPU/FPU revisions are stored in a compacted
		 * form.
		 */
		case INV_CPUCHIP:
			cpu_display((unsigned int) pinvent->inv_state);
			break;
		case INV_FPUCHIP:
			if ( pinvent->inv_state )
				fpu_display(
					(unsigned int)pinvent->inv_state);
			else
				printf("No hardware floating point.\n");
			break;
		case INV_CPUBOARD: {
			/*
			 * HACK to work around the weakness of inventory facility 
			 * which soly depends logical id. The hack here to insert
			 * the entries for disabled cpus.
			 */
			if (verbose && (pinvent->inv_state == INV_IP19BOARD ||
			    pinvent->inv_state == INV_IP21BOARD ||
			    pinvent->inv_state == INV_IP25BOARD )) {

				if (!first_loop)
					return (0);

				if ((pinvent->inv_unit & ALL_8BIT) == ALL_8BIT) {
					display_all_cpus();
					first_loop = 0;
				} 
				else {
					evcfginfo_t *ep = &evcfg;
					evbrdinfo_t *brd;
					int slot = cpuid_to_slot[pinvent->inv_unit];

					brd = &(ep->ecfg_board[slot]);
	
					if (brd->eb_type != EVTYPE_IP19 && 
					    brd->eb_type != EVTYPE_IP21 &&
					    brd->eb_type != EVTYPE_IP25)
						return (0);

					if (last_slot != slot)
						printf("CPU Board at Slot %d: (%s)\n", 
							slot, (brd->eb_enabled ? 
							"Enabled" : "Disabled"));
	
					last_slot = slot;

					if (!brd->eb_enabled)
						return (0);

					display_cpubrd(brd, pinvent->inv_unit);
				}
				return (0);
			}

			/* ALL_8BIT means, all cpu has same speed */
			if ((pinvent->inv_unit & ALL_8BIT) == ALL_8BIT) {
				cpunum = sysmp(MP_NPROCS);
				printf("%d ",cpunum);
			}
			else
				printf("Processor %d: ", pinvent->inv_unit);

			if ( pinvent->inv_controller != 0 )
				printf("%d MHZ ",pinvent->inv_controller);
			switch (pinvent->inv_state) {
				case INV_R2300BOARD:
					printf("R2300 ");
					break;
				case INV_IP4BOARD:
					printf("IP4 ");
					break;
				case INV_IP5BOARD:
					printf("IP5 ");
					break;
				case INV_IP6BOARD:
					printf("IP6 ");
					break;
				case INV_IP7BOARD:
					printf("IP7 ");
					break;
				case INV_IP9BOARD:
					printf("IP9 ");
					break;
				case INV_IP12BOARD:
					printf("IP12 ");
					break;
				case INV_IP17BOARD:
					printf("IP17 ");
					break;
				case INV_IP19BOARD:
					printf("IP19 ");
					break;
				case INV_IP20BOARD:
					printf("IP20 ");
					break;
				case INV_IP21BOARD:
					printf("IP21 ");
					break;
				case INV_IP22BOARD:
					printf("IP22 ");
					break;
				case INV_IP25BOARD:
					printf("IP25 ");
					break;
				case INV_IP26BOARD:
					printf("IP26 ");
					break;
				case INV_IP28BOARD:
					printf("IP28 ");
					break;
				case INV_IP30BOARD:
					printf("IP30 ");
					break;
				case INV_IP32BOARD:
					printf("IP32 ");
					break;
				}
			}
			if ((pinvent->inv_unit & ALL_8BIT) == ALL_8BIT) {
				if ( cpunum > 1 )
					printf("Processors");
				else
					printf("Processor");
			}
			printf("\n");	

			break;
		case INV_CCSYNC:
			if ((sa == NULL) || (sa->class == ALL))
				printf("CC synchronization join counter\n");
			break;
		}
		break;

	case INV_IOBD:
		switch(pinvent->inv_type) {
		case INV_CLOVER2:
			printf("I/O board, ");
			switch(pinvent->inv_unit) {
			case 0:
				printf("slot F: ");
				break;
			case 1:
				printf("slot E: ");
				break;
			}
			switch(pinvent->inv_state) {
			case INV_IO2_REV1:
				printf("IO2 revision 1\n");
				break;
			case INV_IO2_REV2:
				printf("IO2 revision 2\n");
				break;
			case INV_IO3_REV1:
				printf("IO3\n");
				break;
			case INV_IO3_REV2:
				printf("IO3B\n");
				break;
			}
			break;
		
		case INV_EVIO:
			printf("I/O board, Ebus slot %d: ", 
			       pinvent->inv_unit);
			switch (pinvent->inv_state) {
			case INV_IO4_REV1:
				printf("IO4 revision 1\n");
				break;
			default:
				printf("IO4 of unknown revision\n");
			}
			break;
		}
		break;

	case INV_DISK:
		switch(pinvent->inv_type) {
		case INV_DKIPCONTROL:
			printf("Unrecognized Interphase disk controller %d\n",
				pinvent->inv_controller);
			break;

		case INV_DKIP3200:
			printf("Interphase 3200 2-drive SMD disk controller %d: ",
				pinvent->inv_controller);
			dkippmode(pinvent);
			break;

		case INV_DKIP3201:
			printf("Interphase 3201 2-drive ESDI disk controller %d: ",
				pinvent->inv_controller);
			dkippmode(pinvent);
			break;

		case INV_DKIP4200:
			printf("Interphase 4200 2-drive SMD disk controller %d: ",
				pinvent->inv_controller);
			dkippmode(pinvent);
			break;

		case INV_DKIP4400:
			printf("Interphase 4400 4-drive SMD disk controller %d: ",
				pinvent->inv_controller);
			dkippmode(pinvent);
			break;

		case INV_DKIP4201:
			printf("Interphase 4201 4-drive ESDI disk controller %d: ",
				pinvent->inv_controller);
			dkippmode(pinvent);
			break;

		case INV_DKIP4460:
			printf("Interphase 4460 16-drive IPI disk controller %d: ",
				pinvent->inv_controller);
			dkippmode(pinvent);
			break;

		case INV_JAGUAR:
			printf("Interphase 4210 VME-SCSI controller %d: ",
				pinvent->inv_controller);
			printf("Firmware revision %c%c%c\n",
			       pinvent->inv_state >> 16, pinvent->inv_state >> 8,
			       pinvent->inv_state);
			break;

		case INV_XYL714:
			printf("Xylogics 714 4-drive ESDI disk controller %d\n",
				pinvent->inv_controller);
			break;

		case INV_XYL754:
			printf("Xylogics 754 4-drive SMD disk controller %d",
				pinvent->inv_controller);
			if (pinvent->inv_state != 0) {
				/* state: release:16, revision:8, sub_rev:8 */
				printf(": Firmware version %ld.%ld.%ld",
					(pinvent->inv_state >> 16) & 0xffff,
					(pinvent->inv_state >> 8) & 0xff,
					pinvent->inv_state & 0xff);
			}
			printf("\n");
			break;

		case INV_XYL7800:
			printf("Xylogics 7800 16-drive IPI disk controller %d",
				pinvent->inv_controller);
			if (pinvent->inv_state != 0) {
				/* state: release:16, revision:8, sub_rev:8 */
				printf(": Firmware version %lx.%lx.%lx",
					pinvent->inv_state >> 16 & 0xffff,
					pinvent->inv_state >> 8 & 0xff,
					pinvent->inv_state & 0xff);
			}
			printf("\n");
			break;

		case INV_SCSICONTROL:
		case INV_GIO_SCSICONTROL:
		case INV_PCI_SCSICONTROL:
			if (pinvent->inv_type == INV_SCSICONTROL)
				printf("Integral SCSI controller %d: Version ",
					pinvent->inv_controller);
			else if (pinvent->inv_type == INV_GIO_SCSICONTROL)
				printf("GIO SCSI controller %d: Version ",
					pinvent->inv_controller);
			else
				printf("PCI SCSI controller %d: Version ",
				       pinvent->inv_controller);

			switch(pinvent->inv_state)
			{
			case INV_WD93:
				printf("WD33C93"); break;
			case INV_WD93A:
				printf("WD33C93A"); break;
			case INV_WD93B:
				printf("WD33C93B"); break;
			case INV_WD95A:
				printf("WD33C95A"); break;
			case INV_SCIP95:
				printf("SCIP/WD33C95A"); break;
			case INV_ADP7880:
				printf("ADAPTEC 7880"); break;
			default:
				printf("unknown");
			}
			switch(pinvent->inv_state)
			{
			case INV_WD93:
			case INV_WD93A:
			case INV_WD93B:
				if (pinvent->inv_unit)
					printf(", revision %X", pinvent->inv_unit);
				break;

			case INV_WD95A:
			case INV_SCIP95:
				if (pinvent->inv_unit & 0x80)
					printf(", differential");
				else
					printf(", single ended");
				if (pinvent->inv_state == INV_WD95A)
					printf(", revision %X",
					       pinvent->inv_unit & 0x7F);
			}
			putchar('\n');
			break;

		/*
		 * In order to be able to print drive type, the disk
		 * drivers pass controller type in the state field
		 * of the drive entries.
		 */
		case INV_DKIPDRIVE:
			switch(pinvent->inv_state) {
			case INV_DKIP3200:
			case INV_DKIP4200:
			case INV_DKIP4400:
				printf("SMD"); break;

			case INV_DKIP3201:
			case INV_DKIP4201:
				printf("ESDI"); break;

			case INV_DKIP4460:
				printf("IPI"); break;

			default:
				printf("Unknown"); break;
			}

			printf("    Disk drive: unit %d on Interphase controller %d\n",
			       pinvent->inv_unit, pinvent->inv_controller);
			break;

		case INV_XYLDRIVE:
			switch(pinvent->inv_state) {
			case INV_XYL754:
				printf("SMD"); break;

			case INV_XYL714:
				printf("ESDI"); break;

			case INV_XYL7800:
				printf("IPI"); break;

			default:
				printf("Unknown"); break;
			};
			printf(" Disk drive: unit %d on Xylogics controller %d\n",
				pinvent->inv_unit, pinvent->inv_controller);
			break;

		case INV_SCSIDRIVE:
			printf("  %s: unit %d", (pinvent->inv_state & INV_RAID5_LUN) ?
			       "RAID lun" : "Disk drive", pinvent->inv_unit);
			if (pinvent->inv_state)
				printf(", lun %ld", pinvent->inv_state & 0xFF);
			printf(" on SCSI controller %d\n",
				pinvent->inv_controller);
			break;

		case INV_SCSIFLOPPY: /* floppies, WORMS, CDROMs, etc. */
			printf("    Disk drive / removable media: unit %d ",
				pinvent->inv_unit);
			printf("on SCSI controller %d",
				pinvent->inv_controller);
			if(pinvent->inv_state & INV_TEAC_FLOPPY)
				printf(": 720K/1.44M floppy\n");
			else
				printf("\n");
			break;

		case INV_VSCSIDRIVE:
			printf("  Disk drive: unit %d on VME-SCSI controller %d\n",
				pinvent->inv_unit, pinvent->inv_controller);
			break;

		case INV_SCSIRAID:
			printf("  Disk drive: unit %d", pinvent->inv_unit);
			printf(" on SCSI controller %d: RAID\n",
				pinvent->inv_controller);
			break;

                case INV_PCCARD:
                        printf("PC Card: unit %d", pinvent->inv_unit);
                        if (pinvent->inv_state)
                           printf(", lun %ld",(pinvent->inv_state >> 8) & 0xff);
                        printf(" on SCSI controller %d\n",
                                pinvent->inv_controller);
                        break;
		}
		break;

	case INV_SCSI:	/* SCSI devices other than disks and tapes */
	case INV_VSCSI:
		printf("  ");
		switch(pinvent->inv_type) {
		case INV_PRINTER:	/* SCSI printer */
			printf("Printer");
			break;
		case INV_CPU:	/* SCSI CPU device */
			printf("Processor");
			break;
		case INV_WORM:	/* write once read-many */
			printf("WORM");
			break;
		case INV_CDROM:	/* CD-ROM  */
			printf("CDROM");
			break;
		case INV_SCANNER:	/* scanner, like Ricoh IS-11 */
			printf("Scanner");
			break;
		case INV_OPTICAL:	/* read-write optical disks */
			printf("Optical disk");
			break;
		case INV_CHANGER:	/* CDROM jukeboxes */
			printf("Jukebox");
			break;
		case INV_COMM:		/* Communications device */
			printf("Comm device");
			break;
		case INV_RAIDCTLR:	/* RAID controller */
			printf("RAID controller");
			break;
		default:
			printf("Unknown type %d", pinvent->inv_type);
			if(pinvent->inv_state & INV_REMOVE)
				printf(" / removable media");
		}
		printf(": unit %d", pinvent->inv_unit);
		if ((pinvent->inv_state>>8)&0xff)
			printf(", lun %ld", (pinvent->inv_state>>8)&0xff);
		printf(" on %s controller %d\n",
		       pinvent->inv_class == INV_VSCSI ? "VME-SCSI" : "SCSI",
		       pinvent->inv_controller);
		break;

	case INV_UNC_SCSILUN:
		/*
		 * We don't want to print anything for an unconnected SCSI
		 * lun in the inventory.
		 */
		break;

	case INV_SERIAL:
		switch(pinvent->inv_type) {
		case INV_CDSIO:
    printf("async serial controller: cdsio%d, firmware version %ld\n",
				pinvent->inv_controller, pinvent->inv_state);
			break;
		case INV_T3270:
		printf("IBM 3270 Terminal Emulation controller %d\n",
				pinvent->inv_controller);
			break;
		case INV_GSE:
			printf("5080 Gateway card %d\n",
				pinvent->inv_controller);
			break;
		case INV_SI:
			printf("SNA SDLC controller %d\n",
				pinvent->inv_controller);
			break;
		case INV_M333X25:
			printf("Motorola X.25 controller %d\n",
				pinvent->inv_controller);
			break;
		case INV_ONBOARD:
		if (sysmp(MP_NPROCS) > 1)
			printf("On-board serial ports: %d per CPU board\n",
							pinvent->inv_state);
		else
			printf("On-board serial ports: %d\n", pinvent->inv_state);
		break;
		case INV_EPC_SERIAL:
			printf("Integral EPC serial ports: %d\n", pinvent->inv_state);
			break;
		case INV_ICA:
			printf("IRIS Channel Adapter board %d\n",
				pinvent->inv_controller);
			break;
		case INV_VSC:
			printf("VME Synchronous Communications board %d\n",
				pinvent->inv_controller);
			break;
		case INV_ISC:
			printf("ISA Synchronous Communications board %d\n",
				pinvent->inv_controller);
			break;
		case INV_GSC:
			printf("GIO Synchronous Communications board %d\n",
				pinvent->inv_controller);
			break;
		case INV_PSC:
			printf("PCI Synchronous Communications board %d\n",
				pinvent->inv_controller);
			break;
		case INV_ASO_SERIAL:
			printf("ASO 6-port Serial board %d: revision %d.%d.%d"
			       ", Ebus slot %d, IO Adapter %d\n",
			       pinvent->inv_controller, 
			       (pinvent->inv_state >> 28) & 0xf,  /*dang rev*/
			       (pinvent->inv_state >> 12) & 0xff, /*cpld erev*/
			       (pinvent->inv_state >> 20) & 0xff, /*cpld irev*/
			       (pinvent->inv_state >>  4) & 0xff, /*EBUS slot*/
			       (pinvent->inv_state >>  0) & 0xf); /*IOA num*/
			break;
		}
		break;

	case INV_PARALLEL:
		switch(pinvent->inv_type) {
		case INV_IKC:
			printf("IKON Hardcopy controller %d\n",
				pinvent->inv_controller);
			break;
		case INV_GPIB:
			printf("IEEE-488 bus controller %d\n",
				pinvent->inv_controller);
			break;
		case INV_EPC_PLP:
			printf("Integral EPC parallel port: Ebus slot %d\n",
				pinvent->inv_controller);
			break;
		case INV_ONBOARD_PLP:
			if (pinvent->inv_state == 1)
				printf("On-board bi-directional parallel port\n");
			else
				printf("On-board parallel port\n");
			break;
		case INV_EPP_ECP_PLP:
			printf("On-board EPP/ECP parallel port\n");
			break;
		}
		break;
	case INV_AUDIO:
		switch(pinvent->inv_type) {
		case INV_AUDIO_HDSP:
		    printf("Iris Audio Processor: revision %d\n",
			pinvent->inv_state);
		    break;

		case INV_AUDIO_A2:
		    printf("Iris Audio Processor: version A2 revision %d.%d.%d",
		       ((unsigned int)pinvent->inv_state & 0x7000) >> 12,
		       ((unsigned int)pinvent->inv_state & 0xf0) >> 4,
		       (unsigned int)pinvent->inv_state & 0xf);
		    /*
		     * for EVEREST (ASO) machines only
		     */
		    if ((pinvent->inv_state & 0xfe000000) != 0) {
			printf(" unit %d, Ebus slot %d adapter %d",
			       pinvent->inv_controller,
			       (pinvent->inv_state & 0xf0000000) >> 28,
			       (pinvent->inv_state & 0x0e000000) >> 25);
		    }
		    printf("\n");
		    break;

		case INV_AUDIO_A3:
		    /* Moosehead (IP32) baseboard audio - AD1843 codec */
		    printf("Iris Audio Processor: version A3 revision %d",
		       (unsigned int)pinvent->inv_state);
		    printf("\n");
		    break;			

		case INV_AUDIO_RAD:
		    /* RAD PCI chip */
		    printf("Iris Audio Processor: version RAD revision %d.%d,"
			   " number %d",
			   ((unsigned int)pinvent->inv_state >> 4) & 0xf,
			   (unsigned int)pinvent->inv_state & 0xf,
			   pinvent->inv_controller);
		    printf("\n");
		    break;			

		case INV_AUDIO_VIGRA110:
		    printf
		       ("ViGRA 110 Audio %d, base 0x%06X, revision %d\n",
		       (int)pinvent->inv_unit, 
		       (unsigned int)pinvent->inv_state,
		       (int)pinvent->inv_controller);
		    break;

		case INV_AUDIO_VIGRA210:
		    printf
		       ("ViGRA 210 Audio %d, base 0x%06X, revision %d\n",
		       (int)pinvent->inv_unit, 
		       (unsigned int)pinvent->inv_state,
		       (int)pinvent->inv_controller);
		    break;

		}
		break;
	case INV_TAPE:
		switch(pinvent->inv_type) {
		case INV_VSCSITAPE:
		case INV_SCSIQIC: /* bad name: ANY SCSI tape, not just QIC drives */
			printf("  Tape drive: unit %d on ", pinvent->inv_unit);
			if (pinvent->inv_type == INV_VSCSITAPE)
				printf("VME-");
			printf("SCSI controller %d: ",
				pinvent->inv_controller);
			switch(pinvent->inv_state) {
			case TPQIC24:
				printf("QIC 24\n");
				break;
			case TPQIC150:
				printf("QIC 150\n");
				break;
			case TPQIC1000:
				printf("QIC 1000\n");
				break;
			case TPQIC1350:
				printf("QIC 1350\n");
				break;
			case TPDAT:
				printf("DAT\n");
				break;
			case TP9TRACK:
				printf("9 track\n");
				break;
			case TP8MM_8200:
				printf("8mm(8200) cartridge\n");
				break;
			case TP8MM_8500:
				printf("8mm(8500) cartridge\n");
				break;
			case TP3480:
				printf("3480 cartridge\n");
				break;
			case TPDLT:
			case TPDLTSTACKER:
				printf("DLT\n");
				break;
			case TPD2:
				printf("D2 cartridge\n");
				break;
			case TPNTP:
			case TPNTPSTACKER:
				printf("NTP\n");
				break;
			case TPSTK9490:
				printf("STK 9490\n");
				break;
			case TPSTKSD3:
				printf("STK SD3\n");
				break;
			case TPUNKNOWN:
				printf("unknown\n");
				break;
			default:
				printf("unknown type %d\n", pinvent->inv_state);
			}
			/* The TPNTPSTACKER is both a drive and stacker
			 * at the same target and lun 0.
			 */
			if ((pinvent->inv_state == TPNTPSTACKER) &&
			    ((sa == NULL) || (sa->class == ALL)) )
			    printf("  Jukebox: unit %d on SCSI controller %d\n",
				pinvent->inv_unit, pinvent->inv_controller);
			break;
		case INV_XM:
			printf("Xylogics 1/2 inch tape controller %d ctlr type: %x firmware: %ld.%ld\n",
				pinvent->inv_controller,
				pinvent->inv_state >> 16,
				(pinvent->inv_state >> 8) & 0xff,
				pinvent->inv_state  & 0xff );
			break;
		case INV_ISIQIC:
			printf("ISI QIC-02 tape controller %d\n",
				pinvent->inv_controller);
			break;
		}
		break;

	case INV_GRAPHICS: {
		options=0;
		switch(pinvent->inv_type) {
		case INV_GR1BOARD:
			printf("Graphics board");
			if (ngfxbds > 1)
				printf(" %d", pinvent->inv_controller);
			printf(": ");
			if ((pinvent->inv_state & INV_GR1BUSMASK)
			    == INV_GR1PBVME) 
				printf("VGR2");
			else
				printf("GR1");
			printf(".%ld ", pinvent->inv_state & INV_GR1REMASK);
			if (pinvent->inv_state & INV_GR1BIT24) {
				printf("Bit-plane");
				options++;
			}
			else {
			    /* "Auxiliary planes" printed out 
			       only for Da Vinci product */
			    if (pinvent->inv_state & INV_GR1AUX4) {
				printf("Auxiliary planes");
				options++;
			    }
			}
			if (pinvent->inv_state & INV_GR1ZBUF24) {
				printf("%sZ-buffer",options?", ":" ");
				options++;
			}
			if (pinvent->inv_state & INV_GR1TURBO) {
				printf("%sTurbo",options?", ":" ");
				options++;
			}
			if (pinvent->inv_state & INV_GR1SMALLMON) {
				printf("%sSmall monitor",
					options?", ":" ");
				options++;
			}
			if (options)
				printf(" option%sinstalled\n",
					(options>1)?"s ":" ");
			else
				printf("\n");
			break;
		case INV_GRODEV:
			printf("Graphics option installed\n");
			break;
		case INV_GMDEV:
			printf("GT Graphics option installed\n");
			break;
		case INV_VGX:
			printf("VGX Graphics option installed\n");
			break;
		case INV_VGXT:
			printf("VGXT Graphics option installed\n");
			break;
		case INV_RE:
			if (pinvent->inv_state)
				printf("RealityEngineII Graphics Pipe %d at IO Slot %d Physical Adapter %d (Fchip rev %d%s)\n",
					pinvent->inv_unit, pinvent->inv_controller/8, pinvent->inv_controller%8,
					pinvent->inv_state,
					(pinvent->inv_state==1)?"<-BAD!":"");
			else
				printf("RealityEngine Graphics option installed\n");
			break;
		case INV_VTX:
			if (pinvent->inv_state)
				printf("VTX Graphics Pipe %d at IO Slot %d Physical Adapter %d (Fchip rev %d%s)\n",
					pinvent->inv_unit, pinvent->inv_controller/8, pinvent->inv_controller%8,
					pinvent->inv_state,
					(pinvent->inv_state==1)?"<-BAD!":"");
			else
				printf("VTX Graphics option installed\n");
			break;
		case INV_CG2:
			printf("Genlock option installed\n");
			break;
		case INV_LIGHT:
			printf("Graphics board: LG1\n");
			break;
		case INV_GR2:
			if (pinvent->inv_state & INV_GR2_GR3)
				printf("Graphics board: GR3");
			else if (pinvent->inv_state & INV_GR2_INDY)
				printf("Graphics board: GR3");
			else if (pinvent->inv_state & INV_GR2_GR5)
				printf("Graphics board: GR5");
			else if (pinvent->inv_state & INV_GR2_GU1)
				printf("Graphics board: GU1");
			else
				printf("Graphics board: GR2");

			switch (pinvent->inv_state & 0x3c) {
				case INV_GR2_1GE:
					printf("-XS");
					if (pinvent->inv_state & INV_GR2_24)
						printf("24");
					if (pinvent->inv_state & INV_GR2_Z)
						printf(" with Z-buffer");
					break;
				case INV_GR2_2GE:
					printf("-XZ");
					if ((pinvent->inv_state & INV_GR2_24) == 0)
						printf(" missing bitplanes");
					if ((pinvent->inv_state & INV_GR2_Z) == 0)
						printf(" missing Z");
					break;
				case INV_GR2_4GE:
					if ((pinvent->inv_state & INV_GR2_24) &&
					    (pinvent->inv_state & INV_GR2_Z)) {
					    if ((pinvent->inv_state & INV_GR2_INDY) || (pinvent->inv_state & INV_GR2_GR5))
						printf("-XZ");
					    else 
						printf("-Elan");
					}
					else
						printf("-XSM");
					break;
				case INV_GR2_8GE:
					if ((pinvent->inv_state & INV_GR2_24) &&
					    (pinvent->inv_state & INV_GR2_Z))
						printf("-Extreme");
					else {
						printf("-8GE,");
						if (pinvent->inv_state & INV_GR2_24)
							printf("24");
						if (pinvent->inv_state & INV_GR2_Z)
							printf("Z");
					}
					break;
					
				default:
					printf("-Unknown configuration");
					break;
			}
			printf("\n");
			break;
		case INV_NEWPORT:
			printf("Graphics board: ");
			if (pinvent->inv_state & INV_NEWPORT_XL)
				printf("XL\n");		/* Indigo2 */
			else
				printf("Indy %d-bit\n",
					(pinvent->inv_state & INV_NEWPORT_24) ? 24 : 8);
			break;
		case INV_MGRAS: {
			int state = pinvent->inv_state;

			printf("Graphics board: ");

			if (((state & INV_MGRAS_GES) == INV_MGRAS_1GE) &&
			    ((state & INV_MGRAS_RES) == INV_MGRAS_1RE) &&
			    ((state & INV_MGRAS_TRS) == INV_MGRAS_0TR))
				printf("Solid Impact");
			else if (((state & INV_MGRAS_GES) == INV_MGRAS_1GE) &&
				 ((state & INV_MGRAS_RES) == INV_MGRAS_1RE) &&
				 ((state & INV_MGRAS_TRS) != INV_MGRAS_0TR))
				printf("High Impact");
			else if (((state & INV_MGRAS_GES) == INV_MGRAS_2GE) &&
				 ((state & INV_MGRAS_RES) == INV_MGRAS_1RE) &&
				 ((state & INV_MGRAS_TRS) != INV_MGRAS_0TR))
				printf("High-AA Impact");
			else if (((state & INV_MGRAS_GES) == INV_MGRAS_2GE) &&
				 ((state & INV_MGRAS_RES) == INV_MGRAS_2RE) &&
				 ((state & INV_MGRAS_TRS) != INV_MGRAS_0TR))
				printf("Maximum Impact");
			else
				printf("Impact"); 	/* unknown config */

			if ((state & INV_MGRAS_TRS) > INV_MGRAS_2TR)
				printf("/TRAM option card");

			putchar('\n');
			break;
			}
		case INV_RE3: {
			int state = pinvent->inv_state;

			/* display state as hex number 'till we have real product names */
                        
                        if (verbose)
				printf("Graphics board: InfiniteReality (config 0x%08x)\n",state);
			else
				printf("Graphics board: InfiniteReality\n");

			break;
			}
		case INV_CRIME: {
			printf("CRM graphics installed\n");
			break;
			}
		}
		break;
	}

	case INV_NETWORK:
		switch(pinvent->inv_controller) {
		case INV_ETHER_EC:
			printf("Integral Ethernet: ec%d, version %ld\n",
			       pinvent->inv_unit, pinvent->inv_state);
			break;

		case INV_ETHER_GIO:
			printf("E++ controller: ec%d, version %ld\n",
			       pinvent->inv_unit, pinvent->inv_state);
			break;

		case INV_ETHER_ET:
			/* Print the unit number for dual IO3s */
			printf("Integral Ethernet: et%d, IO%ld\n",
				pinvent->inv_unit, pinvent->inv_state);
			break;

		case INV_ETHER_ENP:
			printf(
    "ENP-10 Ethernet controller: enp%d, firmware version %ld (%s)\n",
				pinvent->inv_unit, pinvent->inv_state,
				(pinvent->inv_state == 0) ? "CMC" : "SGI");
			break;
		case INV_ETHER_EE:
			printf("Integral Ethernet controller: et%d, Ebus slot %d\n",
				pinvent->inv_unit, pinvent->inv_state);
			break;
		case INV_HYPER_HY:
			printf(
		"HyperNet controller %d, adapter unit %#lx, port %ld\n",
				pinvent->inv_unit, pinvent->inv_state >> 8,
				pinvent->inv_state & 3);
			break;
		case INV_CRAYIOS_CFEI3:
			printf("CRAY FEI-3 controller %d %lx\n",
				pinvent->inv_unit, pinvent->inv_state);
			break;
		case INV_ETHER_EGL:
			printf("EFast Eagle controller: egl%d\n",
				pinvent->inv_unit);
			break;
		case INV_ETHER_FXP:
			printf("EFast FXP controller: fxp%d\n",
				 pinvent->inv_unit);
			break;
		case INV_ETHER_EP:
			/* mention each board only once */
			if (0 != (pinvent->inv_unit % EP_PORTS_OCT))
				break;
			i = pinvent->inv_state & EP_VERS_DATE_M;
			printf("E-Plex Ethernet controller: ep%d-%d,"
			       " slot %d, adapter %d,"
			       " firmware %d%02d%02d%02d00\n",
			       pinvent->inv_unit,
			       pinvent->inv_unit+EP_PORTS-1,
			       ((pinvent->inv_state&EP_VERS_SLOT_M)
				>>EP_VERS_SLOT_S),
			       ((pinvent->inv_state&EP_VERS_ADAP_M)
				>>EP_VERS_ADAP_S),
			       (i/(24*32*13)) + 92,
			       (i/(24*32)) % 13,
			       (i/24) % 32,
			       i % 24);
			break;
		case INV_FDDI_IPG:
			printf("FDDIXPress controller: ipg%d, version %ld\n",
				pinvent->inv_unit, pinvent->inv_state);
			break;
		case INV_TOKEN_FV:
			printf("IRIS TokenRing controller fv%d: %d Mbit\n",
				pinvent->inv_unit,
				pinvent->inv_state ? 16 : 4);
			break;
		case INV_TOKEN_GTR:
			printf("IRIS TokenRing controller gtr%d: %d Mbit\n",
				pinvent->inv_unit,
				pinvent->inv_state ? 16 : 4);
			break;
		case INV_TOKEN_MTR:
			printf("IRIS TokenRing controller mtr%d: %d Mbit\n",
				pinvent->inv_unit,
				pinvent->inv_state ? 16 : 4);
			break;
		case INV_FDDI_RNS:
			printf("RNS 2200 PCI/FDDI controller: rns%d, version %ld\n",
				pinvent->inv_unit, pinvent->inv_state);
			break;
		case INV_FDDI_XPI:
			printf("XPI FDDI controller: xpi%d",
			       pinvent->inv_unit);
			if (pinvent->inv_state & XPI_VERS_DANG) {
				printf(", slot %d, adapter %d",
				       ((pinvent->inv_state&XPI_VERS_SLOT_M)
					>>XPI_VERS_SLOT_S),
				       ((pinvent->inv_state&XPI_VERS_ADAP_M)
					>>XPI_VERS_ADAP_S));
				i = (pinvent->inv_state&XPI_VERS_MEZ_M) * 60;
			} else {
				i = pinvent->inv_state & XPI_VERS_M;
			}
			printf(", firmware version %d%02d%02d%02d%02d",
				(i/(60*24*32*13)) + 92,
				(i/(60*24*32)) % 13,
				(i/(60*24)) % 32,
				(i/60) % 24,
				i % 60);
			i = (((uint)pinvent->inv_state)/XPI_VERS_PHY2) & 0x3;
			printf(", %s",
			       "SAS\0xxxxDAS\0xxxxDAS-DM\0x?3\0" + i*8);
			if (pinvent->inv_state & XPI_VERS_BYPASS)
				printf(" with bypass\n");
			else
				printf("\n");
			break;
		case INV_HIO_HIPPI:
			printf( "HIPPI adapter: hippi%d, slot %d adap %d, ",
				pinvent->inv_unit,
				pinvent->inv_state>>28 & 0x0F,
				pinvent->inv_state>>24 & 0x0F );
			printf( "firmware version %02d%02d%02d\n",
				pinvent->inv_state>>16 & 0xFF,
				pinvent->inv_state>>8 & 0xFF,
				pinvent->inv_state & 0xFF );
			break;
		case INV_ISDN_SM:
			printf("%sISDN: Basic Rate Interface unit %d, " /*sic*/
			       "revision %d.%d\n",
			       (pinvent->inv_state & 0x01000000 ? 
				"Integral " : ""),
			       pinvent->inv_unit, 
			       (pinvent->inv_state & 0xff00)>>8,
			       pinvent->inv_state & 0x00ff);
			break;
		case INV_ISDN_48XP:
			printf("ISDN: VME Primary Rate Interface unit %d, "
			       "revision %d.%d\n",
			       pinvent->inv_unit,
			       (pinvent->inv_state & 0xff00)>>8,
			       pinvent->inv_state & 0x00ff);
			break;
		case INV_ATM_GIO64:
			printf("ATM OC-3c unit %d: slot %d, adapter %d\n",
				pinvent->inv_unit,
				pinvent->inv_state>>24 & 0xff,
				pinvent->inv_state>>16 & 0xff );
			break;
		}
		break;
	
	case INV_VIDEO:
		switch(pinvent->inv_type) {

		case INV_VIDEO_LIGHT:
			printf("IndigoVideo board: unit %d, revision %d\n",
				pinvent->inv_unit, pinvent->inv_state);
			break;

		case INV_VIDEO_VS2:
			printf("Multi-Channel Option board installed\n");
			break;

		case INV_VIDEO_VINO:
			printf("Vino video: unit %d, revision %d%s\n",
			   pinvent->inv_unit, 
			   pinvent->inv_state & INV_VINO_REV,
			   (pinvent->inv_state & INV_VINO_INDY_NOSW) ? "" :
			   ((pinvent->inv_state & INV_VINO_INDY_CAM)
						? ", IndyCam connected"
						: ", IndyCam not connected"));
			break;

		case INV_VIDEO_EXPRESS:
			if (pinvent->inv_state & INV_GALILEO_JUNIOR)
			    printf("Indigo2 video (ev1): unit %d, revision %d.  %s  %s\n",
				pinvent->inv_unit, 
				pinvent->inv_state & INV_GALILEO_REV,
			   	(pinvent->inv_state & INV_GALILEO_DBOB)
						? "601 option connected." : "",
			   	(pinvent->inv_state & INV_GALILEO_INDY_CAM)
						? "Indycam connected." : "");
			else
			    printf("Galileo video %s (ev1): unit %d, revision %d.  %s  %s\n",
			   	(pinvent->inv_state & INV_GALILEO_ELANTEC)
						? "1.1" : "",
				pinvent->inv_unit, 
				pinvent->inv_state & INV_GALILEO_REV,
			   	(pinvent->inv_state & INV_GALILEO_DBOB)
						? "601 option connected." : "",
			   	(pinvent->inv_state & INV_GALILEO_INDY_CAM)
						? "Indycam connected." : "");
			break;

		case INV_VIDEO_INDY:
			printf("Indy Video (ev1): unit %d, revision %d\n",
				pinvent->inv_unit, pinvent->inv_state);
			break;

		case INV_VIDEO_MVP:
			printf ("Video: MVP unit %d version %d.%d\n",
			    pinvent->inv_unit,
			    INV_MVP_REV(pinvent->inv_state),
			    INV_MVP_REV_SW(pinvent->inv_state));

			if (INV_MVP_AV_BOARD(pinvent->inv_state)) {
			    printf("AV: AV%d Card version %d", 
				INV_MVP_AV_BOARD(pinvent->inv_state),
				INV_MVP_AV_REV(pinvent->inv_state));

			    if (INV_MVP_CAMERA(pinvent->inv_state)) {
				printf(
				    ", O2Cam type %d version %d connected.\n", 
				    INV_MVP_CAMERA(pinvent->inv_state),
				    INV_MVP_CAM_REV(pinvent->inv_state));
			    }
			    else if (INV_MVP_SDIINF(pinvent->inv_state)) {
				printf(
				    ", SDI%d Serial Digital Interface "
				       "version %d connected.\n", 
				    INV_MVP_SDIINF(pinvent->inv_state),
				    INV_MVP_SDI_REV(pinvent->inv_state));
			    }
			    else {
				printf(", Camera not connected.\n");
			    }
			}
			else {
			    printf(" with no AV Card or Camera.\n");
			    break;
			}
			break;

		case INV_VIDEO_INDY_601:
			printf("Indy Video 601 (ev1): unit %d, revision %d\n",
				pinvent->inv_unit, pinvent->inv_state);
			break;

		case INV_VIDEO_VO2:
			printf("Sirius video: unit %d revision %d on bus %d with ",
				pinvent->inv_unit,
				pinvent->inv_state & 0x0f,
				pinvent->inv_controller);
			if (!(pinvent->inv_state & 0xf0)) printf("no ");
			if (pinvent->inv_state & 0x80) printf("CPI ");
			if (pinvent->inv_state & 0x40) printf("DGI ");
			if (pinvent->inv_state & 0x20) printf("BOB ");
			if (pinvent->inv_state & 0x10) printf("SD1 ");
			printf("options\n");
			break;

		case INV_VIDEO_MGRAS:
			printf("IMPACT Video (mgv): unit %d, revision %d",
				pinvent->inv_unit, pinvent->inv_state & 0xf);
			if (!(pinvent->inv_state & 0xf0)) printf("\n");
			else printf(", TMI/CSC: revision %d\n", 
				(pinvent->inv_state & 0xf0) >> 4);
			break;

		default:
			fprintf(stderr, "%s: Unknown type %d class %d\n",
				argv0, pinvent->inv_type, pinvent->inv_class);
		}
		break;

	case INV_BUS:
		switch(pinvent->inv_type) {

		case INV_BUS_VME:
			if( pinvent->inv_controller == 0 )
			    printf("VME bus: adapter 0 mapped to adapter %d\n",
				pinvent->inv_state);
			else
			    printf("VME bus: adapter %d\n", pinvent->inv_state);
			break;

		case INV_BUS_EISA:
			printf("EISA bus: adapter %d\n", pinvent->inv_unit);
			break;

		case INV_BUS_BT3_PCI:
			printf("Bit3 PCI Bridge Card: Bus %d, Slot %d\n",
				pinvent->inv_controller,pinvent->inv_unit);
			break;
		default:
			fprintf(stderr, "%s: Unknown type %d class %d\n",
				argv0, pinvent->inv_type, pinvent->inv_class);
		}
		break;

	case INV_MISC:
		switch(pinvent->inv_type) {

		case INV_MISC_EINT:
			printf("EPC external interrupts\n");
			break;
			
		default:
			fprintf(stderr, "%s: Unknown type %d class %d\n",
				argv0, pinvent->inv_type, pinvent->inv_class);
		}
		break;

	case INV_COMPRESSION:

		switch(pinvent->inv_type) {

		case INV_COSMO:
			printf("Cosmo Compression: unit %d, revision %d\n",
				pinvent->inv_controller, pinvent->inv_state);
			break;

		case INV_INDYCOMP:
			printf("IndyComp: unit %d, revision %x:%x\n",
				pinvent->inv_controller, pinvent->inv_unit,
				pinvent->inv_state);
			break;

		case INV_IMPACTCOMP:
			printf("IMPACT Compression: unit %d, revision %d:%d\n",
				pinvent->inv_controller, pinvent->inv_unit,
				pinvent->inv_state);
			break;

		case INV_VICE:
			{ char *chid;
			switch (pinvent->inv_state) {
			case 0xe1:	chid = "EN";		break;
			case 0xe2:	chid = "DX";		break;
			case 0xe3:	chid = "TRE";		break;
			case 0xe4:	chid = "QT";		break;
			default:	chid = 0;		break;
			}
			if (chid) {
				printf("Vice: %s\n", chid);
			} else {
				printf("Vice: %x\n", pinvent->inv_state);
			}
			break;
			}

		default:
			fprintf(stderr, "%s: Unknown type %d class %d\n",
				argv0, pinvent->inv_type, pinvent->inv_class);
		}
		break;

	case INV_DISPLAY:

		switch(pinvent->inv_type) {
		    
		case INV_PRESENTER_BOARD:
		        printf("Presenter adapter board.\n");
		        break;

		case INV_PRESENTER_PANEL:
		        printf("Presenter adapter board and display.\n");
		        break;

		case INV_ICO_BOARD:
		        printf("IMPACT Channel Option Board\n");
			break;
			    
		default:
			fprintf(stderr, "%s: Unknown type %d class %d\n",
				argv0, pinvent->inv_type, pinvent->inv_class);
		}
		break;

	case INV_PCI_NO_DRV:
		/* reuse the inv_class here */
		    switch (pinvent->inv_type){
			case INV_DISK:
				printf("Mass Storage ");
				break;
			case INV_MEMORY:
				printf("Memory ");
				break;
			case INV_GRAPHICS:
				printf("Display ");
				break;
			case INV_NETWORK:
				printf("Network ");
				break;
			case INV_VIDEO:
				printf("Multimedia ");
				break;
			case INV_BUS:
				printf("Bridge ");
				break;
			case INV_PCI:
				printf("Unknown Type ");
				break;
			default:
				printf("Unknown - %d ", pinvent->inv_type);
		}

		printf("PCI: Bus %d, Slot %d, Function %d, Vendor ID 0x%x, Device ID 0x%x\tNo driver\n",
			(pinvent->inv_controller >> 16) & 0xff, 
			(pinvent->inv_controller >> 0) & 0xff, 
			(pinvent->inv_controller >> 8) & 0xff, 
			pinvent->inv_unit, 
			pinvent->inv_state);
		break;

	case INV_PCI:
		{
		int unknown = 0;
		printf("PCI: Bus %d, Slot %d, ", 
				(pinvent->inv_controller >> 16) & 0xff,
				pinvent->inv_controller & 0xff);
		switch (pinvent->inv_unit){
			case 0x9004: printf("Adaptec");break;
			case 0x104c: printf("Racore");break;
			case 0x0e11: printf("Compaq");break;
			case 0x10b6: printf("Madge PCI TokenRing adapter"); 
				break;
			default: printf("vendor: 0x%x", pinvent->inv_unit); 
				unknown++;
				break;
		}
		switch (pinvent->inv_state){
			case 0xae32:
			case 0x0500: printf(" 10/100Mb Fast Ethernet controller"); break;
			case 0x8078: printf(" SCSI controller");
			default: printf(", device: 0x%x", pinvent->inv_state); 
				unknown++;
				break;
		}
		if (unknown){
		    printf(", type: ");
		    switch (pinvent->inv_type){
			case INV_DISK:
				printf("Mass Storage ");
				break;
			case INV_MEMORY:
				printf("Memory ");
				break;
			case INV_GRAPHICS:
				printf("Display ");
				break;
			case INV_NETWORK:
				printf("Network ");
				break;
			case INV_VIDEO:
				printf("Multimedia ");
				break;
			case INV_BUS:
				printf("Bridge ");
				break;
			case INV_PCI:
				printf("Unknown ");
				break;
		     }
		}
		printf("\n");

		}
		break;
	default:
		fprintf(stderr,"%s: Unknown inventory class %d\n",
				argv0,pinvent->inv_class);
		break;
	}

	return 0;
}


/*
 * dkippmode interprets the controller mode passed by the dkip driver to 
 * tell such things as MACSI mode, cache enabled etc.
 */
#define BIT(x)	(0x01 << x)

/*
 * Controller BIT flags (from dkipreg.h)
 */
#define	CI_SG		BIT(0)		/* do Scatter/Gather		*/
#define CI_WSG		BIT(1)		/* do Word Wide Scatter/Gather	*/
#define	CI_CACHE	BIT(2)		/* Is running Read-Ahead	*/
#define	CI_4UNIT	BIT(3)		/* Is running in 4 Unit mode	*/
#define	CI_MACSI	BIT(4)		/* Is running in MACI mode	*/
#define CI_SORT		BIT(5)		/* Is using Sorting in MACSI	*/
#define CI_OVLSEEK	BIT(6)		/* OVERLAPPED SEEKS in MACSI	*/
#define CI_AUTOREST	BIT(7)		/* AUTO drive clear in MACSI	*/

static int
dkippmode(inventory_t *inv)
{
	printf("Firmware Revision %c%c%c\n", inv->inv_unit,
	       inv->inv_state >> 24, inv->inv_state >> 16);
	if (!verbose) return;
	printf("\t\tmode: ");
	if (inv->inv_state & CI_SG) printf(" SG");
	if (inv->inv_state & CI_WSG) printf(" WSG");
	if (inv->inv_state & CI_CACHE) printf(" CACHE");
	if (inv->inv_state & CI_4UNIT) printf(" 4UNIT");
	else printf(" 2UNIT");
	if (inv->inv_state & CI_MACSI) printf(" MACSI");
	if (inv->inv_state & CI_SORT) printf(" SORT");
	if (inv->inv_state & CI_OVLSEEK) printf(" OVLSEEK");
	if (inv->inv_state & CI_AUTOREST) printf(" AUTORESET");
	printf("\n");
}


int found_item = 0;

int
search_item_func(pinvent, sa)
inventory_t *pinvent;
struct search_args *sa;
{
	if ((sa->class == ALL       || pinvent->inv_class == sa->class ) &&
	    (sa->type  == ALL       || pinvent->inv_type  == sa->type  ) &&
	    (sa->cont  == ALL || pinvent->inv_controller == sa->cont ) &&
	    (sa->unit  == ALL || pinvent->inv_unit == sa->unit  )) {
		if (!sflag )
			(void)display_item(pinvent, (void *)sa);
		found_item = 1;
	}
	return 0;
}

int
search_for_item(struct search_args *sa)
{
	int rc;

	rc = scaninvent((int (*)()) search_item_func, (void *) sa);
	if (rc < 0) {
		fprintf(stderr, "%s: can't read inventory table: %s\n",
			argv0, sys_errlist[errno]);
		exit(127);
	}
	return found_item;
}

display_all()
{
	(void) scaninvent(display_item, (char *)NULL);
}

int
lookup_class(names, arg)
char *names[];
char *arg;
{
	int i;

	for (i = 0; names[i]; i++ )
		if ( strcmp(names[i], arg) == 0 )
			return i+1;
	
	fprintf(stderr,"%s: unknown class `%s'\n",argv0,arg);
	exit(-1);
	/* NOTREACHED */
}

static void
lookup_type(char *s, struct search_args *sa)
{
	int i;

	for ( i = 0; searchtypes[i].mn; i++ )
		if ( strcmp(searchtypes[i].mn, s) == 0 ) {
			sa->class = searchtypes[i].class;
			sa->type = searchtypes[i].type;
			return;
		}
	
	fprintf(stderr,"%s: unknown type `%s'\n",argv0,s);
	exit(-1);
}

static void
lookup_dev(char *s, struct search_args *sa)
{
	int i;

	for ( i = 0; searchdevs[i].mn; i++ )
		if ( strcmp(searchdevs[i].mn, s) == 0 ) {
			sa->class = searchdevs[i].class;
			sa->type = searchdevs[i].type;
			sa->cont = searchdevs[i].cont;
			return;
		}
	
	fprintf(stderr,"%s: unknown device `%s'\n",argv0,s);
	exit(-1);
}

static void
usage()
{
	int c;

	fprintf(stderr,
		"usage: %s {-v -s -c class -t type -d dev -u unit}\n",
		argv0);

	fprintf(stderr, "       where class can be:\n");
	c = 0;
	for ( c = 0; classes[c]; c++ )
		fprintf(stderr, "           %s\n", classes[c]);

	fprintf(stderr, "       type can be:\n");
	c = 0;
	for ( c = 0; searchtypes[c].mn; c++ )
		fprintf(stderr, "           %s\n",
			searchtypes[c].mn);
	
	fprintf(stderr, "       dev can be:\n");
	c = 0;
	for ( c = 0; searchdevs[c].mn; c++ )
		fprintf(stderr, "           %s\n",
			searchdevs[c].mn);
}

void
main(argc,argv)
int argc;
char *argv[];
{
	int c;
	char *p;
	int single;
	struct search_args sa;

	single = 0;
	sa.class = sa.type = ALL;
	sa.cont = ALL;
	sa.unit = ALL;

	argv0 = argv[0];

#ifdef	SGAUX_GETNGFXBDS
	/* 
	 * This call will be supported on kernels where there can be more than
	 * one graphics board; the code handles -1 case correctly.
	 */
	if ((ngfxbds = sgigsc(SGAUX_GETNGFXBDS, 0)) < 0)
		errno = 0;	    /* reset errno */
#else	/* !SGAUX_GETNGFXBDS */
	ngfxbds = 1;
#endif	/* !SGAUX_GETNGFXBDS */


	while ((c = getopt(argc,argv,"vsc:t:d:u:g")) != EOF) {

		switch(c) {

		case 'v':
			/*
			 * Verbose mode: more info about cpu, mem and
			 * disk controllers
			 */
			verbose++;
			get_extended_info();
			break;

		case 's':
			/* Silent mode: exit code tells all */
			sflag++;
			break;

		case 'c':
			/* Look up all items in class.  Argument is either
			 * integer or mnemonic name of class.
			 */
			if (single) {
				usage();
				exit(-1);
			}
			if ( isdigit(*optarg) ) {
				sa.class = strtol(optarg,&p,0);
				if (*p != '\0' || sa.class <= 0) {
					fprintf(stderr,
						"%s: bad class number %s\n",
						argv0, optarg);
					exit(-1);
				}
			} else {
				sa.class = lookup_class(classes, optarg);
			}
			single++;
			break;

		case 't':
			/* Look up specific inventory sub-type */
			if (single) {
				usage();
				exit(-1);
			}
			single++;
			lookup_type(optarg, &sa);
			break;

		case 'd':
			if (single) {
				usage();
				exit(-1);
			}
			single++;
			lookup_dev(optarg, &sa);
			break;

		case 'u':
			c = strtol(optarg,&p,0);
			if (*p != '\0' || c >= 255 || c < 0) {
				fprintf(stderr, "%s: bad unit number %s\n",
					argv0, optarg);
				exit(-1);
			}
			sa.unit = c;
			break;
			
		default:
			usage();
			exit(-1);
		}
	}
	if (argc != optind) {
		usage();
		exit(-1);
	}

	if (!single) {
		if (sa.unit != ALL || sa.cont != ALL) {
			usage();
			exit(-1);
		}
		display_all();
		exit(0);
	}

	exit(!search_for_item(&sa));
	/* NOTREACHED */
}


/************************************************************************
* The following code was hacked out of os/machdep.c and should be updated
* accordingly.
*
*/

/*
 * coprocessor revision identifiers
 */
union rev_id {
	unsigned int	ri_uint;
	struct {
#ifdef MIPSEB
		unsigned int	Ri_fill:16,
				Ri_imp:8,		/* implementation id */
				Ri_majrev:4,		/* major revision */
				Ri_minrev:4;		/* minor revision */
#endif
#ifdef MIPSEL
		unsigned int	Ri_minrev:4,		/* minor revision */
				Ri_majrev:4,		/* major revision */
				Ri_imp:8,		/* implementation id */
				Ri_fill:16;
#endif
	} Ri;
};
#define	ri_imp		Ri.Ri_imp
#define	ri_majrev	Ri.Ri_majrev
#define	ri_minrev	Ri.Ri_minrev

struct imp_tbl {
	char *it_name;
	unsigned it_imp;
};


/* The code assumes that any number greater than the number in the
 * table is that type of processor.  They must be in decending numerical
 * order.  The below table is in the kernel also, so the values must
 * be updated there also if cpu_imp_tbl is changed in hinv.c.
 */
struct imp_tbl cpu_imp_tbl[] = {
	{ "Unknown CPU type.",	C0_MAKE_REVID(C0_IMP_UNDEFINED,0,0) },
	{ "MIPS R5000 Processor Chip",	C0_MAKE_REVID(C0_IMP_R5000,0,0) },
	{ "MIPS R4650 Processor Chip",	C0_MAKE_REVID(C0_IMP_R4650,0,0) },
	{ "MIPS R4700 Processor Chip",	C0_MAKE_REVID(C0_IMP_R4700,0,0) },
	{ "MIPS R4600 Processor Chip",	C0_MAKE_REVID(C0_IMP_R4600,0,0) },
	{ "MIPS R8000 Processor Chip",	C0_MAKE_REVID(C0_IMP_R8000,0,0) },
	{ "MIPS R10000 Processor Chip",	C0_MAKE_REVID(C0_IMP_R10000,0,0) },
	{ "MIPS R6000A Processor Chip",	C0_MAKE_REVID(C0_IMP_R6000A,0,0) },
	{ "MIPS R4400 Processor Chip",	C0_MAKE_REVID(C0_IMP_R4400,C0_MAJREVMIN_R4400,0) },
	{ "MIPS R4000 Processor Chip",	C0_MAKE_REVID(C0_IMP_R4000,0,0) },
	{ "MIPS R6000 Processor Chip",	C0_MAKE_REVID(C0_IMP_R6000,0,0) },
	{ "MIPS R3000A Processor Chip",	C0_MAKE_REVID(C0_IMP_R3000A,C0_MAJREVMIN_R3000A,0) },
	{ "MIPS R3000 Processor Chip",	C0_MAKE_REVID(C0_IMP_R3000,C0_MAJREVMIN_R3000,0) },
	{ "MIPS R2000A Processor Chip",	C0_MAKE_REVID(C0_IMP_R2000A,C0_MAJREVMIN_R2000A,0) },
	{ "MIPS R2000 Processor Chip",	C0_MAKE_REVID(C0_IMP_R2000,0,0) },
	{ 0,						0 }
};

/* The code assumes that any number greater than the number in the
 * table is that type of FPU.  They must be in decending numerical
 * order.
 */
struct imp_tbl fp_imp_tbl[] = {
	{ "Unknown FPU type",			    C0_MAKE_REVID(C0_IMP_UNDEFINED,0,0) },
	{ "MIPS R5000 Floating Point Coprocessor", C0_MAKE_REVID(C0_IMP_R5000,0,0) },
	{ "MIPS R4650 Floating Point Coprocessor",  C0_MAKE_REVID(C0_IMP_R4650,0,0) },
	{ "MIPS R4700 Floating Point Coprocessor",  C0_MAKE_REVID(C0_IMP_R4700,0,0) },
	{ "MIPS R4600 Floating Point Coprocessor",  C0_MAKE_REVID(C0_IMP_R4600,0,0) },
	{ "MIPS R8010 Floating Point Chip",	    C0_MAKE_REVID(0x10,0,0) },
	{ "MIPS R10010 Floating Point Chip",	    C0_MAKE_REVID(9,0,0) },
	{ "MIPS R4000 Floating Point Coprocessor",  C0_MAKE_REVID(5,0,0) },
	{ "MIPS R6010 Floating Point Chip",	    C0_MAKE_REVID(4,0,0) },
	{ "MIPS R3010A VLSI Floating Point Chip",   C0_MAKE_REVID(3,3,0) },
	{ "MIPS R3010 VLSI Floating Point Chip",    C0_MAKE_REVID(3,2,0) },
	{ "MIPS R2010A VLSI Floating Point Chip",   C0_MAKE_REVID(3,1,0) },
	{ "MIPS R2010 VLSI Floating Point Chip",    C0_MAKE_REVID(2,0,0) },
	{ "MIPS R2360 Floating Point Board",	    C0_MAKE_REVID(1,0,0) },
	{ 0,						0 }
};


char *
imp_name(union rev_id ri,
	 struct imp_tbl *itp)
{
	for (; itp->it_name; itp++)
		if (((ri.ri_imp << 8) | (ri.ri_majrev << 4) | (ri.ri_minrev))
							 >= itp->it_imp)
			return(itp->it_name);
	return("Unknown implementation");
}


static void
cpu_display(unsigned int rev)
{
	union rev_id ri;

	ri.ri_uint = rev;
	if (ri.ri_imp == 0) {
		ri.ri_majrev = 1;
		ri.ri_minrev = 5;
	}
	printf("CPU: %s Revision: %d.%u\n", imp_name(ri, cpu_imp_tbl),
				    ri.ri_majrev, ri.ri_minrev);
}

static void
fpu_display(unsigned int rev)
{
	union rev_id ri;

	ri.ri_uint = rev;
	printf("FPU: %s Revision: %d.%u\n", imp_name(ri, fp_imp_tbl),
				    ri.ri_majrev, ri.ri_minrev);
}


char *
proc_name(int type)
{
	switch (type) {
	case EVTYPE_IP19:
		return "R4400";
	case EVTYPE_IP21:
		return "R8000";
	case EVTYPE_IP25:
		return "R10000";
	default:
		return "Unknown";
	}
}
	
static void
display_cpubrd(evbrdinfo_t *brd, int vpid)
{
	evcpucfg_t *cpu;
	int i;

	for (i = 0; i < EV_CPU_PER_BOARD; i++) {

		cpu = &(brd->eb_cpuarr[i]);

		/* just find the one matched to display */
		if (vpid != ALL && vpid != cpu->cpu_vpid)
			continue;

		/* evinfo has too much in it. kick out the one that has nothing */
		if (cpu->cpu_vpid == 0 && cpu->cpu_speed == 0)
			continue;

		if (cpu->cpu_diagval == EVDIAG_NOTFOUND &&
	    	    cpu->cpu_inventory  != EVDIAG_NOTFOUND) {
			printf("  Processor   at Slot %d/Slice %d:\t%s (Invisible)\n", 
				brd->eb_slot, i, proc_name(brd->eb_type));
			continue;
		}

		/* Skip the slice if it doesn't look present */
		if (cpu->cpu_diagval == EVDIAG_NOTFOUND)
			continue;

		printf("  Processor %d at Slot %d/Slice %d: %d Mhz %s with %d MB secondary cache (%s)\n",
			cpu->cpu_vpid, brd->eb_slot, i,
			((brd->eb_type == EVTYPE_IP19) || (brd->eb_type == EVTYPE_IP25)) ? cpu->cpu_speed * 2 :
			cpu->cpu_speed,
			proc_name(brd->eb_type),
			((1 << cpu->cpu_cachesz) / (1024 * 1024)),
			(cpu->cpu_enable ? "Enabled" : "Disabled"));
	}
}

static void
display_all_cpus()
{
	evcfginfo_t *ep = &evcfg;
	evbrdinfo_t *brd;
	int slot;

	for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
		brd = &(ep->ecfg_board[slot]);
	
		if (brd->eb_type != EVTYPE_IP19 && brd->eb_type != EVTYPE_IP21 && brd->eb_type != EVTYPE_IP25)
			continue;

		printf("CPU Board at Slot %d: (%s)\n", slot,
			(brd->eb_enabled ? "Enabled" : "Disabled"));
	
		if (!brd->eb_enabled)
			continue;

		display_cpubrd(brd, ALL);
	}
}

/*
 * Array used to find the actual size of a particular
 * memory bank.  The size is in blocks of 256 bytes, since
 * the memory banks base address drops the least significant
 * 8 bits to fit the base addr in one word.
 */
unsigned long MemSizes[] = {
    0x04,    /* 0 = 4 meg    */
    0x10,    /* 1 = 16 megs   */
    0x20,    /* 2 = 32 megs  */
    0x40,    /* 3 = 64 megs  */
    0x40,    /* 4 = 64 megs  */
    0x100,   /* 5 = 256 megs */
    0x100,   /* 6 = 256 megs */
    0x0      /* 7 = no SIMM   */
};

static void
membrd_display_extended(int slot)
{
	evbnkcfg_t *bnk;
	evbrdinfo_t *brd;
	int i;
	int mem_size = 0;
	static int flip[] = {0, 4, 1, 5, 2, 6, 3, 7};
	
	brd = &(evcfg.ecfg_board[slot]);

	/* Skip empty slots*/
	if (brd->eb_type != EVTYPE_MC3)
		return;

	/* Count total amount of memory on board */
	for (i = 0; i < MC3_NUM_BANKS; i++)
		mem_size += (MemSizes[brd->eb_banks[i].bnk_size] * 4);
	
	printf("MC3 Memory Board at Slot %d: %d MB of memory (%s)\n",
		slot, mem_size, (brd->eb_enabled ? "Enabled" : "Disabled"));
	
	if (brd->eb_diagval != EVDIAG_PASSED)
		return;
	
	for (i = 0; i < MC3_NUM_BANKS; i++) {
		bnk = &(brd->eb_banks[flip[i]]);
	
		/* Skip this bank if it is empty */
		if (bnk->bnk_size == MC3_NOBANK)
			continue;
	
		printf("  Bank %c contains %d MB SIMMS (%s)\n",
			'A' + i, MemSizes[bnk->bnk_size],
			(bnk->bnk_enable ? "Enabled" : "Disabled"));
	}
}

static void
get_extended_info()
{
	int i, slot;
	evcfginfo_t *ep = &evcfg;
	evbrdinfo_t *brd;
	evcpucfg_t *cpu;

	/* don't send error msg because not every platform supports it */
	if (syssgi(SGI_GET_EVCONF, ep, sizeof(evcfginfo_t)) == -1)
		return;

	for (i = 0; i < EV_MAX_CPUS; i++)
		cpuid_to_slot[i] = -1;

	for (slot = 0; slot < EV_MAX_SLOTS; slot++) {

		brd = &(ep->ecfg_board[slot]);

		/* Skip empty slots*/
                if (brd->eb_type == EVTYPE_EMPTY)
			continue;

		switch (brd->eb_type) {
		case EVTYPE_IP19:
		case EVTYPE_IP21:
		case EVTYPE_IP25:
			for (i = 0; i < EV_CPU_PER_BOARD; i++) {
				cpu = &(brd->eb_cpuarr[i]);
				if (cpuid_to_slot[cpu->cpu_vpid] == -1)
					cpuid_to_slot[cpu->cpu_vpid] = slot;
			}
			break;

		case EVTYPE_MC3:
			break;

		case EVTYPE_IO4:
		case EVTYPE_EMPTY:
			break;

		default:
			printf("Unrecognized board.\n");
			break;
		}
	}
}
