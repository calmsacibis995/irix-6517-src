/***********************************************************************\
*	File:		parity.s					*
*									*
*	This file contains the code to perform several memory tests     *
*	from the POD prompt.  The tests are designed to track down      *
*	parity errors in the gcache SRAM.				*
*									*
\***********************************************************************/				
	
#include "ml.h"
#include <asm.h>
#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/evconfig.h>
#include "ip21prom.h"
#include "prom_leds.h"
#include "prom_config.h"
#include "pod.h"
#include "pod_failure.h"
#include "prom_intr.h"

	.text
	.set	noreorder
	.set	noat

#define JUMP( _label )			j	_label; 	\
					nop
	
#define CALL( _label )			jal	_label; 	\
					nop

#define PARITY_DELAY( _reg )		or	_reg, zero; 	\
					ssnop; 			\
					ssnop; 			\
					ssnop

#define SAVE_IU_REGS_TO_FPU		dmtc1	$at, $f1;	\
					dmtc1	$2, $f2;	\
					dmtc1	$3, $f3;	\
					dmtc1	$4, $f4;	\
					dmtc1	$5, $f5;	\
					dmtc1	$6, $f6;	\
					dmtc1	$7, $f7;	\
					dmtc1	$8, $f8;	\
					dmtc1	$9, $f9;	\
					dmtc1	$10, $f10;	\
					dmtc1	$11, $f11;	\
					dmtc1	$12, $f12;	\
					dmtc1	$13, $f13;	\
					dmtc1	$14, $f14;	\
					dmtc1	$15, $f15;	\
					dmtc1	$16, $f16;	\
					dmtc1	$17, $f17;	\
					dmtc1	$18, $f18;	\
					dmtc1	$19, $f19;	\
					dmtc1	$20, $f20;	\
					dmtc1	$21, $f21;	\
					dmtc1	$22, $f22;	\
					dmtc1	$23, $f23;	\
					dmtc1	$24, $f24;	\
					dmtc1	$25, $f25;	\
					dmtc1	$26, $f26;	\
					dmtc1	$27, $f27;	\
					dmtc1	$28, $f28;	\
					dmtc1	$29, $f29;	\
					dmtc1	$30, $f30;	\
					dmtc1	$31, $f31
	
#define RESTORE_IU_REGS_FROM_FPU	dmfc1	$at, $f1;	\
					dmfc1	$2, $f2;	\
					dmfc1	$3, $f3;	\
					dmfc1	$4, $f4;	\
					dmfc1	$5, $f5;	\
					dmfc1	$6, $f6;	\
					dmfc1	$7, $f7;	\
					dmfc1	$8, $f8;	\
					dmfc1	$9, $f9;	\
					dmfc1	$10, $f10;	\
					dmfc1	$11, $f11;	\
					dmfc1	$12, $f12;	\
					dmfc1	$13, $f13;	\
					dmfc1	$14, $f14;	\
					dmfc1	$15, $f15;	\
					dmfc1	$16, $f16;	\
					dmfc1	$17, $f17;	\
					dmfc1	$18, $f18;	\
					dmfc1	$19, $f19;	\
					dmfc1	$20, $f20;	\
					dmfc1	$21, $f21;	\
					dmfc1	$22, $f22;	\
					dmfc1	$23, $f23;	\
					dmfc1	$24, $f24;	\
					dmfc1	$25, $f25;	\
					dmfc1	$26, $f26;	\
					dmfc1	$27, $f27;	\
					dmfc1	$28, $f28;	\
					dmfc1	$29, $f29;	\
					dmfc1	$30, $f30;	\
					dmfc1	$31, $f31

#define MYPRINT( x )			dla	a0, 99f;	\
					jal     pod_puts;       \
                                        nop;     		\
                                        .data;                  \
99:                                     .asciiz x;              \
                                        .text

#define PARITY1_CAUSE_MASK		0x0000000000030000
#define PARITY1_DATA_PATTERN		0x33555666999aaacc
#define PARITY1_LED_ADDRESS		0x9000000019004000	
						
/*\
*** gparity -- Main procedore that performs the memory test.
*** The starting address is in 'a0' and the ending address is
*** in 'a1'.
\*/
LEAF( gparity )
	/*\
	*** Announce the start of the gparity test.
	\*/
	dli	t0, 0x00000000ffffffff
	dli	t1, 0xa800000000000000
	move	s0, a0
	and	s0, t0
	or	s0, t1
	move	s1, a1
	and	s1, t0
	or	s1, t1
	move	s2, ra
	MYPRINT( "Starting G-cache parity test... Starting Address = " )
	move	a0, s0
	CALL( pod_puthex )
	MYPRINT( "... Ending Address = " )
	move	a0, s1
	CALL( pod_puthex )
	MYPRINT( "...\r\n" )

	/*\
	*** Make sure that the FPU can be used.
	\*/
	DMFC0( t0, C0_SR )
	li	t1, (SR_CU1 | SR_FR)
	or	t0, t1
	DMTC0( t0, C0_SR )		

	/*\
	*** Check to see if a parity error is pending.
	\*/
	dli	s3, 0
	DMFC0( t0, C0_CAUSE )
	dli	t1, PARITY1_CAUSE_MASK
	and	t0, t1
	beqz	t0, 2f
	nop
	MYPRINT( "...PARITY ERROR PENDING!  Error will be cleared after memory has been initialized...\r\n" )
	dli	s3, 1
