/* NOTE: This guy will need to do some swizzling to make it work in either
	endianness */

#define R4000 1

#include <stdio.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/IP19addrs.h>
#include "niblet.h"
#include "cob.h"
#include "cp0_r4k.h"
#include "passfail.h"
#include "nib_procs.h"
#include "../pod.h"
#include "../pod_failure.h"
#include "../prom_intr.h"

extern int test_array[][PROCS_PER_TEST + 1];
extern int num_tests;
extern int avail_procs;

/* Section names */
char scn_names[NUM_SECTS][LABEL_LENGTH] = {
	".text",
	".data",
	".bss",
	"SHARED"
};

char reasons[NUM_NIBFAILS][22] = {
	"Test passed", 		/* 0 */	
	"Generic failure",	/* 1 */
	"Cache error",		/* 2 */
	"Unexpected exception",	/* 3 */
	"ERTOIP nonzero",	/* 4 */
	"Bad system call",	/* 5 */
	"Timeout",		/* 6 */
	"Process failed",	/* 7 */
	"Coherency failure",	/* 8 */
	"Bad page reference",	/* 9 */
	"Timer/Interrupt error",/* 10 */
	"Memory error",		/* 11 */
	"Unexpected interrupt",	/* 12 */
	"Wrong endianness",	/* 13 */
	"CPU didn't run"	/* 14 */
};

#define ZEROPROCESSES
/* Redefine ZEROPROCESSES later when we're running cached from RAM!...
 * This define tells dcob to zero Niblet process' BSS and SBSS sections.
 */

void proc_tbl(struct obj_inf *, int, unsigned int *, int);
void prg_code(struct obj_inf *, int, unsigned int *, int);
void pg_tbl(struct obj_inf *, int, unsigned int *, int, int);
static void page_align(unsigned int *);
int dcob(struct obj_inf *, int *, unsigned int, unsigned int *,
		int, int, int);

extern struct obj_inf static_table[];
extern unsigned int niblet_code[];
extern unsigned int nib_exc_start, nib_exc_end;
extern unsigned int nib_data_start, nib_data_end;
extern unsigned int nib_text_start, nib_text_end;
extern unsigned int nib_bss_start, nib_bss_end;
extern unsigned int nib_sbss_start, nib_sbss_end;
extern unsigned int nib_text_entry;
extern unsigned int slave_return;
extern unsigned int nib_text_off, nib_data_off, nib_exc_off;
extern void pon_zero_icache(uint);
extern void pon_invalidate_icache();
extern void pon_flush_dcache();
extern void pon_flush_scache();
extern void pon_validate_scache();
extern void pon_flush_and_inval();



/* #define DEBUG */

#define printf	loprintf


unsigned int *dest_addr(unsigned int original_addr)
{

#ifdef CACHED
	return (unsigned int *)((original_addr & 0x1fffffff) | 0x80000000);
#else
	return (unsigned int *)((original_addr & 0x1fffffff) | 0xa0000000);
#endif
}

