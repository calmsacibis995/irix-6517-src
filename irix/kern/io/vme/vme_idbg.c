/*
 * Copyright 1996-1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure. 
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/*
 *  VMEbus idbg functions
 */

#ident	"$Revision: 1.3 $"

#include <sys/types.h>
#include <ksys/xthread.h>
#include <sys/proc.h>               /* proc_t                            */
#include <sys/dmamap.h>
#include <sys/hwgraph.h>
#include <sys/vmereg.h> 
#include <sys/PCI/bridge.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/pcibr.h>
#include <sys/idbg.h>               /* qprintf()                         */
#include <sys/idbgentry.h>          /* idbg_addfunc()                    */
#include <sys/vme/vmeio.h>         
#include <sys/vme/universe.h>
#include "vmeio_private.h"
#include "universe_private.h"
#include "ude_private.h"
#include "usrvme_private.h"

extern void idbg_vme_universe_state(universe_state_t);
extern void idbg_vme_universe_piomap(universe_piomap_t);
static void universe_piomaps_print(universe_piomap_list_t);
extern void idbg_vme_usrvme_piomap_list(usrvme_piomap_list_t);
static void idbg_vme_vmeio_piomap(vmeio_piomap_t);
extern void idbg_vme_universe_chip(universe_chip_t *); 

/* 
 * Table of VMEbus idbg functions 
 */
static struct vme_idbg_funcs_s {
	char     *name;
	void     (*func)(int);
	char     *help;
} vme_idbg_funcs[] = {
	"universe_state",   (void (*)()) idbg_vme_universe_state, "universe_state <handle>",
	"universe_piomap", (void (*)()) idbg_vme_universe_piomap, "universe_piomap <handle>",
	"usrvme_piomap_list", (void (*)()) idbg_vme_usrvme_piomap_list, "usrvme_piomap_list <handle>",
	"universe_chip", (void (*)()) idbg_vme_universe_chip, "universe_chip <handle>",
	0, 0, 0
};

/*
 * Initialize the VME idbg module
 */
void
vme_idbg_init(void)
{
	struct vme_idbg_funcs_s *func_entry;
	
	for (func_entry = vme_idbg_funcs; func_entry->func; func_entry++) {
		idbg_addfunc(func_entry->name, (void (*)())(func_entry->func));
	}
}

void
idbg_vme_universe_state(universe_state_t universe_state)
{
	universe_pcisimg_num_t img_idx;
	vmeio_intr_level_t     lvl;

	qprintf("Universe State @ 0x%x\n", universe_state);
	qprintf("\t pci connection point 0x%x (%v)\n",
		universe_state->pci_conn, universe_state->pci_conn);
	qprintf("\t universe vertex 0x%x (%v)\n", 
		universe_state->vertex, universe_state->vertex);
	qprintf("\t chip @ 0x%x\n", universe_state->chip);

	for (img_idx = 0; img_idx < UNIVERSE_NUM_OF_PCISIMGS; img_idx++) {
		qprintf("\t PCI slave image (%d):\n", img_idx);
		qprintf("\t\t PCIbus PIO map 0x%x, users 0x%x, am 0x%x\n", 
			universe_state->pcisimgs_fixed[img_idx].pci_piomap,
			universe_state->pcisimgs_fixed[img_idx].users,
			universe_state->pcisimgs_fixed[img_idx].am);
		qprintf("\t\t PCIbus base addr 0x%x, PCIbus bound addr 0x%x\n",
			universe_state->pcisimgs_fixed[img_idx].pci_base,
			universe_state->pcisimgs_fixed[img_idx].pci_bound);
		qprintf("\t\t Translation offset 0x%x, Virtual addr 0x%x\n",
			universe_state->pcisimgs_fixed[img_idx].to,
			universe_state->pcisimgs_fixed[img_idx].kv_base);
	}
	qprintf("\t PCI slave image special:\n");
	qprintf("\t\t PCIbus base addr 0x%x\n", 
		universe_state->pcisimg_spec_state.pci_base);
	qprintf("\t\t kernel base addr 0x%x\n", 
		universe_state->pcisimg_spec_state.kv_base);

	universe_piomaps_print(universe_state->piomaps);

	qprintf("\t DMA:\n");
	for (img_idx = 0; img_idx < UNIVERSE_NUM_OF_VMESIMGS; img_idx++) {
		qprintf("\t VME slave image (%d):\n", img_idx);
		qprintf("\t\t users 0x%x, am 0x%x\n", 
			universe_state->vmesimgs[img_idx].users,
			universe_state->vmesimgs[img_idx].to);
	}

	qprintf("\t INTR:\n");
	for (lvl = VMEIO_INTR_LEVEL_1; 
	     lvl <= VMEIO_INTR_LEVEL_7;
	     lvl++) {
		qprintf("\t\t Level %d: users %d cpu 0x%x\n",
			lvl,
			universe_state->vmebus_intrs[lvl].users,
			universe_state->vmebus_intrs[lvl].cpu);
	}

	qprintf("\t DMA engine:\n");
	qprintf("\t\t linked list command packets @ 0x%x\n", 
		universe_state->dma_engine.ll_kvaddr);
}


