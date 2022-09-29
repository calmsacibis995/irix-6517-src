/*	rvxcpu.h - Edit 0.2

	RomView Version 1.1a
	Copyright (C) 1990-2 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - arbitrary cpu definitions
*/
#ifdef PIRV
typedef struct
	{
	unsigned char type;			/* resource type */
	char *name;					/* resource name */
	char size;					/* resource size */
	unsigned char addr;			/* address */
	} RVREGS;


RVREGS rxregs[] = {
	0,0,0,0
	};
	
typedef struct
	{
	unsigned char type;
	char *name;
	unsigned char addr;
	} RVSFR;

RVSFR	rxsfr[] = {
	0,0
	};

typedef struct
	{
	unsigned char code;			/* opcode */
	char *mnm;					/* mnemonic */
	char ilen;					/* instruction length */
	char oprs;					/* number of operands */
	char *opr[3];				/* first operand */
	unsigned char f;			/* function code */
	} RVINST;

RVINST rxinst[] = {
	0,0,0,0,0,0,0,0
	};
#endif
