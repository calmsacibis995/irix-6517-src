#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/RCS/init.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define _KERNEL  1
#define _PAGESZ  16384
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/immu.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/tcpipstats.h>
#include <sys/pcb.h>
#include <sys/utsname.h>
#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "klib.h"
#include "variable.h"
#include "listvec.h"

/* Global variables
 */
symtab_t *stp;                  /* symbol table pointer                      */
int symbol_table;               /* mdebug/dwarf                              */
struct mbufconst mbufconst;		/* Sizes of mbuf constants                   */
vtab_t *vtab;					/* Pointer to table of eval variable info	 */

/* Pointer to libklib control structure
 */
klib_t *K;

/* Global variables containing information about the dump
 */
int nfstype;                    /* number of vfs's in vfssw[]                */
int mbpages = -1;				/* Number of mbuf pages in use			     */

/* Addresses of key kernel symbols (variables) are obtained from the 
 * appropriate syment struct. Note that if a symbol is actually a pointer
 * type variable, we don't allocate space for the symbol address because
 * it is not referenced anywhere in the code.  Generally speaking, the 
 * variable name is the name of the symbol with an "S_" prefix.
 */
kaddr_t S_mbpages;        		/* Number of cluster pages in use            */
kaddr_t S_strdzone;             /* Streams zone memory 		                 */

/* The following global variables contain kernel pointers to various 
 * structures, lists, tables, buffers, etc. Getting such a pointer is
 * a two-step process. The syment struct for the symbol is located
 * first. Then, the address of the symbol is used to access the actual
 * pointer value. Generally speaking, they variable name is the name 
 * of the symbol with an "_" prefix.
 */
kaddr_t _hbuf;                  /* buf hash table                            */

/* Pointers to various memory blocks allocated during icrash startup.
 * These blocks have been allocated using the B_PERM flag, which makes
 * them permanent. Throughout the run of the program.
 */
k_ptr_t strstp = 0;				/* Streams statas struct                     */
k_ptr_t tlbdumptbl; 			/* tlb dump table                            */
k_ptr_t _vfssw;                 /* vfs switch table                          */

/* Macros for geting addresses of symbols and pointer values
 */
#define GET_SYM(symp, s) \
	symp = get_sym(s, B_TEMP); \
	if (KL_ERROR) { \
		fprintf(KL_ERRORFP, "icrash: %s not found in symbol table\n", s); \
		exit(1); \
	}

#define GET_SYM_ADDR(k, s) \
	k = get_sym_addr(s); \
	if (KL_ERROR) { \
		fprintf(KL_ERRORFP, "icrash: %s not found in symbol table\n", s); \
		exit(1); \
	}

#define INIT_UPDATE(fp, s) \
	if (klib_debug) { \
		fprintf (fp, "%s\n", s); \
	} \
	else if (!report_flag && !suppress_flag && (prog != FRU_PROGRAM)) { \
		fprintf(fp, "."); \
		fflush(fp); \
	}

/*
 * symbols_init()
 */
void
symbols_init()
{
	if ((stp = init_symlib(namelist)) == (symtab_t *)NULL) {
		fprintf(KL_ERRORFP, "\nicrash: Could not initialize symlib!\n");
		exit(1);
	}
	symbol_table = DWARF_TYPE_FLAG;
}

/*
 * init() -- Initialize everything.
 */
