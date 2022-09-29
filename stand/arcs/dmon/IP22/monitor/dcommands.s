#ident	"$Id: dcommands.s,v 1.1 1994/07/21 00:18:02 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/



/*
 * Monitor Commands 
 *
 * Command Interface:
 *
 * Invocation syntax
 *	int cmd_x(char *env_nvptr, int parm1)
 *	where parm1 is from the command table
 *
 * Return status
 *	0 - OK, 1 - Invalid command syntax or Error
 */

#include "sys/regdef.h"
#include "sys/fpregdef.h"
/*#include "mips/dregdef.h"
#include "mips/cpu.h"
#include "mips/cpu_board.h"
*/
#include "sys/sbd.h"
#include "sys/asm.h"
/*#include "mips/mk48t02.h"*/
#include "pdiag.h"
#include "monitor.h"


/*
 * int cmd_config(char *env_nvptr, int parm1)
 *
 * Display sys hardware configurations
 *
 */
LEAF(cmd_config)
	move	s0, ra

	PRINT(" \n\nCPU/PRID= 0x")
	.set	noreorder
	mfc0	a0, C0_PRID
	nop
	.set	reorder
	move	a1, zero
	jal	putbcd

	.set	noreorder
	mfc0	a0, C0_SR		# print FPA/Rev only if SR_CU1=1
	nop
	.set	reorder
	and	a0, SR_CU1
	beq	a0, zero, 1f

	PRINT(" FPA/Rev= 0x")
	.set	noreorder
	cfc1	a0, zero	# read FPA's rev
	nop
	.set	reorder
	move	a1, zero
	jal	putbcd
1:
	PRINT("\nCONFIG= 0x")
	.set	noreorder
	mfc0	a0, C0_CONFIG
	nop
	.set	reorder
	move	a1, zero
	jal	putbcd

	jal	icache_size
	move	s1, v0
	PRINT("\nI-Cache Size: ")
	PRINT(" (0x")
	PUTHEX(s1)
	PRINT(") ")
	srl	s1, 10			# number of KB
	PUTUDEC(s1)
	PRINT(" K-Bytes ")

	jal	icache_ln_size
	move	s1, v0
	PRINT(" Line Size: ")
	PRINT(" (0x")
	PUTHEX(s1)
	PRINT(") ")
	PUTUDEC(s1)
	PRINT(" Bytes")

	jal	dcache_size
	move	s1, v0
	PRINT("\nD-Cache Size: ")
	PRINT(" (0x")
	PUTHEX(s1)
	PRINT(") ")
	srl	s1, 10			# number of KB
	PUTUDEC(s1)
	PRINT(" K-Bytes ")

	jal	dcache_ln_size
	move	s1, v0
	PRINT(" Line Size: ")
	PRINT(" (0x")
	PUTHEX(s1)
	PRINT(") ")
	PUTUDEC(s1)
	PRINT(" Bytes")

	jal	SizeMemory
	move	s1, v0
	PRINT("\nMemory Size: ")
	move	a0, s1
	li	a1, 9			# 9 digits
	jal	putudec
	PRINT(" (0x")
	PUTHEX(s1)
	PRINT(") ")
	srl	s1, 20			# number of MB
	PUTUDEC(s1)
	PRINT(" M-Bytes")

	move	v0, zero		# set normal return value
	j	s0
END(cmd_config)


/*
 * int cmd_rtag(char *env_nvptr, int parm1)
 *
 *	read primary cache tag.
 *
 * command syntax: "rtag [-d|i|s] <addr> <lines> [lpcnt] -q "
 * options:
 *	default is D-cache "-d"
 * 
 * Register Usage:
 *	a0 - nv string ptr
 *	s0 - saved ra
 *	s1 - saved address
 *	t1 - address
 *	s2 - content of TAGLO register
 *	s3 - saved option "d" or "i" or "s"
 *	t3 - option
 *	s4 - saved linesize 
 *	t4 - linesize
 *	s5 - saved number of lines to read
 *	t5 - number of lines to read
 *	s6 - saved loop counter
 *	s7 - quiet flag
 *	t6 - loop counter
 *	v0 - pass/fail return
 */
