/***********************************************************************\
*       File:           disp_evcfg.c                                    *
*                                                                       *
\***********************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/IP21.h>
#include <sys/EVEREST/evconfig.h>
#include "pod_failure.h"
#include "prom_externs.h"

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
	loprintf("   Rev: %d\tInventory: 0x%x    Diag Value: 0x%x, %s\n",
		rev, brd->eb_inventory, brd->eb_diagval,
		(brd->eb_enabled ? "Enabled" : "Disabled"));
	if ((brd->eb_diagval != EVDIAG_PASSED) &&
					(brd->eb_diagval != EVDIAG_NOT_PRESENT))
		loprintf("  *** %s\n", get_diag_string(brd->eb_diagval));
}

 
static void 
dump_ip(evbrdinfo_t *brd)
{
	int i;
	evcpucfg_t *cpu;

	dump_general(brd);

	for (i = 0; i < EV_CPU_PER_BOARD; i++) {
		cpu = &(brd->eb_cpuarr[i]);
		if (cpu->cpu_diagval != EVDIAG_NOTFOUND) {
			loprintf("        CPU %d: Inventory 0x%b, DiagVal 0x%b, Info 0x%b\n",
		      		i, cpu->cpu_inventory, cpu->cpu_diagval, 
				cpu->cpu_info);
			if (cpu->cpu_diagval != EVDIAG_PASSED)
				loprintf("        *** %s\n",
					get_diag_string(cpu->cpu_diagval));
			loprintf("               Virt. #%d, Speed %d MHz, Cache Size %d kB, Prom rev %d, %s\n",
				cpu->cpu_vpid, cpu->cpu_speed, 
				1 << (cpu->cpu_cachesz - 10),
				(cpu->cpu_promrev),
				(cpu->cpu_enable ? "Enabled" : "Disabled"));
		} else {
			loprintf("        CPU %d: Not populated\n", i);
		}
	} 
}

static void 
dump_io4(evbrdinfo_t *brd)
{
	int i;
	evioacfg_t *ioa;

	dump_general(brd);
	loprintf("  Window Number: %d\n", brd->eb_io.eb_winnum);

	for (i = 1; i < IO4_MAX_PADAPS; i++) {
	
		ioa = &(brd->eb_ioarr[i]);

		loprintf("        PADAP %d: ", i);
		switch(ioa->ioa_type) {
		  case IO4_ADAP_EPC:
			loprintf("EPC");
			break;
		  case IO4_ADAP_SCSI:
			loprintf("S1 ");
			break;
		  case IO4_ADAP_FCHIP:
			loprintf("F  ");
			break;
		  case 0:
			loprintf("Not populated.\n");
			break;
		  default:
			loprintf("???");
			break;
		}

		loprintf(" (0x%b), Inventory 0x%b, DiagVal 0x%b, VirtID %d, %s\n",
			ioa->ioa_type, ioa->ioa_inventory, 
			ioa->ioa_diagval, ioa->ioa_virtid, 
			(ioa->ioa_enable ? "Enabled" : "Disabled"));
			if (ioa->ioa_diagval != EVDIAG_PASSED)
				loprintf("        *** %s\n",
					get_diag_string(ioa->ioa_diagval));
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
			loprintf("        Bank %d: IP %d, IF %d, SIMM type %d, Bloc 0x%x\n",
				i, mem->bnk_ip, mem->bnk_if, mem->bnk_size, 
				mem->bnk_bloc);
			loprintf("                Inventory 0x%b, DiagVal 0x%b, %s\n",
				mem->bnk_inventory, mem->bnk_diagval,
				(mem->bnk_enable ? "Enabled" : "Disabled"));
			if (mem->bnk_diagval != EVDIAG_PASSED)
				loprintf("        *** %s\n",
					get_diag_string(mem->bnk_diagval));
		} else {
			loprintf("        Bank %d: Not populated.\n",
				i);	
		}
	}
}


void
dump_evconfig_entry(int slot)
{
	evbrdinfo_t *brd;

	brd = &(EVCFGINFO->ecfg_board[slot]);

	loprintf("Slot 0x%b: Type = 0x%b, Name = ", slot, brd->eb_type);
	switch(brd->eb_type) {
	  case EVTYPE_IP19:
		loprintf("IP19\n");
		dump_ip(brd);
		break;
	  case EVTYPE_IP21:
		loprintf("IP21\n");
		dump_ip(brd);
		break;

	  case EVTYPE_IO4:
		loprintf("IO4\n");
		dump_io4(brd);
		break;

	  case EVTYPE_MC3:
		loprintf("MC3\n");
		dump_mc3(brd);
		break;

	  case EVTYPE_EMPTY:
		loprintf("EMPTY\n");
		dump_general(brd);
		break;
	  default:
		loprintf("Unrecognized board type\n");
		dump_general(brd);
		break;
	}
} 
