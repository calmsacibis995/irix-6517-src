#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_tlb.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include "icrash.h"
#include "extern.h"

/*
 * tlb_banner()
 */
void
tlb_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		if (K_IP(K) == 21 || K_IP(K) == 26) {
			fprintf(ofp, 
				"TLB             VADDR     PID                "
				" PGI       "
				"PFN (C D V G)\n");
		}
		else
		{
			fprintf(ofp, 
				"TLB             VADDR     ENTRY              "
				" PGI       "
				"PFN (C D V G)\n");
		}
	}

	if (flags & SMAJOR) {
		fprintf(ofp, 
		"========================================================"
			"=============\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp, 
		"--------------------------------------------------------"
			"-------------\n");
	}
}

/*
 * print_tlb()
 */
void
print_tlb(int slot, k_ptr_t tlbp, FILE *ofp)
{
	int i, pte2, cc, g, d, sv;
	kaddr_t vaddr;
	k_ptr_t pte;
	k_uint_t pgi, value;

	if (DEBUG(DC_GLOBAL, 4)) {
		fprintf(ofp, "print_tlb: slot=%d, tlbp=0x%x\n", slot, tlbp);
	}

	if (K_IP(K) == 21 || K_IP(K) == 26) {
		int pid;
		k_uint_t tlbhi;
		k_ptr_t t = tlbp;
		int xormask;

		for (i = 0; i < 3; i++) {

			/* Get the virtual address
			 */
			tlbhi = *(kaddr_t*)((uint)t);

			/* Get the pgi for the first mapped page. 
			 */	
			pte = (k_ptr_t)((uint)t + K_REGSZ(K) + 4);

			if (DEBUG(DC_PAGE, 1)) {
				fprintf(ofp, 
					"print_tlb: pte=0x%x, pgi=0x%llx\n",
					pte, pgi);
			}

		        /* 
			 * Below calculation for vaddr comes from 
			 * symmon/commands.c 
			 */
			pid  = (tlbhi & 0xff0) >> 4;
			vaddr = tlbhi & 0xffffffffffffc000;
			xormask = ((tlbhi & K_K2BASE(K)) != K_K2BASE(K)) ? 
				(pid & 0x7f) : 0;
			vaddr |= ((slot ^ xormask) << K_PNUMSHIFT(K));
			
			pgi  = kl_get_pgi(K, pte);
			if (K_IP(K) == 21)
			{
				pgi >>= 3;	       /* TLBLO_HWBITSHIFT */
			}
			else if (K_IP(K) == 26)
			{
				pgi >>= 6;	       /* TLBLO_HWBITSHIFT */
			}

			if (i == 0) {
				fprintf(ofp, 
					"%3d  %16llx      %2x    %16llx  ", 
					slot, vaddr, pid, pgi);
			}
			else {
				fprintf(ofp, 
					"     %16llx      %2x    %16llx  ",
					vaddr, pid, pgi);
			}
			fprintf(ofp, "%8d (%lld %lld %lld %lld)\n", 
				((PDE_SIZE(K) == 8) ? 
				 (int)kl_pte_to_pfn(K, (k_ptr_t)&pgi) :
				 (int)kl_pte_to_pfn(K, (k_ptr_t)
						    ((__psunsigned_t)&pgi+4))),
				(pgi & K_PDE_PG_CC(K)) >> K_PG_CC_SHIFT(K),
				(pgi & K_PDE_PG_D(K))  >> K_PG_D_SHIFT(K),
				(pgi & K_PDE_PG_VR(K)) >> K_PG_VR_SHIFT(K),
				(pgi & K_PDE_PG_G(K))  >> K_PG_G_SHIFT(K));
			
			t = (k_ptr_t)((uint)t + (2 * K_REGSZ(K)));
		}
	}
	else {

		vaddr = *(kaddr_t*)((uint)tlbp);

		if (DEBUG(DC_PAGE, 1)) {
			fprintf(ofp, "print_tlb: addr=0x%llx\n", vaddr);
		}

		/* Get the pgi for the first mapped page. Note that the pgi in 
		 * memory is actually a copy of the TLB. Since the tlb contains
		 * only hardware bits, it may be necessary to left shift the 
		 * value for macros to work properly (for example with IP19 
		 * systems).
		 *
		 * The shift right by 32 for PDE_SIZE == 8 is because of the 
		 * fact that they do a sw of the tlblo entry in "tlbgetent"
		 * even on platforms with 64-bit pte's. Ideally they should 
		 * do a sd.
		 */
		pte = (k_ptr_t)((__psunsigned_t)tlbp + K_REGSZ(K));
		pgi = kl_get_pgi(K, pte);
		
		if (K_IP(K) == 19) {
			pgi <<= 3;		       /* TLBLO_HWBITSHIFT */
		}
		if (PDE_SIZE(K) == 8)
		{
			pgi >>= 32;
			fprintf(ofp, "%3d  %16llx  EntryLo0  %16llx  ", 
				slot, vaddr, pgi);
			fprintf(ofp, "%8ld ", 
				(int)kl_pte_to_pfn(K, (k_ptr_t)&pgi));
		}
		else
		{
			fprintf(ofp, "%3d  %16llx  EntryLo0  %16llx  ", 
				slot, vaddr, pgi);
			fprintf(ofp, "%8ld ", 
				(int)kl_pte_to_pfn(K, (k_ptr_t)pte));
		}
		if (DEBUG(DC_PAGE, 1)) {
			fprintf(ofp, "print_tlb: pgi=0x%llx (0x%x)\n", 
					pgi, kl_pte_to_pfn(K, pte));
		}
		fprintf(ofp, "(%lld %lld %lld %lld)\n", 
			(pgi & K_PDE_PG_CC(K)) >> K_PG_CC_SHIFT(K),
			(pgi & K_PDE_PG_D(K)) >> K_PG_D_SHIFT(K),
			(pgi & K_PDE_PG_VR(K)) >> K_PG_VR_SHIFT(K),
			(pgi & K_PDE_PG_G(K)) >> K_PG_G_SHIFT(K));
			

		/* Now do the same thing for the second mapped page...
		 */
		pte = (k_ptr_t)((__psunsigned_t)tlbp + K_REGSZ(K) + 
				PDE_SIZE(K));
		pgi = kl_get_pgi(K, pte);
		
		if (K_IP(K) == 19) {
			pgi <<= 3;		       /* TLBLO_HWBITSHIFT */
		}
		if (PDE_SIZE(K) == 8)
		{
			pgi >>= 32;
			fprintf(ofp, 
				"                       EntryLo1  %16llx  ", 
				pgi);
			fprintf(ofp, 
				"%8ld ", (int)kl_pte_to_pfn(K, (k_ptr_t)&pgi));
		}
		else
		{
			fprintf(ofp, 
				"                       EntryLo1  %16llx  ", 
				pgi);
			fprintf(ofp, 
				"%8ld ", (int)kl_pte_to_pfn(K, pte));
		}
		fprintf(ofp, "(%lld %lld %lld %lld)\n", 
			(pgi & K_PDE_PG_CC(K)) >> K_PG_CC_SHIFT(K),
			(pgi & K_PDE_PG_D(K)) >> K_PG_D_SHIFT(K),
			(pgi & K_PDE_PG_VR(K)) >> K_PG_VR_SHIFT(K),
			(pgi & K_PDE_PG_G(K)) >> K_PG_G_SHIFT(K));
	}
}

