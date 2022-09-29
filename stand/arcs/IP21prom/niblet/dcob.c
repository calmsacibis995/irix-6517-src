/* NOTE: This guy will need to do some swizzling to make it work in either
	endianness */
/*
   This file executes on either the test machine, which is the machine
   that is being tested, or the home machine, for use in an RTL environment
   where you are not running from the bootprom.  When running on the 
   test machine, dcob is run from the bootprom.  We call this environment
   the COOKED environment.  In the COOKED environment, the bootprom,
   dcob, and niblet all run on the test machine.  The bootprom calls
   do_niblet, which calls dcob.  Dcob generates page tables and proc 
   tables, and then returns to do_niblet.  Do_niblet then calls 
   start_niblet which invokes niblet.

   When running niblet on an RTL, you do not generally want to run
   from the bootprom because it is so time consuming.  Furthermore,
   you do not want to generate page tables and proc tables on the RTL
   because that is also time consuming.  Instead, you run in RAW mode.
   You run dcob on your home machine in RAW mode and it generates
   files which represent the page table and proc tables, and tests,
   namely raw_pg_tbl.s, raw_proc_tbl.s, and raw_programs.s

   I encountered lots of problems making dcob work in both COOKED and
   RAW mode because currently, in COOKED mode we are running on a 64
   bit architecture and the code is compiled with a 64 bit compiler.
   However, in RAW mode we are running using 32 bit compilers.  There
   is lots of pointer manipulation in the dcob code which is really
   confusing when going between 64 bit and 32 bit.  For example:

   1.  There are variables that contain 64 bit addresses that
	   represent addresses on the test machine, such as page table
	   addresses or proc table addresses.   In COOKED mode dcob may
	   actually store values into these addresses, while in RAW mode
	   we may just put the addresses into a data structure, print it
	   to the raw_xxx.s file, or just print it as a debug message.
	   Cur_page is an example of this. We use it to figure out where 
	   to put things in the 64 bit world, and in COOKED mode we copy
	   data from the bootprom out to *cur_page.  In RAW mode, we don't
	   copy data to *cur_page, but we put cur_page into the
	   proc_tables and we use it to figure the start and end of the
	   page tables, also placing this info in the proc tables. So,
	   whereas in a 64 bit world this could just be declared as a
	   pointer, in a 32 bit world it must be declared as a long long
	   in order for the compiler to create a 64 bit variable that can
	   hold the test machines 64 bit addresses.  If we declare it as a
	   pointer on the 32 bit machine, it will be only 32 bits.  Another
	   example of this is entry_base in proc_tbl().  This used to be
	   defined as a pointer to a long long and we would access it as
	   an array like this 'entry_base[j] = whatever'.  However, in
	   RAW mode I want to be able to print the 64 bit value that entry_base
	   contains because it represents the address where the proc_table
	   starts.  But if it is declared as a pointer, it is only 32 bits.
	   So I changed it to be a long long, but now I must cast it as a
	   pointer before I store 

           ((tm_addr_type *)entry_base)[j] = whatever

	   or my indexing will be messed up.  Alternately, I could do this:

   	       entry_base[j*8] = whatever


	2. There are variables that contain addresses whose values
	   represent real addresses on which ever machine we are running
	   on.  For example, start_addr contains the memory location where
	   we should store the dynamic table of test obj_inf structures.
	   We store to this address in both COOKED and RAW modes, so in
	   COOKED mode it must be a 64 bit address and in RAW mode it
	   would be logical for it to be a 32 bit address.  However, there
	   are places in the dcob code where we assign variables of type
	   (2) to variables of type (1).  This means that if a variable of
	   type (2) is declared as a pointer, then on a 32 bit machine it
	   will be only 32 bits long.  So when we assign it to a variable
	   of type (1), which must be 64 bits long, it should work just
	   fine.  And I believe it does work, but it generates warnings
	   which make me uncomfortable.  Unfortunately the compiler
	   generates the same warnings if we assign a 32 bit value to a 64
	   bit value as if we assign a 64 bit value to a 32 bit value.
	   Assigning a 64 bit value to a 32 bit value will clearly cause
	   problems, so I don't want to have the code generating a lot of
	   warnings for the cases that are ok because that will mask the
	   warnings that are *not* ok.

   Basically I've decided that it is both confusing and dangerous to
   use pointers at all in this code because they change size based on
   whether you are running on a 32 bit machine or a 64 bit machine.
   So I have tried to make everything long longs, distinguishing
   between two different types which are both defined as long long.

       typedef long long tm_addr_type
       typedef long long hm_addr_type
       typedef long long em_addr_type

   tm_addr_type refers to an address that always represents an address
   on the test machine.  hm_addr_type refers to an address that always
   represents an address on the home machine.  em_addr_type refers to
   an address that represents an address on the test machine when
   running in COOKED mode and an address on the hom machine when
   running in RAW mode.

REVISION LOG
------------
2/94     RAR  Added ifdef's for T5
3/3/94   RAR  Fixed "Padding text..." msg to print correct entry point;
               Fixed padding code to pad to start of code rather than to
               the entry point.  (Failed for cases when the entry point
               was not at the beginning of the code.)  Changed "Padding
	       text..." msg to print point where the code begins rather
	       than the entry point.
3/8/94   RAR  Changed dcob calls to use quantum instead of QUANTUM so
               as to honor the command line "-q" option.  Enhanced code
	       to adjust this value for TFP's funky behavior.
*/


