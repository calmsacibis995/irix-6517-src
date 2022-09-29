/* nsys.h -
 * 	Niblet system call numbers and descriptions 
 */

#define NCALLS	33
/* How many system calls are currently defined. */

#define RPARM	r5
#define RRET	r31

#define RSYS	r4
/* Register to put system call number in */
#define R_VECT	r6
#define U_RVECT		U_R6
/* Register that exception return addresses go into. */
#define SYSVECT	r7
#define U_SYSVECT	U_R7
/* Register that Niblet puts its own exception handler vector in */

#define NFAIL	0
/* Test failed.  Give up. */
#define	NPASS	1
/* Test passed.  Continue to execute remaining processes */
#define REST	2
/* Call the scheduling routine */
#define GTIME	3
/* Get current time */
#define GQUANT	4
/* Get current time quantum for this process */
#define SQUANT	5
/* Set current time quantum for this process */ 
#define ORDAIN	6
/* Switch this process into kernel mode */
#define DEFROCK	7
/* Switch this process to user mode */
#define MSG	8
/* Print a message - semantics TBD */
#define GNPROC	9
/* Get the number of processes still alive */
#define MARK	10
/* NOP system call.  For debugging with Sable. */
#define INVALID	11
/* Invalidate a random TLB entry */
#define SCOHER	12
/* Set coherency options for a page */
#define GPID	13
/* Get process id */
#define GCOUNT	14
/* Get approximate instruction count for this run */
#define PRNTHEX	15
/* Print the hex number in r2 */
#define GSHARED	16
/* Get the virtual address of the shared address space */
#define STEALE	17
/* Steal exception vector */
#define STEALI	18
/* Steal (global) interrupt vector */
#define SUSPND	19
/* Suspend scheduling */
#define RESUME	20
/* Resume scheduling */
#define WPTE	21
/* Write to one of your page table entries. */
#define GECOUNT	22
/* Get exception count for a particular exception */
#define CECOUNT	23
/* Clear exception count for a particular exception */
#define VPASS	24
/* Verbose pass */
#define CSOUT	25
/* Set up user context switch out routine */
#define CSIN	26
/* Set up user context switch in routine */
#define MODE2	27
/* Process uses MIPS II instructions and addressing */
#define MODE3	28
/* Process uses MIPS III instructions and addressing */
#define VTOP	29
/* Return the physical address corresponding to the virtual address passed */
#define TLBDUMP	30
/* Dump a tlb entry */
#define DEBUGON 31
#define DEBUGOFF 32