void
init(FILE *ofp)
{
	int flags=0;

	struct syment *symp;	/* temp syment pointer */

	/* Initialize the memory pool 
	 */
	INIT_UPDATE(ofp, "Initializing memory pool");
	init_mempool();

	INIT_UPDATE(ofp, "libklib setup");

	if (ignoreos) {
		flags = K_IGNORE_FLAG;
	}
	if (icrashdef_flag) {
		flags |= K_ICRASHDEF_FLAG;
	}

	K = klib_open(namelist, corefile, program, icrashdef, O_RDWR, flags);
	if (KL_ERROR == KLE_SHORT_DUMP) {
		/* In this case, we need to save the availmon report, and
		 * at the very least dump out the "dump header" for the caller
		 * of 'icrash'.
		 */
		if (report_flag && (availmon)) {
			availmon_report(ofp);
			exit(dump_header_report(ofp));
		}
		else {
			fprintf(KL_ERRORFP, 
				"\nWARNING: Compressed vmcore has no real data pages.  "
				"Use the command\n" 
				"         '/etc/uncompvm -h %s' to print out any available\n"
				"         vmcore information.\n", corefile);
		}
		exit(1);
	}
	if (!K) {
		fprintf(stderr, "Could not initialize libklib!\n");
		exit(1);
	}
	if (add_callbacks(K)) {
		fprintf(KL_ERRORFP, "Could not add callback functions!\n");
		exit(1);
	}

	INIT_UPDATE(ofp, "Initializing symbol information");
	symbols_init();

	INIT_UPDATE(ofp, "Initializing klib information");
	if (klib_init(K)) {
		fprintf(KL_ERRORFP, "\nCould not initialize klib!\n");
		exit(1);
	}

	/* Check to make sure that we are trying to analyzing a 6.5
	 * system or a dump from a 6.5 system (unless the undocumented
	 * -i flag is used and then all bets are off).
	 */
	if (K_IRIX_REV(K) != IRIX6_5) {
		if (!ignoreos || (K_IRIX_REV(K) != IRIX_SPECIAL)) {
			fprintf(KL_ERRORFP, "\nNOTICE: This version of icrash will only "
				"work on a live system running\n");
			fprintf(KL_ERRORFP, "        IRIX 6.5 or with a system dump "
				"from a 6.5 system.\n");
			exit(1);
		}
	}

	/* Check to see if system is active
	 */
	if (ACTIVE(K)) {
		if (prog == FRU_PROGRAM) {
			fatal("fru cannot be run against active systems!\n");
		}
		else if (report_flag) {
			fatal("cannot generate a report against a live system!\n");
		}
	}

	INIT_UPDATE(ofp, "Initializing common kernel variables");
	init_global_vars(ofp);

	GET_SYM(symp, "tlbdumptbl");
	tlbdumptbl = alloc_block(K_MAXCPUS(K) * K_TLBDUMPSIZE(K), B_PERM); 
	kl_get_block(K, kl_kaddr_to_ptr(K, symp->n_value), 
		(K_MAXCPUS(K) * K_TLBDUMPSIZE(K)), tlbdumptbl, "tlbdumptbl");
	free_sym(symp);

	if (prog == FRU_PROGRAM) {
		if (K_IP(K) < 0) {
			fatal("could not determine IP type!\n");
		} 
		else if ((K_IP(K) != 19) && (K_IP(K) != 21) && (K_IP(K) != 25)) {
			fatal("fru only applies to IP19, IP21, and IP25 systems!\n");
		}
	}

	vtab = (vtab_t*)alloc_block(sizeof(vtab_t), B_PERM);
	init_variables(vtab);

	/* Adjust the size for long, signed long, and unsigned long
	 * entries in the base_type[] array if this is a 32-bit system.
	 */
	if (K_NBPW(K) == 4) {

		int i = 0;
		extern base_t base_type[];

		while(base_type[i].name) {
			if (!strcmp(base_type[i].name, "long") ||
						!strcmp(base_type[i].name, "signed long") ||
						!strcmp(base_type[i].name, "unsigned long")) {
				base_type[i].byte_size = 4;
			}
			i++;
		}
	}

	/* Initialize the kernel struct table (used by the walk command).
	 */
	(void)init_struct_table();

	switch (prog) {

		case ICRASH_PROGRAM:

			/* Set up history table 
			 */
			using_history();
			stifle_history(MAX_HISTORY);
			break;

		case FRU_PROGRAM:
			break;
	}
}

/*
 * init_global_vars()
 */
void
init_global_vars(FILE *ofp)
{
	k_ptr_t tbp;			/* temp block pointer  */
	struct syment *symp;	/* temp syment pointer */

	/* XXX - In the past, icrash would fail if there was any problem
	 * with the initialization of any of the global variables in this
	 * function. Since all critical global variables have been moved
	 * to libklib, it no longer makes sense to cause icrash to fail if
	 * a problem occurs. Note, however, that ignoring a failure here
	 * does not eliminate the possibility of a problem occurring in
	 * another part of the program.
	 */ 

	if (!(S_mbpages = get_sym_addr("mbpages"))) {
		fprintf(KL_ERRORFP, "\nicrash: mbpages not found in symbol table\n");
	}

	if (!(S_strdzone = get_sym_addr("strdzone"))) {
		if (DEBUG(DC_GLOBAL, 1)) {
			fprintf(KL_ERRORFP, 
				"\nicrash: strdzone not found in symbol table\n");
		}
	}

	if (symp = get_sym("_mbufconst", B_TEMP)) {
		kl_get_struct(K, symp->n_value, sizeof(struct mbufconst), 
				&mbufconst, "mbufconst");
		free_sym(symp);
	}
	else {
		fprintf(KL_ERRORFP, "\nicrash: _mbufconst not found in symbol table\n");
	}

	if (symp = get_sym("nfstype", B_TEMP)) {

		kl_get_block(K, symp->n_value, 4, &nfstype, "nfstype");
		free_sym(symp);

		if (symp = get_sym("vfssw", B_TEMP)) {
			if (!(_vfssw = alloc_block((nfstype * STRUCT("vfssw")), B_PERM))) {
				fprintf(KL_ERRORFP, 
					"NOTICE: could not allocate space for vfssw[]!\n");
			}
			else {
				kl_get_block(K, symp->n_value, 
					(nfstype * STRUCT("vfssw")), _vfssw, "vfssw");
			}
			free_sym(symp);
		}
		else {
			fprintf(KL_ERRORFP, "\nicrash: vfssw not found in symbol table\n");
		}
	}
	else {
		fprintf(KL_ERRORFP, "\nicrash: nfstype not found in symbol table\n");
	}
}