/* Margie removed.  Defined in Makefile */
/* #define R4000 1 */

#include <stdio.h>
#ifndef RAW_NIBLET

#ifdef T5
/* Special everest.h which calls modified IP19.h */
#include "everest.h"
#else
#include <sys/EVEREST/everest.h>
#endif

#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evaddrmacros.h> /* for KPHYSTO32k0 */
#ifdef R4000
#include <sys/EVEREST/IP19addrs.h>
#elif TFP
#include <sys/EVEREST/IP21addrs.h>
#endif
#elif RAW_NIBLET
#include <time.h>
#include <getopt.h>
#endif
#include "niblet.h"
#include "cob.h"
#ifdef R4000
#include "cp0_r4k.h"
#else
#include <sys/tfp.h>
#endif
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

#ifdef RAW_NIBLET
#define store_startup_info(offset, value) \
	fprintf(fh, ".dword %llx\n", value)
#else
#define store_startup_info(offset, value) \
 	store_double_lo(nib_exc_start + offset, value)
#endif


#ifdef RAW_NIBLET
char proc_table_strings[160][16] = {
"U_PC",
"U_R1",
"U_R2",
"U_R3",
"U_R4",
"U_R5",
"U_R6",
"U_R7",
"U_R8",
"U_R9",
"U_R10",
"U_R11",
"U_R12",
"U_R13",
"U_R14",
"U_R15",
"U_R16",
"U_R17",
"U_R18",
"U_R19",
"U_R20",
"U_R21",
"U_R22",
"U_R23",
"U_R24",
"U_R25",
"U_R26",
"U_R27",
"U_R28",
"U_R29",
"U_R30",
"U_R31",
"U_CSIN",
"U_CSOUT",
"U_OUTENHI",
"U_INENHI",
"U_STAT",
"U_PID",
"U_XA",
"U_PGTBL",
"U_QUANT",
"U_NEXT",
"U_MYADR",
"U_PTEXT",
"U_PDATA",
"U_PBSS",
"U_VTEXT",
"U_VDATA",
"U_VBSS",
"U_TSIZE",
"U_DSIZE",
"U_BSIZE",
"U_MAGIC1",
"U_MAGIC2",
"U_SHARED",
"U_EXCCOUNT",
"U_ERRCOUNT",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCTBLBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCCNTBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_EXCENHIBASE",
"U_ERRTBLBASE",
"U_ERRCNTBASE",
"U_PGTBLVEND"
};
#endif





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

void proc_tbl(struct obj_inf *, int, tm_addr_type *, int);
void prg_code(struct obj_inf *, int, tm_addr_type *, int);
void pg_tbl(struct obj_inf *, int, tm_addr_type *, int, int);
static void page_align(tm_addr_type *);
int dcob(struct obj_inf *, int *, tm_addr_type, tm_addr_type *,
		int, int, int);

extern struct obj_inf static_table[];
extern unsigned int niblet_code[];
extern tm_addr_type nib_exc_start, nib_exc_end;
extern tm_addr_type nib_data_start, nib_data_end;
extern tm_addr_type nib_text_start, nib_text_end;
extern tm_addr_type nib_bss_start, nib_bss_end;
extern tm_addr_type nib_sbss_start, nib_sbss_end;
extern tm_addr_type nib_text_entry;
extern void slave_return();
extern unsigned long nib_text_off, nib_data_off, nib_exc_off;
#ifdef R4000
extern void pon_zero_icache(uint);
#endif
extern void pon_invalidate_IDcaches();
extern void pon_flush_dcache();
extern void flush_tlb();
extern void pon_flush_scache();
extern void pon_validate_scache();
/* extern void pon_flush_and_inval(); */
extern void pon_invalidate_dcache(); 
extern uint	load_word(tm_addr_type);
#ifdef RAW_NIBLET
extern int atoi(const char *str);
#endif



/* #define DEBUG */

#ifdef RAW_NIBLET
#define MAX_TESTS 100
#define clear_mem(a,b)
#include <malloc.h>
main(int argc, char *argv[])
{
	tm_addr_type where;
	int which_array = 0x8; /* 1 */


	int i;
	int num_prgs;
	int c;
	int data_addr = NIBLET_RAWDATA_ADDR;
	int errflag = 0;
	int swizzle = 0;
	int quantum = QUANTUM;
	int tcoher = TEXT_COHERENCY;
	int dcoher = DATA_COHERENCY;

	while ((c = getopt(argc, argv, "t:c:q:d:")) != -1)  {
	    switch (c) {
		  case 'c':
			dcoher = atoi(optarg);
			break;
		  case 't':
			tcoher = atoi(optarg);
			break;
		  case 'd':
			data_addr = atoi(optarg);
			break;
		  case 'q':
			quantum = atoi(optarg);
#ifdef TFP
			/* Subtract from 0x80000000 as TFP counts up to this value */
			quantum = 2147483648 - quantum;
#endif
			break;
		  case '?':
			errflag++;
			break;
#if 0
		  case 'g':
			global_config = atoi(optarg);
			break;
		  case 's':
			swizzle = 1;
			break;
#endif
		}
	}

	if ((argc - optind) != 1) {
		fprintf(stderr, "usage: dcob [options] testid\n");
		exit (1);
	}
	which_array = atoi(argv[optind]);

	if (which_array > num_tests) {
		printf("Error: Only %d entries in nib_conf.c\n", num_tests);
		exit(1);
	}
	printf("Dcobbing:");
	i = 0; 
	while (test_array[which_array][i] != -1) {
		printf(" %s", static_table[test_array[which_array][i]].name);
		i++;
	}
	printf("\n");


	if (errflag) {
		fprintf(stderr, "usage: cob -d data_addr -q time_ quantum -t text_coherency \n\t-d data_coherency file1... filen\n");
		exit(1);
	}

	dcob(static_table, test_array[which_array], (tm_addr_type)0, &where, quantum,
		 TEXT_COHERENCY, DATA_COHERENCY);
	exit(0);
}