static void 
universe_piomaps_print(universe_piomap_list_t piomaps)
{
	universe_piomap_list_item_t  item;

	qprintf("\tpiomaps\n");

	item = piomaps->head;
	if (item == 0) {
		qprintf("\t\tno items\n");
	}
	else {
		do {
			qprintf("\t\tpiomap @ 0x%x\n", item->piomap);
			idbg_vme_universe_piomap(item->piomap);
			item = item->next;
		} while (item != 0);
	}
}

static struct reg_desc	universe_lint_desc[] =
{
	{ UNIVERSE_LINT_ACFAIL, 0, "ACFAIL" },
	{ UNIVERSE_LINT_SYSFAIL, 0, "SYSFAIL" },
	{ UNIVERSE_LINT_SW_INT, 0, "SW_INT" },
	{ UNIVERSE_LINT_SW_IACK, 0, "SW_IACK" },
	{ UNIVERSE_LINT_VERR, 0, "VERR" },
	{ UNIVERSE_LINT_LERR, 0, "LERR" },
	{ UNIVERSE_LINT_DMA, 0, "DMA" },
	{ UNIVERSE_LINT_VIRQ7, 0, "VIRQ7" },
	{ UNIVERSE_LINT_VIRQ6, 0, "VIRQ6" },
	{ UNIVERSE_LINT_VIRQ5, 0, "VIRQ5" },
	{ UNIVERSE_LINT_VIRQ4, 0, "VIRQ4" },
	{ UNIVERSE_LINT_VIRQ3, 0, "VIRQ3" },
	{ UNIVERSE_LINT_VIRQ2, 0, "VIRQ2" },
	{ UNIVERSE_LINT_VIRQ1, 0, "VIRQ1" },
	{ UNIVERSE_LINT_VOWN, 0, "VOWN" },
	{0}
};

static struct reg_desc	universe_pci_csr_desc[] =
{
	{ 0x80000000, 0, "Parity Error" },
	{ 0x40000000, 0, "SERR" },
	{ 0x20000000, 0, "Received Master-Abort" },
	{ 0x10000000, 0, "Received Target-Abort" },
	{ 0x08000000, 0, "Signaled Target-Abort" },
	{ 0x01000000, 0, "Data Parity Error" },
	{0}
};

