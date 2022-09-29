/* sysALib.s - PC-386 system-dependent routines */

/* Copyright 1984-1993 Wind River Systems, Inc. */
        .data
	.globl  _copyright_wind_river
	.long   _copyright_wind_river

/*
modification history
--------------------
01q,01nov94,hdn  added a way to find out Pentium in sysCpuProbe().
		 changed a way to find out 386 by checking AC bit.
01p,19oct94,hdn  renamed sysInitGdt to sysGdt.
		 added sysLoadGdt(), sysA20on().
		 added sysA20Result indicates the state of A20 line.
01o,23sep94,hdn  deleted _sysBootType, sysRegSet(), sysRegGet().
		 added jmp instruction in sysInXX() and sysOutXX().
01n,28apr94,hdn  made sysReboot() simple.
01m,06apr94,hdn  created a processor checking routine sysCpuProbe().
		 created the system GDT at GDT_BASE_OFFSET.
01l,17feb94,hdn  changed name RAM_ENTRY to RAM_HIGH_ADRS
		 changed to put the image in upper memory.
01k,27oct93,hdn  added _sysBootType.
01j,25aug93,hdn  changed a way to enable A20.
01i,12aug93,hdn  added codes to load a user defined global descriptor table.
		 made warm start works changing sysReboot().
		 deleted sysGDTRSet().
01h,09aug93,hdn  added codes to recognize a type of cpu.
01g,17jun93,hdn  updated to 5.1.
01f,08arp93,jdi  doc cleanup.
01e,26mar93,hdn  added sysReg[GS]et, sysGDTRSet. added another sysInit.
01d,16oct92,hdn  added Code Descriptors for the nesting interrupt.
01c,29sep92,hdn  added i80387 support. deleted __fixdfsi.
01b,28aug92,hdn  fixed __fixdfsi temporary.
01a,28feb92,hdn  written based on frc386 version.
*/

/*
DESCRIPTION
This module contains system-dependent routines written in assembly
language.

This module must be the first specified in the \f3ld\f1 command used to
build the system.  The sysInit() routine is the system start-up code.

INTERNAL
Many routines in this module doesn't use the "c" frame pointer %ebp@ !
This is only for the benefit of the stacktrace facility to allow it 
to properly trace tasks executing within these routines.

SEE ALSO: 
.I "i80386 32-Bit Microprocessor User's Manual"
*/



#define _ASMLANGUAGE
#include "vxworks.h"
#include "asm.h"
#include "regs.h"
#include "syslib.h"
#include "config.h"

	/* internals */

	.globl	_sysInit	/* start of system code */
	.globl	_sysInByte
	.globl	_sysInWord
	.globl	_sysInWordString
	.globl	_sysOutByte
	.globl	_sysOutWord
	.globl	_sysOutWordString
	.globl	_sysReboot
	.globl	_sysA20on
	.globl	_sysWait
	.globl	_sysCpuProbe
	.globl	_sysLoadGdt
	.globl	_sysIdtr
	.globl	_sysGdtr
	.globl	_sysGdt


	/* externals */

	.globl	_usrInit	/* system initialization routine */

	.text
	.align 4

/*******************************************************************************
*
* sysInit - start after boot
*
* This routine is the system start-up entry point for VxWorks in RAM, the
* first code executed after booting.  It disables interrupts, sets up
* the stack, and jumps to the C routine usrInit() in usrConfig.c.
*
* The initial stack is set to grow down from the address of sysInit().  This
* stack is used only by usrInit() and is never used again.  Memory for the
* stack must be accounted for when determining the system load address.
*
* NOTE: This routine should not be called by the user.
*
* RETURNS: N/A

* sysInit ()              /@ THIS IS NOT A CALLABLE ROUTINE @/
 
*/

#ifdef	BOOTABLE
	/* vxWorks.st is directly booted from a diskette */

_sysInit:
	/* following codes are executed as 16 bits codes */

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
	movl	$_sysInit,%edi
	movl	$_end,%ecx
	subl	%edi,%ecx
	shrl	$2,%ecx
	rep
	movsl
	movl    SP_ARG1(%esp),%ebx	/* %ebx has the startType */
	jmp	warm

	/* load temporary IDT */