/*
 * tlb_cmd() -- Run the 'tlb' command.
 */
int
tlb_cmd(command_t cmd)
{
	int i, j, k;
	k_uint_t value;
	cpuid_t cpuid;
	uint *tlbp;
	kaddr_t addr;
	k_ptr_t pdap;

	pdap = alloc_block(PDA_S_SIZE(K), B_TEMP);
	for (i = 0; i < cmd.nargs; i++) {
		
		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			kl_print_error(K);
			continue;
		}

		cpuid = (cpuid_t)value;
		if ((cpuid < 0) || (cpuid >= K_MAXCPUS(K))) {
			KL_SET_ERROR_NVAL(KLE_BAD_CPUID, cpuid, 0);
			kl_print_error(K);
			continue;
		}
		else {
			kl_get_pda_s(K, (kaddr_t)cpuid, pdap);
			if (KL_ERROR) {
				if (KL_ERROR == KLE_NO_CPU) {
					fprintf(cmd.ofp, "\nCPU %d NOT INSTALLED\n", cpuid);
				}
				else {
					kl_print_error(K);
				}
				continue;
			}
		}
		fprintf (cmd.ofp, "\nTLB DUMP FOR CPU %d:\n\n", cpuid);
		tlb_banner(cmd.ofp, BANNER|SMAJOR);

		tlbp = (uint*) ((uint)tlbdumptbl + ((cpuid * K_TLBDUMPSIZE(K)) + 8));
		for (j = 0, k = 0; j < 
						K_NTLBENTRIES(K); j++, k += K_TLBENTRYSZ(K)) {
			print_tlb(j, (k_ptr_t)((uint)tlbp + k), cmd.ofp);
		}
		tlb_banner(cmd.ofp, SMAJOR);
	}

	if (!cmd.nargs) {
		for (i = 0; i < K_MAXCPUS(K); i++) {
			kl_get_pda_s(K, (kaddr_t)i, pdap);
			if (KL_ERROR) {
				if (KL_ERROR == KLE_NO_CPU) {
					fprintf(cmd.ofp, "\nCPU %d NOT INSTALLED\n", i);
				}
				else {
					kl_print_error(K);
				}
				continue;
			}
			fprintf (cmd.ofp, "\nTLB DUMP FOR CPU %d:\n\n", i);
			tlb_banner(cmd.ofp, BANNER|SMAJOR);
			tlbp = (uint*) ((uint)tlbdumptbl + ((i * K_TLBDUMPSIZE(K)) + 8));
			for (j = 0, k = 0; j < 
							K_NTLBENTRIES(K); j++, k += K_TLBENTRYSZ(K)) {
				print_tlb(j, (k_ptr_t)((uint)tlbp + k), cmd.ofp);
			}
			tlb_banner(cmd.ofp, SMAJOR);
		}
	}
	free_block(pdap);
	return(0);
}

#define _TLB_USAGE "[-w outfile] [cpu_list]"

/*
 * tlb_usage() -- Print the usage string for the 'tlb' command.
 */
void
tlb_usage(command_t cmd)
{
	CMD_USAGE(cmd, _TLB_USAGE);
}

/*
 * tlb_help() -- Print the help information for the 'tlb' command.
 */
void
tlb_help(command_t cmd)
{
	CMD_HELP(cmd, _TLB_USAGE,
		"Display TLB information for each CPU indicated in cpu_list.  If "
		"no CPUs are indicated, TLB information for all CPUs will be "
		"displayed.");
}

/*
 * tlb_parse() -- Parse the command line arguments for 'tlb'.
 */
int
tlb_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE);
}