#endif RAW_NIBLET




tm_addr_type dest_addr(tm_addr_type original_addr)
{

#ifdef CACHED
	return (tm_addr_type)((original_addr & ADDRSPACE_ADDRESS) | ADDRSPACE_RGN_UNMAPPED_CACHED);
#else
	return (tm_addr_type)((original_addr & ADDRSPACE_ADDRESS) | ADDRSPACE_RGN_UNMAPPED_UNCACHED);
#endif
}



int dcob(struct obj_inf *static_table, 
		 int            *nib_list,
		 tm_addr_type   start_addr, 
		 tm_addr_type   *proc_tbl_addr,
		 int            quantum, 
		 int            tcoher, 
		 int dcoher)
{
#ifdef DCOBDEBUG
	int l; /* MARGIE added for debug below */
	int i, j, k;
#endif
	struct obj_inf *dynamic_table;
	tm_addr_type cur_page;
	int swizzle = 0;
	int num_procs = 0;
#ifdef RAW_NIBLET
	FILE *rd;
#endif

#ifdef VERBOSE
	loprintf("nib_kern:    *(%llx) == nib_exc_start\n", nib_exc_start);
	loprintf("nib_kern:    *(%llx) == nib_text_start\n", nib_text_start);
	loprintf("nib_kern:    *(%llx) == nib_text_entry\n", nib_text_entry);
	loprintf("nib_kern:    *(%llx) == nib_data_start\n", nib_data_start);
	loprintf("nib_kern:    *(%llx) == nib_bss_start\n", nib_bss_start);
	loprintf("nib_kern:    *(%llx) == nib_bss_end\n", nib_bss_end);
#endif

#if !defined(RAW_NIBLET) && defined(DCOBDEBUG)
	loprintf("dcob: Start address is %llx\n", start_addr);
#endif

#ifdef DCOBDEBUG
	loprintf("dcob: %d processes available\n", avail_procs);
#endif

#ifdef RAW_NIBLET
	dynamic_table = (struct obj_inf *) malloc(MAX_TESTS * sizeof(struct obj_inf));
#else COOKED
	dynamic_table = (struct obj_inf *)start_addr;
#endif

	while (nib_list[num_procs] != -1) {
		if ((nib_list[num_procs] >= avail_procs) || (nib_list[num_procs] < -1))
			return 1;
		else
			/* Copy obj_inf structs from static table to dynamic table */
			dynamic_table[num_procs] = (static_table[nib_list[num_procs]]);
#ifdef DCOBDEBUG
		loprintf("dcob: Adding niblet process %d\n", nib_list[num_procs]);
#endif
		num_procs++;
	}

#ifdef RAW_NIBLET
	cur_page = NIBLET_RAWDATA_ADDR + 1;
#else
	cur_page = start_addr + (sizeof(struct obj_inf) * num_procs);
#endif
	page_align(&cur_page);

#ifdef DCOBDEBUG
	for (i = 0; i < num_procs; i ++) {
		printf("dcob: file: %s\n", dynamic_table[i].name);
		printf("--------------------------------\n");
		for (j = 0; j < NUM_SECTS; j++) {
			printf("Section: %s\n", scn_names[j]);
			printf("Size: %d\n", dynamic_table[i].scn_size[j]);
 			printf("Addr: 0x%llx\n", dynamic_table[i].scn_addr[j]);
			printf("End: 0x%llx\n", dynamic_table[i].scn_end[j]);
			printf("Ptr: 0x%x\n", dynamic_table[i].scn_ptr[j]);
		}
		printf("PT Start: 0x%llx\n", dynamic_table[i].pg_tbl_addr);
		printf("PT End: 0x%llx\n", dynamic_table[i].pg_tbl_end);
		printf("Entry: 0x%llx\n", dynamic_table[i].entry_addr);
		printf("Text starts with:\n");
		for (k = 0; k < 8; k++) {
			printf("%8x\t", dynamic_table[i].scn_ptr[TEXT_S][k]);
			if ((k % 4) == 3)
				printf("\n");
		}
	}
#endif

#ifdef DCOBDEBUG
	loprintf("\ndcob: Generating code...\n");
#endif

	prg_code(dynamic_table, num_procs, &cur_page, swizzle); 

#ifdef DCOBDEBUG
	loprintf("\ndcob: Generating page tables...                  \n");
#endif

 	pg_tbl(dynamic_table, num_procs, &cur_page, dcoher, tcoher);

	page_align(&cur_page);

#ifdef DCOBDEBUG
	loprintf("\ndcob: Generating process table...\n");
#endif

	*proc_tbl_addr = cur_page;
#ifdef RAW_NIBLET
	rd = fopen("raw_nibdata.s", "w");
	fprintf(rd, ".data\n");
	fprintf(rd, ".align %d\n", PGSZ_BITS);
	fprintf(rd, "\t.dword 0x%llx\n", cur_page); /* STUP_PROCTBL */
	fprintf(rd, "\t.dword 0x0\n");              /* STUP_TIMEOUT */
	fprintf(rd, "\t.dword 0x1\n");              /* STUP_NCPUS */
	fclose(rd);
#endif

	proc_tbl(dynamic_table, num_procs, &cur_page, quantum);


#ifdef RAW_NIBLET
}
#endif /* RAW_NIBLET */

