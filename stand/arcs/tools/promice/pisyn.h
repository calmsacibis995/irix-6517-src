/*	PiSyn.h - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - Parser globals
*/

extern short ps_id;			/* any unit ID if specified */
extern short ps_bank;			/* any bank# if specified */
extern char *ps_str;			/* any string argument */
extern long ps_num[PIC_NUM];	/* specific numeric argumemts */
extern long ps_numl[PIC_LST];	/* variable numeric arguments */
extern long ps_emb;			/* possible numeric in a string */
extern short ps_i;				/* count of specific numerics */
extern short ps_li;			/* count of variable numerics */
extern short ps_sli;			/* count of special variable numerics */
extern unsigned char ps_char;	/* character input holder */
extern unsigned char ps_bchr;	/* last break character */
extern unsigned char ps_bchrc;	/* break character after command */