cold:
	aword			/* next instruction has 32bit address */
	word			/* next instruction has 32bit operand */
	cs 			/* next instruction access Code Segment */
	lidt	_sysIdtr - RAM_LOW_ADRS

	/* load temporary GDT */

	aword			/* next instruction has 32bit address */
	word			/* next instruction has 32bit operand */
	cs			/* next instruction access Code Segment */
	lgdt	_sysInitGdtr - RAM_LOW_ADRS

	/* switch to protected mode */

	mov	%cr0,%eax	/* move CR0 to EAX */
	.byte	0x66		/* next instruction has 32bit operand */
	or	$0x00000001,%eax/* set the PE bit */
	.byte	0x66		/* next instruction has 32bit operand */
	and	$0x7ffffff1,%eax/* clear the PG, TS, EM, MP bit */
	mov	%eax,%cr0	/* move EAX to CR0 */
	jmp	sysInit1	/* near jump to flush a instruction queue */

sysInit1:
	.byte	0x66		/* next instruction has 32bit operand */
	mov	$0x0010,%eax	/* a selector 0x10 is 3rd descriptor */
	mov	%ax,%ds		/* load it to DS */
	mov	%ax,%es		/* load it to ES */
	mov	%ax,%fs		/* load it to FS */
	mov	%ax,%gs		/* load it to GS */
	mov	%ax,%ss		/* load it to SS */
	.byte	0x66		/* next instruction has 32bit operand */
	mov	$0x7000,%esp	/* load 0x7000 to a stack pointer */

	aword			/* next instruction has 32bit address */
	word			/* next instruction has 32bit operand */
	cs			/* next instruction access Code Segment */
	ljmp	_sysInitJump - RAM_LOW_ADRS /* far inter segment jump */

	.align 4,0x90
_sysInitJump:
	.long	_sysInit2 - RAM_LOW_ADRS + RAM_HIGH_ADRS /* offset  : _sysInit2 */
	.word	0x0008			/* selector: 2nd descriptor */


	/* following codes are executed as 32 bits codes */

_sysInit2:
	cli				/* LOCK INTERRUPT */
	movl	$ RAM_HIGH_ADRS,%esp	/* initialized stack pointer */
	movl	$0,%ebp			/* initialized frame pointer */
	movl    $ BOOT_COLD,%ebx	/* %ebx has the startType */

	call	_sysA20on		/* enable A20 */


warm:
	cld				/* copy itself to upper memory */
	movl	$ RAM_HIGH_ADRS,%esi
	movl	$_sysInit,%edi
	movl	$_end,%ecx
	subl	%edi,%ecx
	shrl	$2,%ecx
	rep
	movsl
	jmp	sysInit3

	.text
	.align 4

	/* Global Descriptor Table Register */
_sysInitGdtr:
	.word	0x0027			/* size   : 39(8 * 5 - 1) bytes */
	.long	_sysGdt - RAM_LOW_ADRS + RAM_HIGH_ADRS /* address: _sysGdt */

#else

_sysInit:
	cli				/* LOCK INTERRUPT */
	movl    $ BOOT_WARM_AUTOBOOT,%ebx /* %ebx has the startType */

#endif	/* BOOTABLE */


sysInit3:
	movl	$_sysInit,%esp		/* initialize stack pointer */
	movl	$0,%ebp			/* initialize frame pointer */
	pushl	$0			/* initialize the %eflags */
	popfl

	pushl	$_sysGdtr		/* load the GDT */
	call	_sysLoadGdt

	pushl	%ebx			/* push the startType */
	movl	$_usrInit,%eax
	movl	$_sysInit,%edx		/* push return address */
	pushl	%edx			/*   for emulation for call */
	pushl	$0			/* push EFLAGS, 0 */
	pushl	$0x0008			/* a selector 0x08 is 2nd one */
	pushl	%eax			/* push EIP, _usrInit */
	iret				/* iret */


/*******************************************************************************
*
* sysA20on - enable A20
*
* enable A20
*
* RETURNS: N/A

* void sysA20on (void)
 
*/

	.align 4,0x90
_sysA20on:
	call	_sysWait
	movl	$0xd1,%eax		/* Write command */
	outb	%al,$0x64
	call	_sysWait

	movl	$0xdf,%eax		/* Enable A20 */
	outb	%al,$0x60
	call	_sysWait

	movl	$0xff,%eax		/* NULL command */
	outb	%al,$0x64
	call	_sysWait

	movl	$0x000000,%eax		/* Check if it worked */
	movl	$0x100000,%edx
	movl	$0x0,(%eax)
	movl	$0x0,(%edx)
	movl	$0x01234567,(%eax)
	cmpl	$0x01234567,(%edx)
	jne	sysA20on0

	/* another way to enable A20 */

	movl	$0x02,%eax
	outb	%al,$0x92

	xorl	%ecx,%ecx