int dcob(struct obj_inf *static_table, int *nib_list,
		unsigned int start_addr, unsigned int *proc_tbl_addr,
		int quantum, int tcoher, int dcoher)
{
	int i, j, k;
	struct obj_inf *dynamic_table;
	unsigned int cur_page;
	int swizzle = 0;
	int num_procs = 0;

	loprintf("Start address is %x\n", start_addr);

#ifdef DEBUG
	loprintf("%d processes available\n", avail_procs);
#endif
	dynamic_table = (struct obj_inf *)start_addr;

	while (nib_list[num_procs] != -1) {
		if ((nib_list[num_procs] >= avail_procs) ||
						(nib_list[num_procs] < -1))
			return 1;
		else
			dynamic_table[num_procs] =
					(static_table[nib_list[num_procs]]);
		/* Copy over the pointers to the info */
#ifdef DEBUG
		loprintf("Adding niblet process %d\n", nib_list[num_procs]);
#endif
		num_procs++;
	}

	cur_page = start_addr + (sizeof(struct obj_inf) * num_procs);
	page_align(&cur_page);

#ifdef DEBUG
	for (i = 0; i < num_procs; i ++) {
		printf("file: %s\n", dynamic_table[i].name);
		printf("--------------------------------\n");
		for (j = 0; j < NUM_SECTS; j++) {
			printf("Section: %s\n", scn_names[j]);
			printf("Size: %d\n", dynamic_table[i].scn_size[j]);
 			printf("Addr: 0x%x\n", dynamic_table[i].scn_addr[j]);
			printf("End: 0x%x\n", dynamic_table[i].scn_end[j]);
			printf("Ptr: 0x%x\n", dynamic_table[i].scn_ptr[j]);
		}
		printf("Text starts with:\n");
		for (k = 0; k < 8; k++) {
			printf("%x\t", dynamic_table[i].scn_ptr[TEXT_S][k]);
			if ((k % 4) == 3)
				printf("\n");
		}
	}
#endif
	loprintf("Generating code...\r");

	prg_code(dynamic_table, num_procs, &cur_page, swizzle); 

	loprintf("Generating page tables...                  \r");

 	pg_tbl(dynamic_table, num_procs, &cur_page, dcoher, tcoher);

	page_align(&cur_page);

	loprintf("Generating process table...\r");

	*proc_tbl_addr = cur_page;
	proc_tbl(dynamic_table, num_procs, &cur_page, quantum);

#ifdef DEBUG
	for (i = 0; i < num_procs; i ++) {
		printf("file: %s\n", dynamic_table[i].name);
		printf("--------------------------------\n");
		for (j = 0; j < NUM_SECTS; j++) {
			printf("Section: %s\n", scn_names[j]);
			printf("Real_start: 0x%x\n", dynamic_table[i].start_addr[j]);
 			printf("Real_size: 0x%x\n", dynamic_table[i].real_size[j]);
			printf("Real_end: 0x%x\n", dynamic_table[i].end_addr[j]);
		}
	}
#endif
	return 0;
}

static void page_align(unsigned int *address)
{
	if (*address & 4095)
		*address += (4096 - (*address % 4096));
		/* Page-align the sucker */
}

static void even_page_align(unsigned int *address)
{
	if (*address & 8191)
		*address += (8192 - (*address % 8192));
		/* even-page-align the sucker */
}

/* Make process table */
void proc_tbl(struct obj_inf *objs, int num_prgs, unsigned int *cur_page,
							int quantum) 
{
    int i, j;
    unsigned int *entry_base;
#ifdef DEBUG
    printf("cob: Writing process table\n");
#endif
    for (i = 0; i < num_prgs; i++) {

	entry_base = dest_addr(*cur_page);

#ifdef DEBUG
	loprintf("Entry base == %x\n", entry_base);
#endif
	*cur_page += U_SIZE;
	for (j = 0; j < U_SIZE >> 2; j++) {
	    switch(j) {
		/* Entry Point */
		case OFF_TO_IND(U_PC):
			entry_base[j] = objs[i].entry_addr;
			break;
		case OFF_TO_IND(U_PID):
			entry_base[j] = i + 1;
			break;
		case OFF_TO_IND(U_PTEXT):
			entry_base[j] = objs[i].start_addr[TEXT_S];
			break;
		case OFF_TO_IND(U_PDATA):
			entry_base[j] = objs[i].start_addr[DATA_S];
			break;
		case OFF_TO_IND(U_PBSS):
			entry_base[j] = objs[i].start_addr[BSS_S];
			break;
		case OFF_TO_IND(U_VTEXT):
			entry_base[j] =	objs[i].scn_addr[TEXT_S];
			break;
		case OFF_TO_IND(U_VDATA):
			entry_base[j] =	objs[i].scn_addr[DATA_S];
			break;
		case OFF_TO_IND(U_VBSS):
			entry_base[j] =	objs[i].scn_addr[BSS_S];
			break;
		case OFF_TO_IND(U_TSIZE):
			entry_base[j] =	objs[i].scn_size[TEXT_S];
			break;
		case OFF_TO_IND(U_DSIZE):
			entry_base[j] =	objs[i].scn_size[DATA_S];
			break;
		case OFF_TO_IND(U_BSIZE):
			entry_base[j] =	objs[i].scn_size[BSS_S];
			break;
		case OFF_TO_IND(U_PGTBL):
			entry_base[j] = objs[i].pg_tbl_addr;
			break;
		case OFF_TO_IND(U_PGTBLVEND):
			entry_base[j] = objs[i].pg_tbl_end -
					objs[i].pg_tbl_addr + PTEBASE;
#ifdef DEBUG
			loprintf("U_PGTBLVEND = %x\n",
				objs[i].pg_tbl_end - objs[i].pg_tbl_addr +
								PTEBASE);
#endif
			break;
		case OFF_TO_IND(U_NEXT):
			if ((i + 1) == num_prgs)
				entry_base[j] = 0; /* End of list */
			else
				entry_base[j] = (*cur_page & 0x1fffffff) |
						0x80000000; /* Next entry */
			break;
		case OFF_TO_IND(U_QUANT):
			entry_base[j] = quantum;
			break;
		case OFF_TO_IND(U_MAGIC1):
			entry_base[j] = MAGIC1;
			break;
		case OFF_TO_IND(U_MAGIC2):
			entry_base[j] = MAGIC2;
			break;
		case OFF_TO_IND(U_SHARED):
			entry_base[j] = SHARED_VADDR;
			break;
		case OFF_TO_IND(U_MYADR):
			entry_base[j] = ((*cur_page - U_SIZE) & 0x1fffffff) |
							0x80000000;
			break;
		default:
			entry_base[j] = 0;
	    }
	}
    }
}