2:	nop

	/*\
	*** Initialize all of the memory being tested with thestarting pattern.
	\*/
	dli	s4, PARITY1_DATA_PATTERN
	move	t0, s0
2:	sd	s4, (t0)
	daddu	t0, 8
	bltu	t0, s1, 2b
	nop
	MYPRINT( "...Memory Initialized...\r\n" )

	/*\
	*** If a gcache parity error was pending, clear it.  If not, make sure
	*** that one is not pending now.
	\*/
	beqz	s3, 2f
	nop
	MYPRINT( "...Clearing pending gcache parity error...\r\n" )
	DMTC0( zero, C0_CAUSE )
	j	3f
	nop
2:	DMFC0( t0, C0_CAUSE )
	dli	t1, PARITY1_CAUSE_MASK
	and	t0, t1
	beqz	t0, 3f
	nop
	MYPRINT( "...Pending gcache parity error after memory initialization...\r\n" )
	MYPRINT( "...Clearing pending gcache parity error...\r\n" )
	DMTC0( zero, C0_CAUSE )
3:	nop
	
	/*\
	*** Verify that the memory contains the starting pattern.
	\*/
	move	t0, s0
2:	ld	t1, (t0)
	PARITY_DELAY( t1 )
	DMFC0( t2, C0_CAUSE )
	dli	t3, PARITY1_CAUSE_MASK
	and	t2, t3
	beqz 	t2, 3f
	nop
	CALL( parity_error )
3:	beq	t1, s4, 3f
	nop
	move	ta0, s4
	CALL( data_mismatch_error )
3:	daddu	t0, 8
	bltu	t0, s1, 2b
	nop
	MYPRINT( "...Memory verified...\r\n" )

	/*\
	*** Set up values for main loop.
	\*/
	move	s5, s4
	move	s6, s4
	not	s6
	
	/*\
	*** Delta loop (v0,t3).
	\*/
	dli	v0, 8
	dli	t3, 0
2:	dli	ta0, PARITY1_LED_ADDRESS	
	sd	t3, (ta0)
	
	/*\
	*** Count loop (v1).
	\*/
	dli	v1, 0
3:	nop
	
	/*\
	*** LSW loop (a0).
	\*/
	dli	a0, 0
4:	nop
	
	/*\
	*** MSW loop (a1).
	\*/
	move	a1, s0
5:	nop
	
	/*\
	*** Read data from memory and make sure it is correct.
	\*/
	daddu	t0, a0, a1
        bgeu	t0, s1, 7f
	nop	
	ld	t1, (t0)
	PARITY_DELAY( t1 )
	DMFC0( t2, C0_CAUSE )
	dli	ta0, PARITY1_CAUSE_MASK
	and	t2, ta0
	beqz	t2, 6f
	nop
	CALL( parity_error )
6:	beq	t1, s5, 6f
	nop
	move	ta0, s5
	CALL( data_mismatch_error )
6:	nop
	
	/*\
	*** Store inverse pattern to memory.
	\*/
	sd	s6, (t0)
	daddu	k1, t0, 0x4000
	ld	zero, (k1)
        PARITY_DELAY( k1 )
        PARITY_DELAY( k1 )
        PARITY_DELAY( k1 )
        PARITY_DELAY( k1 )
        DMTC0( zero, C0_CAUSE )
	
	/*\
	*** Read inverse data from memory and make sure it is correct.
	\*/	
	daddu	t0, a0, a1
	ld	t1, (t0)
	PARITY_DELAY( t1 )
	DMFC0( t2, C0_CAUSE )
	dli	ta0, PARITY1_CAUSE_MASK
	and	t2, ta0
	beqz	t2, 6f
	nop
	CALL( parity_error )
6:	beq	t1, s6, 6f
	nop
	move	ta0, s6
	CALL( data_mismatch_error )
6:	nop
7:	nop	

	/*\
	*** MSW loop (a1).
	\*/
	daddu	a1, v0
	bltu	a1, s1, 5b
	nop

	/*\
	*** LSW loop (a0).
	\*/
	daddu	a0, 8
	bltu	a0, v0, 4b
	nop

	/*\
	*** Reverse the patterns.
	\*/
	move	ta0, s5
	move	s5, s6
	move	s6, ta0

	/*\
	*** Count loop (v1).
	\*/
	beqz	v1, 3b
	daddu	v1, 1

	/*\
	*** Count loop (v1).
	\*/
	dli	v1, 0
