
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
  **************************************************************************/

#ident	"arcs/ide/EVEREST/lib/disp_evcfg.c:  $Revision: 1.9 $"

/*
 * prints board configurations
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evdiag.h>
#include <ide_msg.h>
#include "libsc.h"

/*
 * dump_evconfig
 *	Prints out the contents of the everest configuration table.
 */

static void 
dump_general(evbrdinfo_t *brd)
{
	int rev;

	if (brd->eb_type != EVTYPE_IO4)
		rev = brd->eb_rev;
	else
		rev = (brd->eb_rev & IO4_REV_MASK) >> IO4_REV_SHFT;
	printf("   Rev: %d\tInventory: 0x%x    Diag Value: 0x%x, %s\n",
		rev, brd->eb_inventory, brd->eb_diagval,
		(brd->eb_enabled ? "Enabled" : "Disabled"));
	if ((brd->eb_diagval != EVDIAG_PASSED) &&
					(brd->eb_diagval != EVDIAG_NOT_PRESENT))
		printf("  *** Failed with error value %d\n",brd->eb_diagval);
}

 
static void 
dump_ip19(evbrdinfo_t *brd)
{
	int i;
	evcpucfg_t *cpu;

	dump_general(brd);

	for (i = 0; i < EV_MAX_CPUS_BOARD; i++) {
		cpu = &(brd->eb_cpuarr[i]);
		if (cpu->cpu_diagval != EVDIAG_NOTFOUND) {
			printf("        CPU %d: Inventory 0x%x, DiagVal 0x%x, Info 0x%x\n",
		      		i, cpu->cpu_inventory, cpu->cpu_diagval, 
				cpu->cpu_info);
			if (cpu->cpu_diagval != EVDIAG_PASSED)
			    printf("  *** Failed with error value %d\n",
				    brd->eb_diagval);
			printf("               Virt. #%d, Speed %d MHz, Cache Size %d kB, Prom rev %d, %s\n",
				cpu->cpu_vpid, cpu->cpu_speed, 
				1 << (cpu->cpu_cachesz - 10),
				(cpu->cpu_promrev),
				(cpu->cpu_enable ? "Enabled" : "Disabled"));
		} else {
			printf("        CPU %d: Not populated\n", i);
		}
	} 
}


static void
dump_ip21(evbrdinfo_t *brd)
{
	dump_general(brd);
}


static void 
dump_io4(evbrdinfo_t *brd)
{
	int i;
	evioacfg_t *ioa;

	dump_general(brd);
	printf("  Window Number: %d\n", brd->eb_io.eb_winnum);

	for (i = 1; i < IO4_MAX_PADAPS; i++) {
	
		ioa = &(brd->eb_ioarr[i]);

		printf("        PADAP %d: ", i);
		switch(ioa->ioa_type) {
		  case IO4_ADAP_EPC:
			printf("EPC ");
			break;
		  case IO4_ADAP_SCSI:
			printf("S1  ");
			break;
		  case IO4_ADAP_FCHIP:
			printf("F   ");
			break;
		  case IO4_ADAP_DANG:
			printf("DANG");
			break;
		  case 0:
			printf("Not populated.\n");
			continue;
			break;
		  default:
			printf("???");
			break;
		}

		printf(" (0x%x), Inventory 0x%x, DiagVal 0x%x, VirtID %d, %s\n",
			ioa->ioa_type, ioa->ioa_inventory, 
			ioa->ioa_diagval, ioa->ioa_virtid, 
			(ioa->ioa_enable ? "Enabled" : "Disabled"));
			if (ioa->ioa_diagval != EVDIAG_PASSED)
			    printf("  *** Failed with error value %d\n",
				    brd->eb_diagval);
	}
}


