/* romcard.s - PC-386 ROM initialization module */

/* Copyright 1984-1991 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,27apr94,hdn  written
*/

/*
DESCRIPTION
This module contains the loader for codes programmed into a 27020 or 27040
EPROM on Blunk Microsystems's RomCard 1.0.  The entry point romcard, is the
first code executed on power-up.
This code is intended to be position independent and executed in 16bit 8086
real mode.
*/

	.globl	_romcard

	.text

_romcard:
	.byte	0x31,0xc9		/*   xor cx,cx			*/
	.byte	0x8e,0xd1		/*   mov ss,cx			*/
	.byte	0xbc,0x00,0x70		/*   mov sp,$7000		*/
	.byte	0xe8,0x00,0x00		/*   call label1		*/
					/* label1:			*/
	.byte	0x5b			/*   pop bx			*/
	.byte	0x81,0xe3,0x00,0xff	/*   and bx,$ff00		*/
	.byte	0x2e,0x89,0x0f		/*   mov cs:[bx],cx		*/
	.byte	0xb8,0x00,0x08		/*   mov ax,$800		*/
	.byte	0x8e,0xd8		/*   mov ds,ax			*/

	.byte	0x1e			/*   push ds			*/
	.byte	0xb8,0x00,0x00		/*   mov ax,$0000		*/
	.byte	0x50			/*   push ax			*/

					/* label2:			*/
	.byte	0x31,0xff		/*   xor di,di			*/
					/* label3:			*/
	.byte	0x2e,0x8b,0x81,0x00,0x02 /*  mov ax,cs:[bx+di+$200]	*/
	.byte	0x89,0x05		/*   mov [di],ax		*/
	.byte	0x47			/*   inc di			*/
	.byte	0x47			/*   inc di			*/
	.byte	0x81,0xff,0x00,0x02	/*   cmp di,$200		*/
	.byte	0x75,0xf1		/*   jne label3			*/

	.byte	0x41			/*   inc cx			*/
	.byte	0x2e,0x89,0x0f		/*   mov cs:[bx],cx		*/
	.byte	0x8c,0xd8		/*   mov ax,ds			*/
	.byte	0x05,0x20,0x00		/*   add ax,$20			*/
	.byte	0x8e,0xd8		/*   mov ds, ax			*/
	.byte	0x81,0xf9,0x00,0x04	/*   cmp cx, 1024		*/
	.byte	0x75,0xde		/*   jne label2			*/
	.byte	0xcb			/*   retf			*/