LEAF(cmd_rtag)
	.set	noreorder
	move	s0,ra

        /*
         * get dcache or icache  parameter from the command line
         */
        jal     getopt_nv       # get the option: default=="-d"
        nop
        move    s3,v0           # s3 is the option: 0==default= dcache 

        /*
         * get the virtual ADDRESS for the cache line
         */
        jal     aton_nv         # get addr
        nop
        bne     v1,zero,2f      # v1!=0 if error
        move    s1,v0           # s1 is the address

	/*
	 * Check if address is in Kseg0 space
	 */
	and	v0,v0,0xf0000000	# mask out the upper 4 bits
	li	t0,K0BASE
	sltu	t0,v0,t0
	bne	t0,zero,4f		# error if address < K0BASE
	nop
	li	t0,K1BASE
	sltu	t0,v0,t0
	beq	t0,zero,4f		# error if address > K1BASE
	nop

         /* 
	  *Get number of lines to READ, put it in s5
          */

        jal     aton_nv         # words to read
        nop
 	move    s5,v0           # s5 is the count
	bne     v1,zero,7f      # if v1!=0 (error), use default
        nop
	bne     v0,zero,6f	# aton_nv returns non-zero value, take it.
	nop	
				# aton_nv returns zero, use default

7:	or	s5,zero,1	# set up default case
	b	9f
	nop

	/*
	 * Get the number of times to execute the loop
	 */	
6:
        jal     aton_nv         # words to read
        nop
 	move    s6,v0           # s6 is the loop count
	bne     v1,zero,9f      # if v1!=0 (error), use default
        nop
	bne     v0,zero,11f	# aton_nv returns non-zero value, take it.
	nop	

9:
	or	s6, zero, 1	# setup the default

11:
        jal     getopt_nv       # get the option: default=="-q"
        nop
        move    s7,v0           # s3 is the option: 0==default, quiet mode

8:	/*
	 *loop for extended reads
	 */
	move	t5, s5
	move	t7, s1

10:
	beq	s3,zero,1f	# no option specified, use default -d
	nop
	beq	s3,char_D,1f	# [-d] select
	nop
	beq	s3,char_S,5f	# [-s] select
	nop
	bne	s3,char_I,2f	# invalid option
	nop

	jal	icache_ln_size
	nop
	mtc0	zero, C0_TAGLO
	bne	s7, zero, 99f
	move	s4,v0
	PRINT("\r\nI-cache Addr: 0x")
99:
	cache	CACH_PI | C_ILT,0(t7)	# load I tag into C0_TAGLO
	nop
	nop
	nop
	nop
	nop
	b	3f
	nop

5:	jal	scache_ln_size	
	nop
	mtc0	zero, C0_TAGLO
	bne	s7, zero, 98f

	move	s4,v0
	PRINT("\r\nS-cache Addr: 0x")
98:
	cache	CACH_SD | C_ILT,0(t7)	# load S tag into C0_TAGLO
	nop
	nop
	nop
	nop
	nop
	b	3f
	nop

1:	jal	dcache_ln_size
	nop
	mtc0	zero, C0_TAGLO
	bne	s7, zero, 97f
	move	s4,v0
	PRINT("\r\nD-cache Addr: 0x")
97:
	cache	CACH_PD | C_ILT,0(t7)	# load D tag into C0_TAGLO
	nop
	nop
	nop
	nop
	nop

3:					# common code
	mfc0	s2,C0_TAGLO
	nop
	bne	s7, zero, 96f
	PUTHEX(t7)			# echo the address
	PRINT(" --> TAGLO: 0x")
	PUTHEX(s2)

96:
	add	t7,t7,s4		# increment address
	subu	t5,1			# decrement counter
	bne	t5,zero,10b	
	nop

	subu	s6, 1			# decrement loop count
	bne	s6, zero, 8b
	nop
	/*  End of loop */        

	j	s0		# return
	and	v0,zero,0
4:
	PRINT("\r\nAddress not in Kseg0 space")
2:
	j	s0		# error return
	or	v0,zero,1
	.set	reorder
END(cmd_rtag)