/* Generate page-aligned code */
void prg_code(struct obj_inf *objs, int num_prgs, unsigned int *cur_page,
	int swizzle)
{
    int i, j, k;
    unsigned int ptr;
    unsigned int *scn_mem;

    for (i = 0; i < NUM_SECTS; i++) {
	for (j = 0; j < num_prgs; j++) {
	    loprintf("Setting up process %d, section %d  \r", j, i);
	    if (objs[j].scn_size[i] == 0){
			/* What do we do?  Nothing? */
			;
	    } else if ((j == 0) || i != SHARED_S) {
		/* Only set up memory for the SHARED page once... the first
			time. */

#ifdef DEBUG
		printf("Current page beginning: %x\n", *cur_page);
#endif
		page_align(cur_page);

#ifdef DEBUG
		printf("Aligned current page beginning: %x\n", *cur_page);
#endif

		scn_mem = dest_addr(*cur_page);

#ifdef DEBUG
		loprintf("Dest memory = %x\n", scn_mem);
#endif

	        objs[j].start_addr[i] = *cur_page;
	        objs[j].real_size[i] = (((objs[j].scn_size[i] -1 ) / PG_SIZE) 
						+ 1) * PG_SIZE;
	        objs[j].end_addr[i] = objs[j].start_addr[i] + 
						objs[j].real_size[i] - 1;
	        *cur_page += objs[j].real_size[i];

		/* If it's a real section, and it has a real pointer... */
		if ((i < NUM_REAL_SECTS)/* && objs[j].scn_ptr[i] */) { 
#ifdef DEBUG
			printf("Loadin' up program %d, section %d.\n", j, i);
			printf("    %x words.\n",
					objs[j].scn_size[i] / sizeof(int));
#endif
			for (k = 0; k < ((objs[j].scn_size[i] / sizeof(int)));
									k++)
				scn_mem[k] = (objs[j].scn_ptr[i])[k^swizzle];
		} else {
#ifdef ZEROPROCESSES
			if ((j == 0) || (i != SHARED_S)) { 
#ifdef DEBUG
				printf("Zeroing out program %d, section %d.\n",
									j, i);
				printf("    %x words.\n",
					objs[j].scn_size[i] / sizeof(int));
#endif
				ptr = (unsigned int)scn_mem;
				for (k = 0; k < objs[j].scn_size[i]; k+=4) {
					store_word(ptr + k, 0);
				}
			}
#else
#ifdef DEBUG
			loprintf(">>> NOT zeroing niblet process sections!\n");
#endif /* DEBUG */
#endif /* 0 */
		}
	    } else {
		/* Set up the globally shared memory here... */
		objs[j].start_addr[i] =objs[0].start_addr[i]; 
		objs[j].real_size[i] = objs[0].real_size[i]; 
		objs[j].end_addr[i] = objs[0].end_addr[i]; 
	    }
	}
    }
}


