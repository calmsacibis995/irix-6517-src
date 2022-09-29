#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#ifndef FILTER
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <procfs/procfs.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex.h>

/*
 ******************************************************************************
 */

#define	MAX(A,B)		(((A) > (B)) ? (A) : (B))
#define	MIN(A,B)		(((A) < (B)) ? (A) : (B))
#define	TRUE			(1)
#define	FALSE			(0)
#define	MINIMUM			(0)
#define	TYPICAL			(1)
#define	MAXIMUM			(2)		
#define	BUFLEN			(1024)
#define	MAX_STATS		(20)
#define	L1LOAD			(8)
#define	QUADWORD		(16)
#define	L1BLOCKSIZE		(32)
#define	L2BLOCKSIZE		(128)
#define	STANDARDINPUT		"-"
#define	STANDARDOUTPUT		"-"
#define	_fpout			output_stream
#define	_MultiRunFiles		FALSE

/* values used to signal an uninitialized perfy_option_t */
#define	_MHz			(0)
#define _cpurev                 (0)
#define _cpuspeciesmix          (0)
#define	_IP			(-1)

#define	_range			TRUE
#define	_reuse			(0)
#define	COST(C)			(((C) < 0.0) ? -(C)*1.0e-3*options->MHz : (C))
#define	PCYCLE			(1.0e-6/options->MHz)

/*
 ******************************************************************************
 */

/*
 * Data types used by perfy.c
 */

typedef	long long	evcnt_t;

typedef	struct {
  int		perfy;
  FILE		*fpout;
  int		MultiRunFiles;
  int		IP;
  int		MHz;
  unsigned      cpu_majrev;
  int		range;
  int		reuse;
  /* XXX reorder list after testing */
  int           cpu_species_mix;
} perfy_option_t;

typedef	struct {
  int		active;
  evcnt_t	count;
} counts_t;

/*
 * Counter descriptions.
 */

#define	SYSTEM_TABLE		"/etc/perfex.costs"
#define	NCOSTS			(3)
#define	EVENT_LEN		(2)
#define MAX_EVENT_DESC_LEN	(60)
#define	NCOUNTERS		(2*16)
#define	CYCLES0			( 0)
#define	GRADUATEDINSTS0		(15)
#define	CYCLES1			(16)
#define	GRADUATEDINSTS1		(17)

static char* R10000EventDesc[NCOUNTERS] = {
      "Cycles",								
      "Issued instructions",					       	
      "Issued loads",							
      "Issued stores",							
      "Issued store conditionals",					
      "Failed store conditionals",					
      "Decoded branches",						
      "Quadwords written back from scache",				
      "Correctable scache data array ECC errors",			
      "Primary instruction cache misses",				
      "Secondary instruction cache misses",				
      "Instruction misprediction from scache way prediction table",	
      "External interventions",						
      "External invalidations",						
      "Virtual coherency conditions",					
      "Graduated instructions",						
      "Cycles",								
      "Graduated instructions",						
      "Graduated loads",  						
      "Graduated stores",						
      "Graduated store conditionals",					
      "Graduated floating point instructions",				
      "Quadwords written back from primary data cache",			
      "TLB misses",							
      "Mispredicted branches",						
      "Primary data cache misses",					
      "Secondary data cache misses",					
      "Data misprediction from scache way prediction table",		
      "External intervention hits in scache",				
      "External invalidation hits in scache",				
      "Store/prefetch exclusive to clean block in scache",		
      "Store/prefetch exclusive to shared block in scache"};