void 
idbg_vme_universe_chip(universe_chip_t *universe_chip)
{
	universe_pcisimg_t *               pcisimg;
	universe_vmesimg_t *               vmesimg;
	universe_dma_engine_regs_t *       regs;
	universe_pcisimg_num_t	           img_idx;

	regs = (universe_dma_engine_regs_t *) 
		UNIVERSE_REG_ADDR(universe_chip, UNIVERSE_DMA_CTL);

	/* PIO related registers */
	qprintf("\t PIO resources:\n");
	for (img_idx = 0; img_idx < UNIVERSE_NUM_OF_PCISIMGS; img_idx++) {
		qprintf("\t\t Slave Image 0x%x:\n", img_idx);
		pcisimg = (universe_pcisimg_t *)
			UNIVERSE_REG_ADDR(universe_chip, 
					  universe_pcisimg_offset[img_idx]);
		
		qprintf("\t\t\t Control EN %x PWEN %x VDW %x VAS %x PGM %x SUPER %x\n", 
			pcisimg->ctl.en, 
			pcisimg->ctl.pwen, 
			pcisimg->ctl.vdw,
			pcisimg->ctl.vas, 
			pcisimg->ctl.pgm, 
			pcisimg->ctl.super);

		qprintf("\t\t\t\t VCT %x LAS %x\n", 
			pcisimg->ctl.vct, 
			pcisimg->ctl.las);
		qprintf("\t\t\t BASE 0x%x\n", pcisimg->bs);
		qprintf("\t\t\t BOUND 0x%x\n",pcisimg->bd);
		qprintf("\t\t\t Translation offset 0x%x\n", pcisimg->to);
	}

	qprintf("\t\t Special Slave Image:\n");
	qprintf("\t\t\t EN %x PWEN %x VDW3 %x VDW2 %x VDW1 %x VDW0 %x\n",
		universe_chip->pcisimg_spec.en,
		universe_chip->pcisimg_spec.pwen,
		universe_chip->pcisimg_spec.vdw3,
		universe_chip->pcisimg_spec.vdw2,
		universe_chip->pcisimg_spec.vdw1,
		universe_chip->pcisimg_spec.vdw0);
	qprintf("\t\t\t PGM3 %x PGM2 %x PGM1 %x PGM0 %x ",
		universe_chip->pcisimg_spec.pgm3,
		universe_chip->pcisimg_spec.pgm2,
		universe_chip->pcisimg_spec.pgm1,
		universe_chip->pcisimg_spec.pgm0);
	qprintf("\t\t\t SUPER3 %x SUPER2 %x SUPER1 %x SUPER0 %x\n",
		universe_chip->pcisimg_spec.super3,
		universe_chip->pcisimg_spec.super2,
		universe_chip->pcisimg_spec.super1,
		universe_chip->pcisimg_spec.super0);
	qprintf("\t\t\t BS %x LAS %x\n",
		universe_chip->pcisimg_spec.bs,
		universe_chip->pcisimg_spec.las);

	/* DMA registers */
	qprintf("\t DMA resources:\n");
	for (img_idx = 0; img_idx < UNIVERSE_NUM_OF_VMESIMGS; img_idx++) {
		qprintf("\t\t Slave Image 0x%x:\n", img_idx);
		vmesimg = (universe_vmesimg_t *)
			UNIVERSE_REG_ADDR(universe_chip,
					  universe_vmesimg_offset[img_idx]);
		qprintf("\t\t\t CONTROL EN %x PWEN %x PREN %x PGM %x SUPER %x\n", 
			vmesimg->ctl.en, 
			vmesimg->ctl.pwen, 
			vmesimg->ctl.pren,
			vmesimg->ctl.pgm, 
			vmesimg->ctl.super);
		qprintf("\t\t\t\t VAS %x LD64EN %x LLRMW %x LAS %x\n", 
			vmesimg->ctl.vas, 
			vmesimg->ctl.ld64en, 

			vmesimg->ctl.llrmw, 
			vmesimg->ctl.las);
		qprintf("\t\t\t BASE 0x%x\n", (__uint32_t)vmesimg->bs);
		qprintf("\t\t\t BOUND 0x%x\n", (__uint32_t)vmesimg->bd);
		qprintf("\t\t\t Translation offset 0x%x\n", 
			(__uint32_t)vmesimg->to);
	}

			
	/* DMA engine registers */
	qprintf("\t DMA engine:\n");
	qprintf("\t\t Transfer Control:\n");
	qprintf("\t\t\t L2V %x VDW %x VAS %x PGM %x SUPER %x VCT %x LD64EN %x\n", 
		regs->ctl.l2v, regs->ctl.vdw, regs->ctl.vas, regs->ctl.pgm,
		regs->ctl.super, regs->ctl.vct, regs->ctl.ld64en); 
	qprintf("\t\t Transfer Byte Count: 0x%x\n", regs->tbc);
	qprintf("\t\t PCIbus Address: 0x%x\n", regs->la);
	qprintf("\t\t VMEbus Address: 0x%x\n", regs->va);
	qprintf("\t\t Command Packet Pointer: 0x%x\n", 
		regs->cpp.addr_31_3 << UNIVERSE_DMA_CPP_ADDR_SHIFT);
	qprintf("\t\t General Control/Status:\n");
	qprintf("\t\t\t CHAIN %x ACT %x STOP %x HALT %x DONE %x\n",
		regs->gcs.chain, regs->gcs.act, regs->gcs.stop, 
		regs->gcs.halt, regs->gcs.done);
	qprintf("\t\t\t LERR %x VERR %x P_ERR %x\n",
		regs->gcs.lerr, regs->gcs.verr, regs->gcs.perr);
	qprintf("\t\t\t INT_STOP %x INT_HALT %x INT_DONE %x\n",
		regs->gcs.int_stop, regs->gcs.int_halt, regs->gcs.int_done);
	qprintf("\t\t\t INT_LERR %x INT_VERR %x INT_M_ERR %x\n",
		regs->gcs.int_lerr, regs->gcs.int_verr, regs->gcs.int_m_err);
	qprintf("\t\t Linked List Update Enable: %x\n", regs->llue.update);

	/* Interrupt */
	qprintf("\t Interrupt:\n");
	qprintf("\t\t Enable: %R\n", 
		universe_chip->lint_en, universe_lint_desc);
	qprintf("\t\t Status: %R\n", 
		universe_chip->lint_stat, universe_lint_desc);

	/* PCI CSR */
	qprintf("\t PCI CSR: %R\n", universe_chip->pci_csr, universe_pci_csr_desc);

	/* Error management */
	qprintf("\t VMEbus Error:\n");
	qprintf("\t\t AM Erorr Log: amerr %x iack %x m_err %x v_stat %x\n",
		universe_chip->v_amerr.amerr, universe_chip->v_amerr.iack,
		universe_chip->v_amerr.m_err, universe_chip->v_amerr.v_stat);
	qprintf("\t\t Address Error Log: 0x%x\n", universe_chip->v_aerr);
}