/*
 * int cmd_wtag(char *env_nvptr, int parm1)
 *
 *	write primary cache tag.
 *
 * command syntax: "wtag [-d|i] <addr> <data> [lines] [lpcnt]"
 * options:
 *	default is D-cache "-d"
 * 
 * Register Usage:
 *	a0 - nv string ptr
 *	s0 - saved ra
 *	s1 - address
 *	s2 - data to be written to TAGLO
 *	s3 - option "d","i" or "s"
 *	s4 - linesize
 *	s5 - number of lines to write
 *	s6 - loop count
 *	v0 - pass/fail return
 */
LEAF(cmd_wtag)
	.set	noreorder
	move	s0,ra

        /*
         * get dcache or icache  parameter from the command line
         */
        jal     getopt_nv       # get the option: default=="-d"
        nop
        move    s3,v0           # s3 is the option: 0==default= dcache 

        /*
         * get the virtual ADDRESS for the cache line
         */
        jal     aton_nv         # get addr
        nop
        bne     v1,zero,2f      # v1!=0 if error
        move    s1,v0           # s1 is the address

	/*
	 * Check if address is in Kseg0 space
	 */

	and	v0,v0,0xf0000000	# mask out the upper 4 bits
	li	t0,K0BASE
	sltu	t0,v0,t0
	bne	t0,zero,4f		# error if address < K0BASE
	nop
	li	t0,K1BASE
	sltu	t0,v0,t0
	beq	t0,zero,4f		# error if address > K1BASE
	nop

        /*
         * get DATA from command line
         */

        jal     aton_nv         # get data
        nop
        bne     v1,zero,2f      # v1!=0 if error
        move    s2,v0           # s2 is the data
	mtc0	s2,C0_TAGLO	# move data to TAGLO
	nop

         /* 
	  *Get number of lines to READ, put it in s5
          */

        jal     aton_nv         # words to read
        nop
 	move    s5,v0           # s5 is the count
	bne     v1,zero,7f      # if v1!=0 (error), use default
        nop
	bne     v0,zero,6f	# aton_nv returns non-zero value, take it.
	nop	
				# aton_nv returns zero, use default

7:	or	s5,zero,1	# set up default case
	b	9f
	nop

	/*
	 * Get the number of times to execute the loop
	 */	
6:
        jal     aton_nv         # words to read
        nop
 	move    s6,v0           # s6 is the loop count
	bne     v1,zero,9f      # if v1!=0 (error), use default
        nop
	bne     v0,zero,8f	# aton_nv returns non-zero value, take it.
	nop	

9:
	or	s6, zero, 1	# set up default loop count
8:
	PRINT("\r\n  Writing from 0x")
	PUTHEX(s1)
	PRINT(" -> 0x")

11:
	move	t6, s1
	move	t5, s5
10:
	beq	s3,zero,1f	# no option specified, use default -d
	nop
	beq	s3,char_D,1f	# [-d] select
	nop
	beq	s3,char_S,5f	# [-s] select
	nop 
	bne	s3,char_I,2f	# invalid option
	nop

					# i-cache	
	jal	icache_ln_size
	nop
	move	s4,v0
	cache	CACH_PI | C_IST,0(t6)	# store data to I tag
	b	3f
	nop

5:	jal 	scache_ln_size		# s-cache
	nop
	move s4,v0
66:
	cache   CACH_SD | C_IST,0(t6) # store data to S tag
	b	3f
	nop

1:	jal	dcache_ln_size		# d-cache
	nop	
	move	s4,v0
	cache	CACH_PD | C_IST,0(t6)	# store data to D tag

3:
	add	t6,t6,s4	#increment address
	subu	t5,1		#decrement counter
	bne	t5,zero,10b
	nop

	subu	s6, 1			# decrement loop count
	bne	s6, zero, 11b
	nop
	PUTHEX(t6)
	/*  End of loop */

	j	s0		# return
	and	v0,zero,0
4:
	PRINT("\r\nAddress not in Kseg0 space")
2:
	j	s0		# error return
	or	v0,zero,1
	.set	reorder
END(cmd_wtag)
/* end */