sysA20on1:
	inb	$0x92,%al
	andb	$0x02,%al
	loopz	sysA20on1

	movl	$0x000000,%eax		/* Check if it worked */
	movl	$0x100000,%edx
	movl	$0x0,(%eax)
	movl	$0x0,(%edx)
	movl	$0x01234567,(%eax)
	cmpl	$0x01234567,(%edx)
	jne	sysA20on0
	movl	$-1,_sysA20Result

sysA20on0:
	ret

        .align 4,0x90
_sysA20Result:
	.long	0x00000000	/* 0 if A20 is on, -1 if not */

/*******************************************************************************
*
* sysWait - wait until the input buffer become empty
*
* wait until the input buffer become empty
*
* RETURNS: N/A

* void sysWait (void)
 
*/

	.align 4,0x90
_sysWait:
	xorl	%ecx,%ecx
sysWait0:
	movl	$0x64,%edx		/* Check if it is ready to write */
	inb	%dx,%al
	andb	$2,%al
	loopnz	sysWait0
	ret

/*******************************************************************************
*
* sysInByte - input one byte from I/O space
*
* RETURNS: Byte data from the I/O port.

* UCHAR sysInByte (address)
*     int address;	/@ I/O port address @/
 
*/

	.align 4,0x90
_sysInByte:
	movl	4(%esp),%edx
	movl	$0,%eax
	inb	%dx,%al
	jmp	sysInByte0
sysInByte0:
	ret

/*******************************************************************************
*
* sysInWord - input one word from I/O space
*
* RETURNS: Word data from the I/O port.

* USHORT sysInWord (address)
*     int address;	/@ I/O port address @/
 
*/

	.align 4,0x90
_sysInWord:
	movl	4(%esp),%edx
	movl	$0,%eax
	inw	%dx,%ax
	jmp	sysInWord0
sysInWord0:
	ret

/*******************************************************************************
*
* sysOutByte - output one byte to I/O space
*
* RETURNS: N/A

* void sysOutByte (address, data)
*     int address;	/@ I/O port address @/
*     char data;	/@ data written to the port @/
 
*/

	.align 4,0x90
_sysOutByte:
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	outb	%al,%dx
	jmp	sysOutByte0
sysOutByte0:
	ret

/*******************************************************************************
*
* sysOutWord - output one word to I/O space
*
* RETURNS: N/A

* void sysOutWord (address, data)
*     int address;	/@ I/O port address @/
*     short data;	/@ data written to the port @/
 
*/

	.align 4,0x90
_sysOutWord:
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	outw	%ax,%dx
	jmp	sysOutWord0
sysOutWord0:
	ret

/*******************************************************************************
*
* sysInWordString - input word string from I/O space
*
* RETURNS: N/A

* void sysInWordString (port, address, count)
*     int port;		/@ I/O port address @/
*     short *address;	/@ address of data read from the port @/
*     int count;	/@ count @/
 
*/

	.align 4,0x90
_sysInWordString:
	pushl	%edi
	movl	8(%esp),%edx
	movl	12(%esp),%edi
	movl	16(%esp),%ecx
	cld
	rep
	insw	%dx,(%edi)
	movl	%edi,%eax
	popl	%edi
	ret

/*******************************************************************************
*
* sysOutWordString - output word string to I/O space
*
* RETURNS: N/A

* void sysOutWordString (port, address, count)
*     int port;		/@ I/O port address @/
*     short *address;	/@ address of data written to the port @/
*     int count;	/@ count @/
 
*/

	.align 4,0x90
_sysOutWordString:
	pushl	%esi
	movl	8(%esp),%edx
	movl	12(%esp),%esi
	movl	16(%esp),%ecx
	cld
	rep
	outsw	(%esi),%dx
	movl	%esi,%eax
	popl	%esi
	ret

/*******************************************************************************
*
* sysReboot - warm start
*
* RETURNS: N/A
*
* NOMANUAL

* void sysReboot ()
 
*/

	.align 4,0x90
_sysReboot:
	movl	$0,%eax
	lgdt	(%eax)		/* crash the global descriptor table */
	ret

