/***********************************************************************\
*	File:		prom_leds.h					*
*									*
*	Contains the led values displayed at various phases in the	*
*	PROM startup sequence. 						*
*									*
\***********************************************************************/

#ifndef _IP19_PROM_LEDS_H_
#define _IP19_PROM_LEDS_H_

#ident "$Revision"

#define PLED_CLEARTAGS                  1
/* Clearing the primary data cache tags */

#define PLED_CKCCLOCAL                  2
/* Testing CC chip local registers */

#define PLED_CCLFAILED_INITUART         3
/* Failed the local test but trying to initialize the UART anyway */

#define PLED_CCINIT1                    4
/* Initializing the CC chip local registers */

#define PLED_CKCCCONFIG                 5
/* Testing the CC chip config registers (requires a usable bus to pass) */
/* NOTE: Hanging in this test usually means that the bus has failed.
 * 	Check the oscillator.
 */

#define PLED_CCCFAILED_INITUART         6
/* Failed the config reg test but trying to initialize the UART anyway */

#define PLED_NOCLOCK_INITUART           7
/* CC clock isn't running init uart anyway */

#define PLED_CCINIT2                    8
/* Initializing the CC chip config registers */

#define PLED_UARTINIT                   9
/* Inititalizing the CC chip UART */
/* NOTE: Hanging in this test usually means that the UART clock is bad.
 * 	Check the connection to the system controller.
 */

#define PLED_CCUARTDONE                 10
/* Finished initializing the CC chip UART */

#define PLED_CKACHIP                    11
/* Testing the A chip registers */

#define PLED_AINIT                      12
/* Initializing the A ahip */

#define PLED_CKEBUS1                    13
/* Checking the EBus with interrupts. */

#define PLED_SCINIT                     14
/* Initializing the system controller */

#define PLED_BMARB                      15
/* Arbitrating for a bootmaster */

#define PLED_BMASTER                    16
/* This processor is the bootmaster */

#define PLED_CKEBUS2                    17
/* In second EBus test.  Run only by the master */

#define PLED_POD                        18
/* Setting up this CPU slice for POD mode */

#define PLED_PODLOOP                    19
/* Entering POD loop */

#define PLED_CKPDCACHE1                 20
/* Checking the primary data cache */

#define PLED_MAKESTACK                  21
/* Creating a stack in the primary data cache */

#define PLED_MAIN                       22
/* Jumping into C code - calling main() */

#define PLED_CKIAID                     23
/* Check IA and ID chips on master IO4 */

#define PLED_CKEPC                      24
/* Check EPC chip on master IO4 */

#define PLED_IO4INIT                    25
/* Initializing the IO4 prom */

#define PLED_NVRAM                      26
/* Getting NVRAM variables */

#define PLED_FINDCONS                   27
/* Checking the path to the EPC chip which will contain the console UART */

#define PLED_CKCONS                     28
/* Testing the console UART */

#define PLED_CONSINIT                   29
/* Setting up the console UART */

#define PLED_CONFIGCPUS                 30
/* Configuring out CPUs that are disabled */

#define PLED_CKRAWMEM                   31
/* Checking out raw memory (running BIST) */

#define PLED_CONFIGMEM                  32
/* Configuring memory */

#define PLED_CKMEM                      33
/* Checking configured memory */

#define PLED_LOADPROM                   34
/* Loading IO4 prom */

#define PLED_CKSCACHE1                  35
/* First pass of secondary cache testing - test the scache like a RAM */

#define PLED_CKPICACHE                  36
/* Check the primary instruction cache */

#define PLED_CKPDCACHE2                 37
/* check the primary data cache writeback mechanism */

#define FLED_BADEAROM			38
/* Scache test 2 doesn't exist, so replace it with FLED_BADEAROM. */
/* The EAROM was corrupted and couldn't be repaired. */

#define PLED_CKBT                       39
/* Check the bus tags */

#define PLED_BTINIT                     40
/* Clear the bus tags */

#define PLED_CKPROM                     41
/* Checksum the IO prom */

#define PLED_INSLAVE                    42
/* This CPU is entering slave mode */

#define PLED_PROMJUMP                   43
/* Jumping to the IO prom */

#define PLED_SLAVEJUMP                  44
/* A slave is jumping to the IO4 PROM slave code */

#define PLED_NMIJUMP			45
/* Jumping through the NMIVEC */

/*
 * Failure mode LED values.  If the Power-On Diagnostics
 * find an unrecoverable problem with the hardware,
 * they will call the flash leds routine with one of
 * the following values as an argument.
 */

#define FLED_CANTSEEMEM			46
/* Flashed by slave processors if they take an exception while trying to
 * write their evconfig entries.  Often means the processor's getting D-chip
 * parity errors.
 */

#define FLED_NOUARTCLK			47
/* The CC UART clock is not running.  No system controller access is possible.
 */

#define	FLED_IMPOSSIBLE1 		48
/* We fell through one of the supposedly unreturning subroutines.
 * Really shouldn't be possible. 
 */

#define FLED_DEADCOP1			49	
/* Coprocessor 1 is dead - not seeing this doesn't mean it works. */

#define FLED_CCCLOCK			50	
/* The CC clock isn't running */

#define FLED_CCLOCAL			51	
/* Failed CC local register tests */

#define FLED_CCCONFIG			52
/* Failed CC config register tests */

#define FLED_ACHIP			53
/* Failed A Chip register tests */

#define FLED_BROKEWB			54
/* By the time this CPU had arrived at the bootmaster arbitration barrier,
 * the rendezvous time had passed.  Thsi implies that a CPU is running too
 * slowly, the ratio of bus clock to CPU clok rate is too high, or a bit
 * in the CC clock is stuck on.
 */

#define FLED_BADDCACHE			55
/* This CPU's primary data cache test failed */

#define FLED_BADIO4			56
/* The IO4 board is bad - can't get to the console. */

/* Exception failure mode values */
#define FLED_UTLBMISS			57
/* Took a TLB Refill exception */

#define FLED_XTLBMISS			58
/* Took an extended TLB Refill exception */

#define PLED_WRCONFIG			59	
/* Writing evconfig structure:
 *	The master CPU writes the whole array
 *	The slaves only write their own entries.
 */


#define FLED_GENERAL			60	
/* Took a general exception */

#define FLED_NOTIMPL			61	
/* Took an unimplemented exception */

#define FLED_ECC			62
/* Took a cache error exception */

#endif /* _IP19_PROM_LEDS_H_ */