#ifndef RAW_NIBLET
#if 0
/* MARGIE added this - just debug info for even when DCOBDEBUG is off */
	for (l = 0; l < num_procs; l ++) {
		printf("file: %s\n", dynamic_table[l].name);
		printf("--------------------------------\n");
	}
#ifdef DCOBDEBUG
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
#endif
	return 0;
}

#endif RAW_NIBLET



static void page_align(tm_addr_type *address)
{
	if (*address & 4095)
		*address += (4096 - (*address % 4096));
		/* Page-align the sucker */
}


#ifdef RAW_NIBLET
#define set_entry(value) \
	fprintf(pt, "\t.dword\t0x%llx\t\t\t# %s\n", (long long) value, string); \
	
#else
#define set_entry(value) \
	((tm_addr_type *) entry_base)[j] = value;
#endif

/* 
   MARGIE made some changes here to account for the fact that the
   proc table is now all 64 bit entries.  So entry base is now
   a pointer to a 64 bit value and we intiialize the proc table
   by doing store doubles to it instead of store words.
*/
/* Make process table */
void proc_tbl(struct obj_inf *objs, 
			  int num_prgs, 
			  tm_addr_type *cur_page,
			  int quantum) 
{
    int i;
    long long j;
    tm_addr_type entry_base; /* entry_base is just for debug info in RAW mode */
#ifdef RAW_NIBLET
	FILE *pt, *rd;
    time_t stamp;
	char *string;
#endif


#ifdef RAW_NIBLET
    pt = fopen("raw_proc_tbl.s", "w");
    time(&stamp);
    fprintf(pt, "\t# Created %s\n", ctime(&stamp));
	fprintf(pt, ".data\n");
#endif


#ifdef DCOBDEBUG
    printf("proc_tbl: Writing process table\n");
#endif
    for (i = 0; i < num_prgs; i++) {

#ifdef RAW_NIBLET
		fprintf(pt, "us%d:\t# Proc table entry for program #%d (%s)\n",
				i + 1, i + 1, objs[i].name);
		if (i == 0) {
			fprintf(pt, ".align %d\n", PGSZ_BITS);
		}
#endif

		entry_base = (tm_addr_type) dest_addr(*cur_page);

#if defined(DCOBDEBUG) || defined(VERBOSE)
		loprintf("proc_tbl: p%d *(%llx) == %s PROC. Entry = %llx\n", i + 1, entry_base, objs[i].name, objs[i].entry_addr);
#endif

		*cur_page += U_SIZE;
		for (j = 0; j < U_SIZE >> 3; j++) {
#ifdef RAW_NIBLET
			string = proc_table_strings[j];
#endif
			switch(j) {
			case OFF_TO_IND(U_PC):
#ifdef DCOBDEBUG
				printf("proc_tbl: p%d %s Entry point is %llx\n", i+1, objs[i].name, objs[i].entry_addr);
#endif
				set_entry(objs[i].entry_addr);
				break;
			case OFF_TO_IND(U_PID):
				set_entry(i + 1);
				break;
			case OFF_TO_IND(U_PTEXT):
				set_entry(objs[i].start_addr[TEXT_S]);
				break;
			case OFF_TO_IND(U_PDATA):
				set_entry(objs[i].start_addr[DATA_S]);
				break;
			case OFF_TO_IND(U_PBSS):
				set_entry(objs[i].start_addr[BSS_S]);
				break;
			case OFF_TO_IND(U_VTEXT):
				set_entry(objs[i].scn_addr[TEXT_S]);
				break;
			case OFF_TO_IND(U_VDATA):
				set_entry(objs[i].scn_addr[DATA_S]);
				break;
			case OFF_TO_IND(U_VBSS):
				set_entry(objs[i].scn_addr[BSS_S]);
				break;
			case OFF_TO_IND(U_TSIZE):
				set_entry(objs[i].scn_size[TEXT_S]);
				break;
			case OFF_TO_IND(U_DSIZE):
				set_entry(objs[i].scn_size[DATA_S]);
				break;
			case OFF_TO_IND(U_BSIZE):
				set_entry(objs[i].scn_size[BSS_S]);
				break;
			case OFF_TO_IND(U_PGTBL):
				set_entry(objs[i].pg_tbl_addr);
				break;
			case OFF_TO_IND(U_PGTBLVEND):
				set_entry(objs[i].pg_tbl_end - objs[i].pg_tbl_addr + PTEBASE);
#ifdef DCOBDEBUG
				loprintf("proc_tbl: U_PGTBLVEND = %x\n",
						 objs[i].pg_tbl_end - objs[i].pg_tbl_addr +
						 PTEBASE);
#endif
				break;
			case OFF_TO_IND(U_NEXT):
				if ((i + 1) == num_prgs) {
					set_entry(0); /* End of list */
				}
				else {
					set_entry((*cur_page & ADDRSPACE_ADDRESS) | ADDRSPACE_RGN_UNMAPPED_CACHED); /* Next entry */
				}
				break;
			case OFF_TO_IND(U_QUANT):
				set_entry(quantum);
				break;
			case OFF_TO_IND(U_MAGIC1):
				set_entry(MAGIC1);
				break;
			case OFF_TO_IND(U_MAGIC2):
				set_entry(MAGIC2);
				break;
			case OFF_TO_IND(U_SHARED):
				set_entry(SHARED_VADDR);
				break;
			case OFF_TO_IND(U_MYADR):
				set_entry(((*cur_page - U_SIZE) & ADDRSPACE_ADDRESS) | ADDRSPACE_RGN_UNMAPPED_CACHED);
				break;
			default:
				set_entry(0);
				break;
			} /* switch */
		} /* for (j = 0; j < U_SIZE >> 3; j++)  */
#ifdef RAW_NIBLET
		fprintf(pt, "\n");
#endif
    } /* for (i = 0; i < num_prgs; i++) */
#ifdef RAW_NIBLET
    fprintf(pt, "coblist:\n");
    fprintf(pt, "\t.ascii \"Running:");
    for (i = 0; i < num_prgs; i++)
		fprintf(pt, " %s", objs[i].name);
    fprintf(pt, "\"\n");
    fclose(pt);
#endif

}