void
idbg_vme_universe_piomap(universe_piomap_t universe_piomap)
{
	qprintf("Universe PIO map @ 0x%x\n", universe_piomap);
	idbg_vme_vmeio_piomap(&(universe_piomap->vme_piomap));
	qprintf("\t image_num 0x%x pci_addr 0x%x pci_piomap 0x%x\n",
		universe_piomap->image_num, universe_piomap->pci_addr,
		universe_piomap->pci_piomap);
	qprintf("\t universe_state 0x%x\n", universe_piomap->universe_state);
}

void 
idbg_vme_vmeio_piomap(vmeio_piomap_t vmeio_piomap)
{
	qprintf("\t Generic VME PIO map @ 0x%x\n", vmeio_piomap);
	qprintf("\t\t dev: 0x%x am: 0x%x\n",
		vmeio_piomap->dev, vmeio_piomap->am);
	qprintf("\t\t vmeaddr: 0x%x mapsz: 0x%x kvaddr: 0x%x\n",
		vmeio_piomap->vmeaddr, 
		vmeio_piomap->mapsz, 
		vmeio_piomap->kvaddr);
	qprintf("\t\t flags: 0x%x\n", vmeio_piomap->flags);
}

void
idbg_vme_usrvme_piomap_list(usrvme_piomap_list_t list)
{
	usrvme_piomap_list_item_t item;

	for (item = list->head; item != 0; item = item->next) {
		printf("(uthread 0x%x piomap 0x%x uvaddr 0x%x kvaddr 0x%x)\n",
		       item->uthread, item->piomap, item->uvaddr, item->kvaddr);
	}
}
      