void pg_tbl(struct obj_inf *objs, int num_prgs, unsigned int *cur_page,
		int dcoher, int tcoher)
{

    int i, j, k;
    unsigned int page;
    int high_page;
    unsigned int enlo;
    int found = 0;
    unsigned int *scn_mem;

	loprintf("Writing page tables...    \r");

    /* Generate a page table for each program */
    for (i = 0; i < num_prgs; i++) {
	high_page = -1;
	for (j = 0; j < NUM_SECTS; j++) {
#ifdef DEBUG		
		loprintf("Object %d, section %d\n", i, j);
#endif
		if ((int)VA2PG(objs[i].scn_end[j]) > high_page)
			high_page = (int)VA2PG(objs[i].scn_end[j]);
#ifdef DEBUG
		loprintf("hp = %d, sp = %x, np = %x, va_end = %x, vpn_end = %x\n",
			high_page,
			objs[i].start_addr[j],
			objs[i].real_size[j],
			objs[i].end_addr[j],
			VA2PG(objs[i].scn_end[j])
			);
#endif
	}
	page_align(cur_page);
	objs[i].pg_tbl_addr = ((*cur_page) & 0x1fffffff) | 0x80000000;
#ifdef DEBUG
	loprintf("upt%d:\t# Page table for program #%d (%s)\n",
			i + 1, i + 1, objs[i].name);
	loprintf("cur_page == 0x%x\n", *cur_page);
#endif

	scn_mem = dest_addr(*cur_page);

#ifdef DEBUG
	loprintf("scn_mem = %x\n", (int)scn_mem);
#endif
	for (j = 0; j <= high_page; j++) {
	    found = 0;
	    for (k = 0; k < NUM_SECTS; k++) {
		if ((j >= VA2PG(objs[i].scn_addr[k])) && 
				(j <= VA2PG(objs[i].scn_end[k])) &&
				(objs[i].scn_size[k] != 0)) {
			page =  VA2PG(objs[i].start_addr[k]) + 
					(j - VA2PG(objs[i].scn_addr[k]));
			enlo = page << TLBLO_PFNSHIFT;
			enlo |= PG_FLAGS;
			if (k == TEXT_S)
				enlo |= tcoher << 3;
			else		
				enlo |= dcoher << 3;

			scn_mem[j * 2] = enlo;
			scn_mem[j * 2 + 1] = 0;
#ifdef DEBUG
			loprintf("*(%x) == %x (%s VPN 0x%x PFN 0x%x)\n",
						&(scn_mem[j * 2]), enlo,
						scn_names[k], j, page);

			loprintf("\t.word\t0x%x, 0x%x\t# (%s VPN 0x%x PFN 0x%x)\n", 
					    enlo, 0, scn_names[k], j, page);
#endif
			found = 1;
		} 
	    }
	    if (!found) {
		scn_mem[j * 2] = 0;
		scn_mem[j * 2 + 1] = 0;
	    }
	}
	*cur_page += high_page * 8;	/* One doubleword per PTE */

	if (!(((high_page * 8) / PG_SIZE) & 1)) {

#ifdef CACHED
	scn_mem = (unsigned int *)(((*cur_page) & 0x1fffffff) | 0x80000000);
#else
	scn_mem = (unsigned int *)(((*cur_page) & 0x1fffffff) | 0xa0000000);
#endif /* CACHED */

	    page_align(cur_page);
	    clear_mem(*cur_page, ((int)*cur_page) + PG_SIZE);

	    (*cur_page) += PG_SIZE;
#ifdef DEBUG
	    loprintf("\n\t.align %d\n", PG_SIZE);
	    loprintf("\t.word 0\t\t# Makes it OK to load two consecutive pgs\n");
#endif
	} /* if (odd number of pages) */

        page_align(cur_page);
	
#ifdef DEBUG
	loprintf("cur_page == 0x%x\n", *cur_page);
#endif
	objs[i].pg_tbl_end = ((*cur_page) & 0x1fffffff) | 0x80000000;
    } /* for (i == num_prgs) */
}

static void copy_code(unsigned int *dest_start, unsigned int *dest_end,
			unsigned int source_off)
{
	unsigned int *source;

	/* Convert the base anf offset into a source pointer */
	source = &(niblet_code[source_off / sizeof(unsigned int)]);

#ifdef DEBUG
	loprintf("Copy: source = 0x%x\n", source);
	loprintf("Copy: dest_start = 0x%x, dest_end = 0x%x\n",
							dest_start, dest_end);
#endif

/* Convert pointers to uncached space */
	dest_start = dest_addr((unsigned int)dest_start);
	dest_end = dest_addr((unsigned int)dest_end);

#ifdef DEBUG
	loprintf("Copy: Converted dest_start = 0x%x, dest_end = 0x%x\n",
							dest_start, dest_end);
#endif
	while (dest_start < dest_end) {
		*dest_start++ = *source++;
		/* Copy those exception handlers, text, and data! */
	}

}