/* Generate page-aligned code */
void prg_code(struct obj_inf *objs, int num_prgs, tm_addr_type *cur_page,
	int swizzle)
{
    int i, j, k, start_k;
    tm_addr_type ptr;
    tm_addr_type scn_mem; /* keep as int * because we use index it as array of ints */

#ifdef RAW_NIBLET
	FILE *prg;
    prg = fopen("raw_programs.s", "w");
	fprintf(prg, ".data\n");
#endif
    for (i = 0; i < NUM_SECTS; i++) {
		for (j = 0; j < num_prgs; j++) {
#ifdef DCOBDEBUG
			loprintf("prg_code: Setting up process %d, section %d  \n", j, i);
#endif
			if (objs[j].scn_size[i] == 0){
				/* What do we do?  Nothing? */
				;
			} else if ((j == 0) || i != SHARED_S) {
				/* Only set up memory for the SHARED page once... the first
				   time. */
				
#ifdef DCOBDEBUG
				printf("prg_code: Current page beginning: %llx\n", *cur_page);
#endif
				page_align(cur_page);
#ifdef RAW_NIBLET
				fprintf(prg, "\t# program %d, section %s.\n", j, scn_names[i]);
				fprintf(prg, "\t.align %d\n", PGSZ_BITS);
#endif

#ifndef RAW_NIBLET
				/* scn_mem accessed as an array of ints!  Not used by raw_niblet */
				scn_mem = dest_addr(*cur_page);

#if defined(DCOBDEBUG) || defined(VERBOSE)
				if (i != SHARED_S) {
					loprintf("prg_code: p%d *(%llx) == section %s, 0x%x words\n", j+1, scn_mem, scn_names[i], objs[j].scn_size[i] / sizeof(int), scn_mem);
				}
#endif
#endif

				objs[j].start_addr[i] = *cur_page;
				objs[j].real_size[i] = (((objs[j].scn_size[i] -1 ) / PG_SIZE) 
										+ 1) * PG_SIZE;
				objs[j].end_addr[i] = objs[j].start_addr[i] + 
					objs[j].real_size[i] - 1;
				*cur_page += objs[j].real_size[i];
				if ((j == 0) && i == SHARED_S) {

#if defined(DCOBDEBUG) || defined(VERBOSE)
					loprintf("shrd_mem:    *(%llx) == SHMEM start (size = %llx)\n", objs[j].start_addr[i], objs[j].real_size[i]);
					loprintf("shrd_mem:    *(%llx) == SHMEM end\n", objs[j].end_addr[i]);
#endif
				}

				/* If it's a real section, and it has a real pointer... */
				if ((i < NUM_REAL_SECTS)/* && objs[j].scn_ptr[i] */) { 
#ifdef DCOBDEBUG
					printf("prg_code: Loading 0x%x words\n", objs[j].scn_size[i] / sizeof(int));
#endif

					start_k = 0;
	
					if (scn_names[i][1] == 't') {
						if (objs[j].entry_addr != 0) {
#ifdef DCOBDEBUG
							printf("prg_code: Padding text section from 0 to code start point 0x%llx\n", objs[j].scn_addr[i]);
#endif
							for (start_k = 0; start_k < (objs[j].scn_addr[i]/sizeof(int)); start_k++) {
#ifndef RAW_NIBLET
								((int *)scn_mem)[start_k] = 0;
#elif RAW_NIBLET
								fprintf(prg, "\t.word\t0x%x\n", 0);
#endif
							}
						}
					}
					else {  /* MARGIE remove */
						printf("prg_code: Not text!\n");
					}
					
	
					for (k = start_k; k < start_k + ((objs[j].scn_size[i] / sizeof(int)));
						 k++) {
#ifndef RAW_NIBLET
						((int *)scn_mem)[k] = (objs[j].scn_ptr[i])[(k-start_k)^swizzle];
#elif RAW_NIBLET
						fprintf(prg, "\t.word\t0x%x\n", (objs[j].scn_ptr[i])[(k-start_k)^swizzle]);
#endif
					}
				} else {
#ifdef ZEROPROCESSES
					if ((j == 0) || (i != SHARED_S)) { 
#ifdef DCOBDEBUG
						printf("prg_code: Zeroing out program %d, section %d, 0x%x words.\n", j,i, objs[j].scn_size[i] / sizeof(int));
#endif
						ptr = scn_mem;
						for (k = 0; k < objs[j].scn_size[i]; k+=4) {
#ifndef RAW_NIBLET
							store_word(ptr + k, 0);
#elif RAW_NIBLET
							fprintf(prg, "\t.word\t0x%x\n", 0);
#endif

						}
					}
#else
#ifdef DCOBDEBUG
					loprintf("prg_code: NOT zeroing niblet process sections!\n");
#endif /* DCOBDEBUG */
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
#ifdef RAW_NIBLET
    fclose(prg);
#endif
}



void pg_tbl(struct obj_inf *objs, int num_prgs, tm_addr_type *cur_page,
			int dcoher, int tcoher)
{

    int i, j, k;
    unsigned int page;
    int high_page;
    unsigned long long enlo;			/* enlo used to be int - MARGIE */
    int found = 0;
    tm_addr_type scn_mem;		/* used to be unsigned int *scn_mem; - MARGIE */
#ifdef RAW_NIBLET
	FILE *pt;
#endif

#ifdef DCOBDEBUG
	loprintf("page_tbl: Writing page tables...    \n");
#endif

#ifdef RAW_NIBLET
    pt = fopen("raw_pg_tbl.s", "w");
	fprintf(pt, ".data\n");
#endif

    /* Generate a page table for each program */
    for (i = 0; i < num_prgs; i++) {
		high_page = -1;
		for (j = 0; j < NUM_SECTS; j++) {
#ifdef DCOBDEBUG		
			loprintf("page_tbl: Object %d, section %d\n", i, j);
#endif
			if ((int)VA2PG(objs[i].scn_end[j]) > high_page)
				high_page = (int)VA2PG(objs[i].scn_end[j]);
#ifdef DCOBDEBUG
			loprintf("page_tbl: hp = %d, sp = %x, np = %x, va_end = %x, vpn_end = %x\n",
					 high_page,
					 objs[i].start_addr[j],
					 objs[i].real_size[j],
					 objs[i].end_addr[j],
					 VA2PG(objs[i].scn_end[j])
					 );

#endif
		}
		page_align(cur_page);
		objs[i].pg_tbl_addr = ((*cur_page) & ADDRSPACE_ADDRESS) | ADDRSPACE_RGN_UNMAPPED_CACHED;
#ifdef DCOBDEBUG
		loprintf("page_tbl: upt%d:\t# Page table for program #%d (%s)\n",
				 i + 1, i + 1, objs[i].name);
		loprintf("page_tbl: cur_page == 0x%llx\n", *cur_page);
#endif

		scn_mem = dest_addr(*cur_page);

#if defined(DCOBDEBUG)
		loprintf("page_tbl: p%d page table starts at %llx\n", i, scn_mem);
#endif
#ifdef RAW_NIBLET
		fprintf(pt, "upt%d:\t# Page table for program #%d (%s)\n", i + 1, i + 1, objs[i].name);
		fprintf(pt, "\t.align %d\n", PGSZ_BITS);
#endif
		for (j = 0; j <= high_page; j++) {
			found = 0;
			for (k = 0; k < NUM_SECTS; k++) {
				if ((j >= VA2PG(objs[i].scn_addr[k])) && (j <= VA2PG(objs[i].scn_end[k])) && (objs[i].scn_size[k] != 0)) {
					page =  VA2PG(objs[i].start_addr[k]) + (j - VA2PG(objs[i].scn_addr[k]));
					enlo = page << TLBLO_PFNSHIFT;
					enlo |= PG_FLAGS;
					if (k == TEXT_S)
						enlo |= tcoher << TLBLO_CSHIFT;
					else		
						enlo |= dcoher << TLBLO_CSHIFT;

					/* 
					   In R4000 we used a double word composed of the enlo value in the top half and
					   0 in the bottom half.  In niblet.s we load just the top half with a lw, and
					   don't really use the bottom half.  Don't know why they structured their page
					   table this way.  Seems like it wastes space.  I've changed this now so that
					   in R4000-land we just use a double word enlo entry.

					   Scn_mem used to be declared as a pointer to an int since we were storing
					   two words into the R4000's page table.  When I ported this to TFP, I changed
					   it to a pointer to a long, so in R4000-land it would still be a pointer to
					   a 32 bit value and in TFP int would be a pointer to a 64 bit value.  But when
					   I added in the RAW_NIBLET stuff, I began needing scn_mem to always be able
					   to hold a 64 bit value even when run on the home machines 32 bit OS.  So I changed
					   scn_mem to just be of type addr_type, which is always 64 bits, and I cast it
					   to a pointer when I need to use it as an array.  It is never used as an array
					   in RAW_NIBLET. - Margie
				   */
#ifdef RAW_NIBLET
/*
					fprintf(pt, "\t.word\t0x%x, 0x%x\t", 0, enlo);
					fprintf(pt, "# (%s VPN 0x%x PFN 0x%x)\n", scn_names[k], j, page);
*/
					fprintf(pt, "\t.dword\t0x%llx\t", enlo);
					fprintf(pt, "# (%s VPN 0x%x PFN 0x%x)\n", scn_names[k], j, page);

#else
					((long long *) scn_mem)[j] = enlo;
#endif

#if defined(DCOBDEBUG) || defined(VERBOSE)
					/*
					 * Note, we would like to print &scn_mem[j] here, but we can't because
					 * scn_mem is a 64 bit address, but when running on a 32bit OS &scn_mem[j]
					 * is only 32 bits.  So we do the pointer arithmetic ourselves. - Margie
					 */
					loprintf("page_tbl: p%d *(%llx) == %llx (%s VPN 0x%x PFN 0x%x)\n", 
							 i+1, scn_mem + (j * 8), enlo,
							 scn_names[k], j, page);
#endif
					found = 1;
				} 
			}
			if (!found) {
#ifndef RAW_NIBLET
				((tm_addr_type *) scn_mem)[j] = 0;
#else
				fprintf(pt, "\t.dword\t0x0\t\t# (VPN 0x%x Invalid)\n", j);
#endif

			}
		}
		*cur_page += high_page * 8;	/* One doubleword per PTE */

		/*
		 * The setup of ENLO and ENHI in R4000-land encourages the kernel to load two pages at
		 * a time.  Thus, if a section takes up an odd number of pages, when we get to this
		 * point in the code we have set of a page table with an odd number of entries.  Since
		 * Niblet's tlb code always loads two entries, we must add an extra entry that it
		 * invalid.  The following if-expression just says "if the section contains an odd number
		 * of pages".
		 */
		if (!(((high_page * 8) / PG_SIZE) & 1)) {

#ifdef CACHED
			scn_mem = ((*cur_page) & ADDRSPACE_ADDRESS) | ADDRSPACE_RGN_UNMAPPED_CACHED;
#else
			scn_mem = ((*cur_page) & ADDRSPACE_ADDRESS) | ADDRSPACE_RGN_UNMAPPED_UNCACHED;
#endif /* CACHED */

			page_align(cur_page);
			clear_mem(*cur_page, ((long)*cur_page) + PG_SIZE);

			(*cur_page) += PG_SIZE;
#ifdef DCOBDEBUG
			loprintf("\n\t.align %d\n", PGSZ_BITS);
			loprintf("\t.word 0\t\t# Makes it OK to load two consecutive pgs\n");
#endif
#ifdef RAW_NIBLET
			fprintf(pt, "\n\t.align %d\n", PGSZ_BITS);
			fprintf(pt, "\t.word 0\t\t# Makes it OK to load two consecutive pgs\n");
#endif

		} /* if (odd number of pages) */

        page_align(cur_page);
	
#ifdef DCOBDEBUG
		loprintf("page_tbl: cur_page == 0x%llx\n", *cur_page);
#endif
		objs[i].pg_tbl_end = ((*cur_page) & ADDRSPACE_ADDRESS) | ADDRSPACE_RGN_UNMAPPED_CACHED;
    } /* for (i == num_prgs) */
#ifdef RAW_NIBLET
    fclose(pt);
#endif
}

#ifndef RAW_NIBLET


/* keep as unsigned int * because we are moving words between source and dest */
static void copy_code(unsigned int *dest_start, 
					  unsigned int *dest_end,
					  unsigned long source_off)  
{
	unsigned int *source;  

	/* Convert the base anf offset into a source pointer */
	source = &(niblet_code[source_off / sizeof(unsigned int)]);

#ifdef DCOBDEBUG
	loprintf("Copy: source_off = 0x%x, source = 0x%x\n", source_off, source);
	loprintf("Copy: dest_start = 0x%x, dest_end = 0x%x\n",
							dest_start, dest_end);
#endif

/* Convert pointers to uncached space */
	dest_start = (unsigned int *) dest_addr((tm_addr_type)dest_start);
	dest_end = (unsigned int *) dest_addr((tm_addr_type)dest_end);

#ifdef DCOBDEBUG
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

	start = (unsigned int *) dest_addr((tm_addr_type)start);
	end = (unsigned int *) dest_addr((tm_addr_type)end);

#ifdef DCOBDEBUG
	loprintf("Clearing from %x to %x\n", start, end);
#endif

	while (start < end)
		store_word(start++, 0);
}


int start_niblet(tm_addr_type where, int timeout, int which_array)
{
/*	int (*niblet)(); */
	volatile int result;
	volatile unsigned int *reg_storage;
	unsigned int reg_area[68];
	evcfginfo_t *cfginfo;
	evbrdinfo_t *brd;
	int i, j;
	int num_cpus = 0;
	int overflow = 0;
	tm_addr_type directory;

	if ((tm_addr_type)(reg_area) & 0x7)
		reg_storage = &(reg_area[1]);
	else
		reg_storage = &(reg_area[0]);

	/* Initialize Niblet */
	/* Start all CPUs at niblet entry point. */
	/* Change pass and fail so all processors take an interrupt */
	/* Add timeout interrupt */
/*	niblet = (int (*)())nib_text_entry;

	loprintf("Niblet's text starts at %x, entry point at %x\n",
			nib_text_start, nib_text_entry); */

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
							KPHYSTO32K0(nib_text_entry);
					num_cpus++;
				} else {
					brd->eb_cpuarr[j].cpu_launch =
						KPHYSTO32K0(slave_return);
				}
			}
		}
	}
	if (overflow)
		loprintf("Niblet can handle a maximum of 15 CPUs.\n");

#endif /* MP */
#endif /* !IN_CACHE */
#ifdef DCOBDEBUG
	loprintf("Storing proc table address (%x) into %x\n",
			where, nib_exc_start);
#endif
	/* Store process table address before launching anyone */
	store_startup_info(STUP_PROCTBL, where);
	/* Store timeout value too */
	store_startup_info(STUP_TIMEOUT, timeout);
	/* Store the number of processors that will run this test. */
#ifdef MP
	store_startup_info(STUP_NCPUS, num_cpus);
#else
	store_startup_info(STUP_NCPUS, 1);
#endif /* MP */	
	
	/* initialize stuff that will be stored by Niblet */
	store_startup_info(STUP_DIRECTORY, 0);
	store_startup_info(STUP_WHY, 0);

	save_gprs(reg_storage);

#ifdef DCOBDEBUG
	loprintf("Invalidating Icache, Dcache, TLB\n");
#endif
#ifdef INVALIDATE_IN_DCOB
	call_asm(pon_invalidate_IDcaches, 0);
	call_asm(flush_tlb);
#endif

#ifdef INVALIDATE_IN_DCOB
#ifndef IN_CACHE
	call_asm(pon_invalidate_dcache, 0);
#else
	call_asm(pon_flush_dcache, 0);
#endif /* !IN_CACHE */
#endif

#ifdef MP

#ifdef DCOBDEBUG
	loprintf("Sending interrupt and calling Niblet.\n");
#endif
	/* Now launch all of the slaves! */
	/* Ideally, later, we should check their diag_val */
	store_double_lo(EV_SENDINT, (LAUNCH_LEVEL << 8) | PGRP_SLAVE);
#endif

#ifdef VERBOSE
	loprintf("--> Niblet (%x)\n", nib_text_entry);
	loprintf("******************************************************************\n");
#endif

/* NOTE: this calls pon_invalidate_IDcaches! */
	result = pod_jump(nib_text_entry);

	if (result)
		loprintf("Supertest FAILED.\n");
	else
		loprintf("Supertest PASSED.\n\n");

	(void)load_gprs(reg_storage);

	if (result == NIBFAIL_BARRIER) {
		loprintf("    Processor(s) did not arrive!\n");
		directory = (tm_addr_type) load_word(nib_exc_start + STUP_DIRECTORY);
		if (! (ADDRSPACE_RGN_UNMAPPED_CACHED & (unsigned long)directory)) {
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

tm_addr_type where;
int timeout = 0xffffffff;
int result;
tm_addr_type data_addr;

	if (which_array >= num_tests) {
		loprintf("*** Test index out of range.\n\n");
		return EVDIAG_TBD;
	}

#ifdef DCOBDEBUG
	printf("\nstatic_table = 0x%x\n", (tm_addr_type)static_table);
	printf("nib_exc_start = 0x%x\n", (tm_addr_type)nib_exc_start);
	printf("nib_exc_end = 0x%x\n", (tm_addr_type)nib_exc_end);
	printf("niblet_code = 0x%x\n", (tm_addr_type)niblet_code);
#endif

#ifdef INVALIDATE_IN_DCOB
#ifndef IN_CACHE
	call_asm(pon_invalidate_dcache, 0);
#else
	call_asm(pon_flush_dcache, 0);
	loprintf("Setting up scache\n");
	call_asm(pon_validate_scache, 0);
#endif /* IN_CACHE */
#endif

#ifdef R4000
/* Margie: this would not link, so removed it for now.  Check this out.  FIX */
#if 1
	call_asm(pon_zero_icache, 0x80100000);
#else
	zero_icache();
#endif
#endif

	data_addr = (nib_sbss_end > nib_bss_end) ? nib_sbss_end : nib_bss_end;
/*	data_addr = nib_bss_end; */
	page_align(&data_addr);

	if (dcob(static_table, test_array[which_array],
			data_addr,
			&where, QUANTUM, TEXT_COHERENCY, DATA_COHERENCY)) {
		loprintf("*** Error in test array\n");
		return 1;
	}

#ifdef DCOBDEBUG
	loprintf("Copying vectors...                      \n");
	loprintf("\n");
#endif

/* remove for running on system! */
	copy_code((unsigned int *)(TLB_REFILL_HANDLER_LOC_UNCACHED),
		(unsigned int*)(TLB_REFILL_HANDLER_LOC_UNCACHED + nib_exc_end - nib_exc_start),
		nib_exc_off);
#if 0
	copy_code((unsigned int *)(ADDRSPACE_RGN_UNMAPPED_CACHED),
		(unsigned int*)(ADDRSPACE_RGN_UNMAPPED_CACHED + nib_exc_end - nib_exc_start),
		nib_exc_off);
#endif

#ifdef DCOBDEBUG
	loprintf("Copying text...     \n");
	loprintf("\n");
#endif
	copy_code((unsigned int *)nib_text_start,
			(unsigned int*)nib_text_end, nib_text_off);
	
#ifdef DCOBDEBUG
	loprintf("Copying data...\n");
	loprintf("\n");
#endif
	copy_code((unsigned int *)nib_data_start,
			(unsigned int*)nib_data_end, nib_data_off);

#ifdef DCOBDEBUG
	loprintf("Clearing BSS...\n");
	loprintf("\n");
#endif

	clear_mem((unsigned int *)nib_bss_start,
			(unsigned int*)nib_bss_end);
	
#ifdef DCOBDEBUG
	loprintf("Clearing SBSS...\n");
	loprintf("\n");
#endif
	clear_mem((unsigned int *)nib_sbss_start,
			(unsigned int*)nib_sbss_end);

#ifdef DCOBDEBUG
	loprintf("Starting Niblet\n");
#endif
	where &= ADDRSPACE_ADDRESS;
	where |= ADDRSPACE_RGN_UNMAPPED_CACHED;
#ifdef DCOBDEBUG
	printf("Niblet proc table resides at 0x%x\n", where);
#endif

	result = start_niblet(where, timeout, which_array);

	printf("Niblet Complete.\n");
	return result;
}


#endif RAW_NIBLET
