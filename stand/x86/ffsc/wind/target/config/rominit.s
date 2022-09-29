/* romInit.s - PC-386 ROM initialization module */

/* Copyright 1984-1993 Wind River Systems, Inc. */
	.data
	.globl	_copyright_wind_river
	.long	_copyright_wind_river

/*
modification history
--------------------
01k,21oct94,hdn  cleaned up.
01j,23sep94,hdn  deleted _sysBootType and uses stack.
01i,06apr94,hdn  moved a processor checking routine to sysALib.s.
		 created the system GDT at GDT_BASE_OFFSET.
01h,17feb94,hdn  deleted a piece of code which copy itself to upper memory.
01g,27oct93,hdn  added _sysBootType.
01f,25aug93,hdn  changed a way to enable A20.
01e,12aug93,hdn  added codes to load a user defined global descriptor table.
01d,09aug93,hdn  added codes to recognize a type of cpu.
01c,17jun93,hdn  updated to 5.1.
01b,26mar93,hdn  added some codes to switch to the protected mode
01a,19mar92,hdn  written by modifying v01c of h32/romInit.s
*/

/*
DESCRIPTION
This module contains the entry code for the VxWorks bootrom.

The routine sysToMonitor(2) jumps to the location XXX bytes
passed the beginning of romInit, to perform a "warm boot".

This code is intended to be generic accross i80x86 boards.
Hardware that requires special register setting or memory
mapping to be done immediately, may do so here.
*/

#define _ASMLANGUAGE
#include "vxworks.h"
#include "syslib.h"
#include "asm.h"
#include "config.h"


	/* internals */

	.globl	_romInit	/* start of system code */
	.globl	_sdata		/* start of data */


	/* externals */

	.globl	_sysGdt	
	.globl	_sysGdtr	
	.globl	_sysIdtr	
	.globl	_sysA20on
	.globl	_sysLoadGdt
	.globl	_usrInit


_sdata:
	.asciz	"start of data"

	.text
	.align 4

/*******************************************************************************
*
* romInit - entry point for VxWorks in ROM
*

* romInit (startType)
*     int startType;	/@ only used by 2nd entry point @/

*/

_romInit:
	/*
	 * following codes are executed as 16 bits codes
	 */
	cli				/* LOCK INTERRUPT */
	jmp	cold

	.align 4,0x90
	cli				/* LOCK INTERRUPT */
	movl    SP_ARG1(%esp),%ebx	/* %ebx has the startType */
	jmp	warm

	.align 4,0x90
	cli				/* LOCK INTERRUPT */
	cld				/* copy itself to the entry point */
	movl	$ RAM_LOW_ADRS,%esi
	movl	$_romInit,%edi
	movl	$_end,%ecx
	subl	%edi,%ecx
	shrl	$2,%ecx
	rep
	movsl
	movl    SP_ARG1(%esp),%ebx	/* %ebx has the startType */
	jmp	warm

	/* copyright notice appears at beginning of ROM (in TEXT segment) */

	.ascii   "Copyright 1984-1993 Wind River Systems, Inc."
	.align 4

	/* load temporary IDT */
cold:
	aword			/* next instruction has 32bit address */
	word			/* next instruction has 32bit operand */
	cs 			/* next instruction access Code Segment */
	lidt	_sysIdtr - RAM_HIGH_ADRS

	/* load temporary GDT */

	aword			/* next instruction has 32bit address */
	word			/* next instruction has 32bit operand */
	cs			/* next instruction access Code Segment */
	lgdt	_sysGdtr - RAM_HIGH_ADRS

	/* switch to protected mode */

	mov	%cr0,%eax	/* move CR0 to EAX */
	.byte	0x66		/* next instruction has 32bit operand */
	or	$0x00000001,%eax/* set the PE bit */
	mov	%eax,%cr0	/* move EAX to CR0 */
	jmp	romInit1	/* near jump to flush a instruction queue */

romInit1:
	.byte	0x66		/* next instruction has 32bit operand */
	mov	$0x0010,%eax	/* a selector 0x10 is 3rd descriptor */
	mov	%ax,%ds		/* load it to DS */
	mov	%ax,%es		/* load it to ES */
	mov	%ax,%fs		/* load it to FS */
	mov	%ax,%gs		/* load it to GS */
	mov	%ax,%ss		/* load it to SS */
	.byte	0x66		/* next instruction has 32bit operand */
	mov	$0x7000,%esp	/* set a stack pointer */

	aword			/* next instruction has 32bit address */
	word			/* next instruction has 32bit operand */
	cs			/* next instruction access Code Segment */
	ljmp	_romInitJump - RAM_HIGH_ADRS /* far inter segment jump */

	.align 4,0x90
_romInitJump:
	.long	_romInit2		/* offset  : _romInit2 */
	.word	0x0008			/* selector: 2nd descriptor */


	/* following codes are executed as 32 bits codes */

_romInit2:
	cli				/* LOCK INTERRUPT */
	movl	$ STACK_ADRS,%esp	/* initialized stack pointer */
	movl	$0,%ebp			/* initialized frame pointer */
	movl    $ BOOT_COLD,%ebx	/* %ebx has the startType */

	call	_sysA20on		/* enable A20 */


warm:
	movl	$ STACK_ADRS,%esp	/* initialise the stack pointer */
	movl	$0,%ebp			/* initialise the frame pointer */
	pushl	$0			/* initialise the %eflags */
	popfl

	pushl	$_sysGdtr		/* load the GDT */
	call	_sysLoadGdt

	pushl	%ebx			/* push the startType */
	movl	$_usrInit,%eax
	movl	$_romInitHlt,%edx	/* push return address */
	pushl	%edx			/*   for emulation for call */
	pushl	$0			/* push EFLAGS, 0 */
	pushl	$0x0008			/* a selector 0x08 is 2nd one */
	pushl	%eax			/* push EIP, _usrInit */
	iret				/* iret */

	/* just in case, if there's a problem in usrInit */

_romInitHlt:
	hlt	
	