clear_mem(unsigned int *start, unsigned int *end)
{

	start = dest_addr((unsigned int)start);
	end = dest_addr((unsigned int)end);

#ifdef DEBUG
	loprintf("Clearing from %x to %x\n", start, end);
#endif

	while (start < end)
		store_word(start++, 0);
}

int start_niblet(unsigned int where, int timeout, int which_array)
{
	int (*niblet)();
	volatile int result;
	volatile unsigned int *reg_storage;
	unsigned int tmp;
	unsigned int reg_area[68];
	evcfginfo_t *cfginfo;
	evbrdinfo_t *brd;
	int i, j;
	int num_cpus = 0;
	int overflow = 0;
	char *directory;

	if ((unsigned int)(reg_area) & 0x7)
		reg_storage = &(reg_area[1]);
	else
		reg_storage = &(reg_area[0]);

	/* Initialize Niblet */
	/* Start all CPUs at niblet entry point. */
	/* Change pass and fail so all processors take an interrupt */
	/* Add timeout interrupt */
	niblet = (int (*)())nib_text_entry;

	loprintf("Niblet's text starts at %x, entry point at %x\n",
			nib_text_start, nib_text_entry);

#ifndef IN_CACHE
#ifdef MP
	cfginfo = EVCFGINFO;

	/* Set the launch address for every slave */
	for (i = 0; i < EV_MAX_SLOTS; i++) {
		brd = &(cfginfo->ecfg_board[i]);
		if ((brd->eb_type & EVCLASS_MASK) == EVCLASS_CPU) {
			for (j = 0; j < EV_MAX_CPUS_BOARD; j++) {
				if ((brd->eb_cpuarr[j].cpu_vpid != CPUST_NORESP)
 				    && (brd->eb_cpuarr[j].cpu_enable)
				    && !(overflow = (num_cpus >= 15))) {
					loprintf("Slot %b CPU %b\n", i, j);
					brd->eb_cpuarr[j].cpu_launch =
							nib_text_entry;
					num_cpus++;
				} else {
					brd->eb_cpuarr[j].cpu_launch =
							slave_return;
				}
			}
		}
	}
	if (overflow)
		loprintf("Niblet can handle a maximum of 15 CPUs.\n");

#endif /* MP */
#endif /* !IN_CACHE */
#ifdef DEBUG
	loprintf("Storing proc table address (%x) into %x\n",
			where, nib_exc_start);
#endif
	/* Store process table address before launching anyone */
	store_word(nib_exc_start + STUP_PROCTBL, where);
	/* Store timeout value too */
	store_word(nib_exc_start + STUP_TIMEOUT, timeout);
	/* Store the number of processors that will run this test. */
#ifdef MP
	store_word(nib_exc_start + STUP_NCPUS, num_cpus);
#else
	store_word(nib_exc_start + STUP_NCPUS, 1);
#endif /* MP */	
	
	/* initialize stuff that will be stored by Niblet */
	store_word(nib_exc_start + STUP_DIRECTORY, 0);
	store_word(nib_exc_start + STUP_WHY, 0);

	save_gprs(reg_storage);

	call_asm(pon_invalidate_icache, 0);

#ifndef IN_CACHE
	call_asm(pon_flush_and_inval, 0);
#else
	call_asm(pon_flush_dcache, 0);
#endif /* !IN_CACHE */

#ifdef MP

	loprintf("Sending interrupt and calling Niblet.\n");
	/* Now launch all of the slaves! */
	/* Ideally, later, we should check their diag_val */
	store_double_lo(EV_SENDINT, (LAUNCH_LEVEL << 8) | PGRP_SLAVE);
#endif

	result = pod_jump(nib_text_entry);

	if (result)
		loprintf("Supertest FAILED.\n");
	else
		loprintf("Supertest PASSED.\r\n");

	(void)load_gprs(reg_storage);

	if (result == NIBFAIL_BARRIER) {
		loprintf("    Processor(s) did not arrive!\n");
		directory = load_word(nib_exc_start + STUP_DIRECTORY);
		if (! (0x80000000 & (unsigned long)directory)) {
		    loprintf("    Structure corrupted!\n");
		} else {
			
		    /* Have to figure out who didn't make it. */
		    for (i = 0; i < EV_MAX_SLOTS; i++) {
			brd = &(cfginfo->ecfg_board[i]);
			if ((brd->eb_type & EVCLASS_MASK) == EVCLASS_CPU) {
			    for (j = 0; j < EV_MAX_CPUS_BOARD; j++) {
				if ((brd->eb_cpuarr[j].cpu_vpid
							    != CPUST_NORESP)
					    && (brd->eb_cpuarr[j].cpu_enable)
					    && (num_cpus > 0)) {
				    if (!load_word(directory
						+ (i*4 + j) * DIR_ENT_SIZE
						+ DIR_BAROFF))
					loprintf( "    CPU %b/%b didn't run!\n",
								    i, j);
				    num_cpus--;
				} /* if */
			    } /* for */
			} /* if */
		    } /* for */
		} /* else */
	} else if (result) {
	    i = load_word(nib_exc_start + STUP_WHY);
	    if ((i <= 0) || (i >= NUM_NIBFAILS)) {
		loprintf("    Reason corrupted!\n");
	    } else {
		j = load_word(nib_exc_start + STUP_WHO);
		loprintf("    %s on CPU %b/%b!\n", reasons[i],
			(j & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT,
			(j & EV_PROCNUM_MASK));
	    }
	    if (i == NIBFAIL_PROC) {
		/* get PID */
		i = load_word(nib_exc_start + STUP_PID);
		if ((i <= 0) || (i > PROCS_PER_TEST)) {
		    loprintf("    Bad PID (%d)\n", i);
		} else {
		    loprintf("    PID %d", i);
		    /* Get Niblet test number. */
		    j = test_array[which_array][i - 1];
	
		    if ((j < 0) || (j > NUM_PROCS))
			loprintf("\n    Bad process name (%d).\n", j);
		    else
			/* Get Niblet test name. */
			loprintf(" (%s) failed.\n",
					    static_table[j].name);
		}
	    }
	}

	if (result)
		return EVDIAG_NIBFAILED;
	else
		return 0;
}


int do_niblet(unsigned int which_array) {

unsigned int where;
int timeout = 0xffffffff;
int result;
int *tmp_ptr;
unsigned int data_addr;

	if (which_array >= num_tests) {
		loprintf("*** Test index out of range.\r\n");
		return EVDIAG_TBD;
	}

#ifdef DEBUG
	printf("\nstatic_table = 0x%x\n", (unsigned int)static_table);
	printf("nib_exc_start = 0x%x\n", (unsigned int)nib_exc_start);
	printf("nib_exc_end = 0x%x\n", (unsigned int)nib_exc_end);
	printf("niblet_code = 0x%x\n", (unsigned int)niblet_code);
#endif

#ifndef IN_CACHE
	call_asm(pon_flush_and_inval, 0);
#else
	call_asm(pon_flush_dcache, 0);
	loprintf("Setting up scache\r");
	call_asm(pon_validate_scache, 0);
#endif /* IN_CACHE */

#if 1
	call_asm(pon_zero_icache, 0x80100000);
#else
	zero_icache();
#endif

	data_addr = nib_bss_end;
	page_align(&data_addr);

	if (dcob(static_table, test_array[which_array],
			data_addr,
			&where, QUANTUM, TEXT_COHERENCY, DATA_COHERENCY)) {
		loprintf("*** Error in test array\n");
		return 1;
	}

	loprintf("Copying vectors...                      \r");
#ifdef DEBUG
	loprintf("\n");
#endif

	copy_code((unsigned int *)(0x80000000),
		(unsigned int*)(0x80000000 + nib_exc_end - nib_exc_start),
		nib_exc_off);

	loprintf("Copying text...     \r");
#ifdef DEBUG
	loprintf("\n");
#endif
	copy_code((unsigned int *)nib_text_start,
			(unsigned int*)nib_text_end, nib_text_off);
	
	loprintf("Copying data...\r");
#ifdef DEBUG
	loprintf("\n");
#endif
	copy_code((unsigned int *)nib_data_start,
			(unsigned int*)nib_data_end, nib_data_off);

	loprintf("Clearing BSS...\r");
#ifdef DEBUG
	loprintf("\n");
#endif

	clear_mem((unsigned int *)nib_bss_start,
			(unsigned int*)nib_bss_end);
	
	loprintf("Clearing SBSS...\r");
#ifdef DEBUG
	loprintf("\n");
#endif
	clear_mem((unsigned int *)nib_sbss_start,
			(unsigned int*)nib_sbss_end);

	loprintf("Starting Niblet\r");
	where &= 0x1fffffff;
	where |= 0x80000000;
	printf("Niblet proc table resides at 0x%x\n", where);

	result = start_niblet(where, timeout, which_array);

	printf("Niblet Complete.\n");
	return result;
}