/*******************************************************************************
*
* sysCpuProbe - initialize the CR0 and check a type of CPU
*
* RETURNS: a type of CPU; 0(386), 1(486), 2(Pentium).

* UINT sysCpuProbe (void)

*/

        .align 4,0x90
_sysCpuProbe:
        cli
        movl    %cr0,%eax
        andl    $0x7ffafff1,%eax        /* PG=0, AM=0, WP=0, TS=0, EM=0, MP=0 */
        orl     $0x60000000,%eax        /* CD=1, NW=1 */
        movl    %eax,%cr0

	pushfl
	popl	%edx
	movl	%edx,%ecx
	xorl	$0x00040000,%edx	/* AC bit */
	pushl	%edx
	popfl
	pushfl
	popl	%edx
	xorl	%edx,%ecx
        jz	sysCpuProbe0

	pushfl
	popl	%edx
	movl	%edx,%ecx
	xorl	$0x00200000,%edx	/* ID bit */
	pushl	%edx
	popfl
	pushfl
	popl	%edx
	xorl	%edx,%ecx
	jz	sysCpuProbe1

	movl	$ X86CPU_PENTIUM,%eax	/* 2 for Pentium */
	ret

sysCpuProbe1:
        movl    $ X86CPU_486,%eax	/* 1 for 486 */
        ret

sysCpuProbe0:
        movl    $ X86CPU_386,%eax	/* 0 for 386 */
	ret

/*******************************************************************************
*
* sysLoadGdt - load the global descriptor table.
*
* RETURNS: N/A
*
* NOMANUAL

* void sysLoadGdt (char *sysGdtr)
 
*/

        .align 4,0x90
_sysLoadGdt:
	movl	4(%esp),%eax
	lgdt	(%eax)
	movw	$0x0010,%ax		/* a selector 0x10 is 3rd one */
	movw	%ax,%ds	
	movw	%ax,%es
	movw	%ax,%fs
	movw	%ax,%gs
	movw	%ax,%ss
	ret

/*******************************************************************************
*
* sysGdt - the global descriptor table.
*
* RETURNS: N/A
*
* NOMANUAL
*
*/

	.text
        .align 4,0x90
_sysIdtr:
	.word	0x0000			/* size   : 0 */
	.long	0x00000000		/* address: 0 */

        .align 4,0x90
_sysGdtr:
	.word	0x0027			/* size   : 39(8 * 5 - 1) bytes */
	.long	_sysGdt

	.align 4,0x90
_sysGdt:
	/* 0(selector=0x0000): Null descriptor */
	.word	0x0000
	.word	0x0000
	.byte	0x00
	.byte	0x00
	.byte	0x00
	.byte	0x00

	/* 1(selector=0x0008): Code descriptor */
	.word	0xffff			/* limit: xffff */
	.word	0x0000			/* base : xxxx0000 */
	.byte	0x00			/* base : xx00xxxx */
	.byte	0x9a			/* Code e/r, Present, DPL0 */
	.byte	0xcf			/* limit: fxxxx, Page Gra, 32bit */
	.byte	0x00			/* base : 00xxxxxx */

	/* 2(selector=0x0010): Data descriptor */
	.word	0xffff			/* limit: xffff */
	.word	0x0000			/* base : xxxx0000 */
	.byte	0x00			/* base : xx00xxxx */
	.byte	0x92			/* Data r/w, Present, DPL0 */
	.byte	0xcf			/* limit: fxxxx, Page Gra, 32bit */
	.byte	0x00			/* base : 00xxxxxx */

	/* 3(selector=0x0018): Code descriptor, for the nesting interrupt */
	.word	0xffff			/* limit: xffff */
	.word	0x0000			/* base : xxxx0000 */
	.byte	0x00			/* base : xx00xxxx */
	.byte	0x9a			/* Code e/r, Present, DPL0 */
	.byte	0xcf			/* limit: fxxxx, Page Gra, 32bit */
	.byte	0x00			/* base : 00xxxxxx */

	/* 4(selector=0x0020): Code descriptor, for the nesting interrupt */
	.word	0xffff			/* limit: xffff */
	.word	0x0000			/* base : xxxx0000 */
	.byte	0x00			/* base : xx00xxxx */
	.byte	0x9a			/* Code e/r, Present, DPL0 */
	.byte	0xcf			/* limit: fxxxx, Page Gra, 32bit */
	.byte	0x00			/* base : 00xxxxxx */

