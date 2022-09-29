#ident  "IP32prom csu.s"

#include <asm.h>
#include <regdef.h>
#include <fault.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <flash.h>

#if defined(IP32SIM)
#define SIM 1
#else
#undef  SIM
#endif

FIRMWARE_SP = PROM_STACK - 4*EXSTKSZ

/*********
 * fwEntry	Receives control from post1
 * fwStart	Internal entry point.
 *--------
 *		a0	Firmware dispatch code
 *		a1	Reset count
 */

 # SEGMENT_START	# reserve space for the header ...

LEAF(firmwareHeader)
	.set noreorder;					
	b	1f
	nop
	.space	FS_HDR_LEN
1:							
	.set reorder
	END(firmwareHeader)

NESTED(fwEntry,0,ra)
XNESTED(fwStart)
	move	s0,a0		# Preserve dispatch code
	move	s1,a1		# Preserve reset count
	
#if 0
 	jal	finit1		# finit(reset_count)
        
 	bgtz	sp,1f		# Skip if on user mode stack (simulation)
 	move	sp,v0		# Finit1 returns top of stack address
#endif

/*
 * Zero the BSS
 * (Everything between firstBss and PROM_STACK-64).  Note that _fbss
 * and _end and those things are broken.  If they are ever fixed, this
 * should just clear from _fbss to _end.
 * On exit from finit1, the BSS has been zeroed and we have determined
 * the firmware stack address.  The caller's stack cannot be in the bss
 * to PROM_STACK-64 region as we clobber that area.
 */
    .set    noreorder
#if !defined(SIM)        
    lui     t0, K1BASE >> 16      /* load kseg1 address space.          */
    la      t1, firstBss          /* load the firstBss address.         */
    or      a0, t0, t1            /* K0_TO_K1(firstBss)                 */
    li      t0, (PROM_STACK - 64) /* load the constant PROMSTACK - 64   */
    jal     bzero                 /* bzero((void*)K0_TO_K1(firstBss),bssSize) */
    subu    a1, t0, a0            /* unsigned bssSize = (unsigned)PROM_STACK-64 - K0_TO_K1((unsigned)firstBss); */
#endif
     
/*
 *  Declare that we are in the prom 
 */
    la      t0, _prom             /* load "_prom" variable address.     */
    li      t1, 1                 /* load constant 1                    */
    sw      t1, (t0)              /* _prom = 1;                         */
    
/*
 *  Assign value to "_fault_sp" variable.
 */
    la      t0, _fault_sp         /* load address.                      */
    li      t1, PROM_STACK        /* the value of PROM_STACK            */
    sw      t1, 0x0(t0)           /* _fault_sp = (unsigned long*)PROM_STACK; */
    
/*
 *  Now setup Firmware stack pointer.
 */
    li      sp, FIRMWARE_SP       /* sp = (unsigned long*)PROM_STACK-EXSTKSZ */

/*
 *  Initialize rtc flags.
 */
    la      t0, _rtcinitted       /* initialize the rtc initted flag.   */
    sw      zero, 0x0(t0)
    .set    reorder
    
/*
 *  This is it, we are ready to enter the firmware.
 */

1:  move    a0,s0
    move    a1,s1
    jal     firmware	# Enter the bulk of the firmware.
2:  b       1b		# Shouldn't return.
    END(fwStart)


/***********
 * getConfig	Return config register
 */
LEAF(getConfig)
	.set	noreorder
	mfc0	v0,C0_CONFIG
	.set	reorder
	jr	ra
	END(getConfig)


	
/* This hack is present because _fbss doesn't work for "ld -r -d" */
	.globl	firstBss
	LBSS(firstBss,4)		# Marker of beginning of BSS