static void 
dump_mc3(evbrdinfo_t *brd)
{
	int i;
	evbnkcfg_t *mem;

	dump_general(brd);

	for (i = 0; i < MC3_NUM_BANKS; i++) {
		mem = &(brd->eb_banks[i]);
		if (mem->bnk_size != MC3_NOBANK) {
			printf("        Bank %d: IP %d, IF %d, SIMM type %d, Bloc 0x%x\n",
				i, mem->bnk_ip, mem->bnk_if, mem->bnk_size, 
				mem->bnk_bloc);
			printf("                Inventory 0x%x, DiagVal 0x%x, %s\n",
				mem->bnk_inventory, mem->bnk_diagval,
				(mem->bnk_enable ? "Enabled" : "Disabled"));
			if (mem->bnk_diagval != EVDIAG_PASSED)
				printf("  *** Failed with error value %d\n",
					brd->eb_diagval);
		} else {
			printf("        Bank %d: Not populated.\n",
				i);	
		}
	}
}


void
dump_evconfig_entry(int slot)
{
	evbrdinfo_t *brd;

	brd = &(EVCFGINFO->ecfg_board[slot]);

	printf("Slot 0x%x: Type = 0x%x, Name = ", slot, brd->eb_type);
	switch(brd->eb_type) {
	  case EVTYPE_IP19:
		printf("IP19\n");
		dump_ip19(brd);
		break;
	  case EVTYPE_IP21:
		printf("IP21\n");
		dump_ip21(brd);
		break;

	  case EVTYPE_IO4:
		printf("IO4\n");
		dump_io4(brd);
		break;

	  case EVTYPE_MC3:
		printf("MC3\n");
		dump_mc3(brd);
		break;

	  case EVTYPE_EMPTY:
		printf("EMPTY\n");
		dump_general(brd);
		break;
	  default:
		printf("Unrecognized board type\n");
		dump_general(brd);
		break;
	}
} 

extern char     *atob();

dispmc3(int argc, char **argv)
{
    int 	slot;

    if (argc != 2){
	msg_printf(ERR,"dmc:Invalid number of parameters\n\tdmc slot\n");
	return(1);
    }

    if (atob(argv[1], &slot) == (char *)0) {
	msg_printf(ERR, "invalid slot number\n");
	return(1);
    }

    msg_printf(DBG,"Dump MC3 info slot:%d\n",slot);

    if (slot <=0 || slot >= EV_MAX_SLOTS){
	msg_printf(ERR,"Invalid slot number: %s\n", argv[1]);
	return(1);
    }

    if (board_type(slot) != EVTYPE_MC3){
	msg_printf(ERR,"slot %d does not have a MC3 installed");
	return (1);
    }

    setup_err_intr(slot, 0);
    dump_evconfig_entry(slot);
    clear_err_intr(slot, 0);

    return(0);
}

dispio4(int argc, char **argv)
{
    int 	slot;

    if (argc != 2){
	msg_printf(ERR,"dio:Invalid number of parameters\n\tdio slot\n");
	return(1);
    }

    if (atob(argv[1], &slot) == (char *)0) {
	msg_printf(ERR, "invalid slot number\n");
	return(1);
    }

    msg_printf(DBG,"Dump IO4 info slot:%d\n",slot);

    if (slot <=0 || slot >= EV_MAX_SLOTS){
	msg_printf(ERR,"Invalid slot number: %s\n", argv[1]);
	return(1);
    }

    if (board_type(slot) != EVTYPE_IO4){
	msg_printf(ERR,"slot %d does not have an IO4 installed");
	return (1);
    }

    setup_err_intr(slot, 0);
    dump_evconfig_entry(slot);
    clear_err_intr(slot, 0);

    return(0);
}

dispip19(int argc, char **argv)
{
    int 	slot;

    if (argc != 2){
	msg_printf(ERR,"dip:Invalid number of parameters\n\tdip slot\n");
	return(1);
    }

    if (atob(argv[1], &slot) == (char *)0) {
	msg_printf(ERR, "invalid slot number\n");
	return(1);
    }

    msg_printf(DBG,"Dump IP19 info slot:%d\n",slot);

    if (slot <=0 || slot >= EV_MAX_SLOTS){
	msg_printf(ERR,"Invalid slot number: %s\n", argv[1]);
	return(1);
    }

    if (board_type(slot) != EVTYPE_IP19){
	msg_printf(ERR,"slot %d does not have an IP19 installed");
	return (1);
    }

    setup_err_intr(slot, 0);
    dump_evconfig_entry(slot);
    clear_err_intr(slot, 0);

    return(0);
}