3:	nop
	
	/*\
	*** LSW loop (a0).
	\*/
	move	a0, v0
	dsubu	a0, a0, 8
4:	nop
	
	/*\
	*** MSW loop (a1).
	\*/
	move	a1, s1
	dsubu	a1, a1, v0
5:	nop			

	/*\
	*** Read data from memory and make sure it is correct.
	\*/
	daddu	t0, a0, a1
	bgeu	t0, s1, 7f
	nop
	ld	t1, (t0)
	PARITY_DELAY( t1 )
	DMFC0( t2, C0_CAUSE )
	dli	ta0, PARITY1_CAUSE_MASK
	and	t2, ta0
	beqz	t2, 6f
	nop
	CALL( parity_error )
6:	beq	t1, s5, 6f
	nop
	move	ta0, s5
	CALL( data_mismatch_error )
6:	nop
	
	/*\
	*** Store inverse pattern to memory.
	\*/
	sd	s6, (t0)
        daddu	k1, t0, 0x4000
	ld	zero, (k1)
	PARITY_DELAY( k1 )
        PARITY_DELAY( k1 )
        PARITY_DELAY( k1 )
        PARITY_DELAY( k1 )
        DMTC0( zero, C0_CAUSE )
	
	/*\
	*** Read inverse data from memory and make sure it is correct.
	\*/	
	daddu	t0, a0, a1
	ld	t1, (t0)
	PARITY_DELAY( t1 )
	DMFC0( t2, C0_CAUSE )
	dli	ta0, PARITY1_CAUSE_MASK
	and	t2, ta0
	beqz	t2, 6f
	nop
	CALL( parity_error )
6:	beq	t1, s6, 6f
	nop
	move	ta0, s6
	CALL( data_mismatch_error )
6:	nop
7:	nop	

	/*\
	*** MSW loop (a1).
	\*/
	dsubu	a1, a1, v0
	bltu	a1, s0, 1f
	nop
	j	5b
	nop
1:	nop
	
	/*\
	*** LSW loop (a0).
	\*/
	dsubu	a0, a0, 8
	blt	a0, zero, 1f
	nop
	j	4b
	nop
1:	nop
	
	/*\
	*** Reverse the patterns.
	\*/
	move	ta0, s5
	move	s5, s6
	move	s6, ta0
	
	/*\
	*** Count loop (v1).
	\*/
	beqz	v1, 3b
	daddu	v1, 1

	/*\
	*** Delta loop (v0).
	\*/
	dsll	v0, 1
	daddu	t3, 1
	daddu	ta0, v0, s0
	bltu	ta0, s1, 2b
	nop
	
	/*\
	*** Return.
	\*/
	MYPRINT( "...done\r\n" )
	JUMP( s2 )
	END( gparity )	

/*\
*** parity_error --- Display parity error.
\*/
LEAF( parity_error )
	/*\
	*** Save iu registers to fpu registers.
	\*/
	SAVE_IU_REGS_TO_FPU

	/*\
	*** Print error information.
	\*/
	move	s0, t0
	move	s1, t1
	move	s2, v0
	MYPRINT( "...PARITY ERROR at address 0x" )
	move	a0, s0
	CALL( pod_puthex )
	MYPRINT( " (stride = " )
	move	a0, s2
	CALL( pod_puthex )
	MYPRINT( ") (data = 0x" )
	move	a0, s1
	CALL( pod_puthex )
	MYPRINT( ")...\r\n" )

	/*\
	*** Clear cause register.
	\*/
	DMTC0( zero, C0_CAUSE )

	/*\
	*** Restore registers.
	\*/
	RESTORE_IU_REGS_FROM_FPU

	/*\
	*** Return.
	\*/
	j	ra
	nop
	END( parity_error )

/*\
*** data_mismatch_error --- Display data mismatch error.
\*/
LEAF( data_mismatch_error )		
	/*\
	*** Save iu registers to fpu registers.
	\*/
	SAVE_IU_REGS_TO_FPU

	/*\
	*** Print error information.
	\*/
	move	s0, t0
	move	s1, t1
	move	s2, v0
	move	s3, ta0
	MYPRINT( "...DATA MISMATCH ERROR at address 0x" )
	move	a0, s0
	CALL( pod_puthex )
	MYPRINT( " (stride = " )
	move	a0, s2
	CALL( pod_puthex )
	MYPRINT( ") (expected data = 0x" )
	move	a0, s3
	CALL( pod_puthex )
	MYPRINT( ") (data = 0x" )
	move	a0, s1
	CALL( pod_puthex )
	MYPRINT( ")...\r\n" )

	/*\
	*** Clear cause register.
	\*/
	DMTC0( zero, C0_CAUSE )

	/*\
	*** Restore registers.
	\*/
	RESTORE_IU_REGS_FROM_FPU

	/*\
	*** Return.
	\*/
	j	ra
	nop
	END( data_mismatch_error )