static char* R12000EventDesc[NCOUNTERS] = {
      "Cycles",								
      "Decoded instructions",					       	
      "Decoded loads",							
      "Decoded stores",							
      "Miss handling table occupancy",					
      "Failed store conditionals",					
      "Resolved conditional branches",					
      "Quadwords written back from scache",				
      "Correctable scache data array ECC errors",			
      "Primary instruction cache misses",				
      "Secondary instruction cache misses",				
      "Instruction misprediction from scache way prediction table",	
      "External interventions",						
      "External invalidations",						
      "ALU/FPU progress cycles",					
      "Graduated instructions",						
      "Executed prefetch instructions",					
      "Prefetch primary data cache misses",				
      "Graduated loads",  						
      "Graduated stores",						
      "Graduated store conditionals",					
      "Graduated floating point instructions",				
      "Quadwords written back from primary data cache",			
      "TLB misses",							
      "Mispredicted branches",						
      "Primary data cache misses",					
      "Secondary data cache misses",					
      "Data misprediction from scache way prediction table",		
      "State of intervention hits in scache",				
      "State of invalidation hits in scache",				
      "Store/prefetch exclusive to clean block in scache",		
      "Store/prefetch exclusive to shared block in scache"};

static char* CommonEventDesc[NCOUNTERS] = {
      "Cycles",								
      "[Ambiguous event]",					       	        
      "[Ambiguous event]",							
      "[Ambiguous event]",							
      "[Ambiguous event]",					                
      "[Ambiguous event]",					                
      "[Ambiguous event]",					     	        
      "Quadwords written back from scache",				
      "Correctable scache data array ECC errors",			
      "Primary instruction cache misses",				
      "Secondary instruction cache misses",				
      "Instruction misprediction from scache way prediction table",	
      "External interventions",						
      "External invalidations",						
      "ALU/FPU progress cycles",					
      "Graduated instructions",						
      "[Ambiguous event]",							
      "[Ambiguous event]",						        
      "Graduated loads",  						
      "Graduated stores",						
      "Graduated store conditionals",					
      "Graduated floating point instructions",				
      "Quadwords written back from primary data cache",			
      "TLB misses",							
      "Mispredicted branches",						
      "Primary data cache misses",					
      "Secondary data cache misses",					
      "Data misprediction from scache way prediction table",		
      "External intervention hits in scache",				
      "External invalidation hits in scache",				
      "Store/prefetch exclusive to clean block in scache",		
      "Store/prefetch exclusive to shared block in scache"};


extern	perfy_option_t		perfy_options;
#ifdef FILTER
#define output_stream stderr
#else
extern FILE* output_stream;
#endif

/*
 * These are the procedural interfaces available to perfex.
 */

extern int 
PFX_start_counters_pid(int, int, pid_t );

extern int 
PFX_start_counters_pid_all(  pid_t );

extern int 
PFX_read_counters_pid(int, evcnt_t *, int, evcnt_t *, pid_t, int);

extern int
PFX_print_counters(int, evcnt_t, int, evcnt_t);

#ifndef FILTER
extern int
PFX_print_counters_all(hwperf_cntr_t *);
#endif

extern int
PFX_print_counters_thread( int, char*, int, evcnt_t, int, evcnt_t);

#ifndef FILTER
extern int 
PFX_print_counters_thread_all( int, char*, hwperf_cntr_t *);
#endif

extern void
dump_table(perfy_option_t *options, double *pTable, FILE *fpout);

extern int
load_table(perfy_option_t *options, char *CostFileName);

extern void
print_table(perfy_option_t *options, counts_t *counts, pid_t pid, char* command);

#define MAXCPU    512

#define CPUSPECIES_PURE_R10000                1040
#define CPUSPECIES_PURE_R12000                1041
#define CPUSPECIES_MIXED_R10000_R12000        1042

extern int
system_cpu_mix(void);    

/*
 * This is the public procedural interface
 */

extern int 
start_counters_( int*, int* );

extern int 
read_counters_( int* , evcnt_t* , int* , evcnt_t* );

extern int
print_counters_(int*, evcnt_t*, int*, evcnt_t*);

extern int
print_costs_(int*, evcnt_t*, int*, evcnt_t*);

extern int
load_costs_( char* );

extern int 
start_counters(int , int );

extern int 
read_counters(int, evcnt_t *, int, evcnt_t *);

extern int
print_counters(int, evcnt_t, int, evcnt_t);

extern int
print_costs(int, evcnt_t, int, evcnt_t);

extern int
load_costs( char* );
