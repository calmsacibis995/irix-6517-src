/**************************************************************************
 *                                                                        *
 * Copyright (C) 1995 Silicon Graphics, Inc.                              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <regdef.h>
#include <asm.h>
#include "../computed_include/mbwc_wrap.h"

        .weakext        wcstombs,_wcstombs
        .weakext        btowc,_btowc
        .weakext        wctob,_wctob

#define wcstombs _wcstombs
#define btowc _btowc
#define wctob _wctob

#define LIBC_HANDLE __libc_mbhandle

#define GET_GLOBL_ADDR(func,name,reg) \
	USE_ALT_CP(reg) ; \
	SETUP_GP ; \
	SETUP_GP64(reg,func) ; \
	LA  reg, name

#define MBWC_wrapper(name, offs, reg, handle) \
.ent    name; \
.globl  name; \
name: \
.frame  sp, 0, ra ; \
GET_GLOBL_ADDR(name,LIBC_HANDLE,reg) ; \
PTR_L   t9, ( (_MIPS_SZPTR/8)* offs ) (reg) ;\
PTR_L   reg, (_MIPS_SZPTR/8)*handle(reg) ;\
j       t9; \
.end    name 

#define MBRWC_wrapper(name, offs, reg, handle) \
.globl  name; \
.ent    name 2; \
name: \
PTR_L   t9, ( (_MIPS_SZPTR/8)* offs ) (reg) ;\
PTR_L   reg, (_MIPS_SZPTR/8)*handle(reg) ;\
j       t9; \
.end    name 


MBWC_wrapper(btowc,    __MBWC_btowc,    a1, 0)
MBWC_wrapper(mblen,    __MBWC_mblen,    a2, 0)
MBWC_wrapper(mbstowcs, __MBWC_mbstowcs, a3, 0)
MBWC_wrapper(wctob,    __MBWC_wctob,    a1, 1)
MBWC_wrapper(wctomb,   __MBWC_wctomb,   a2, 1)
MBWC_wrapper(wcstombs, __MBWC_wcstombs, a3, 1)

	.ent	mbtowc
	.globl	mbtowc
mbtowc:
	.frame  sp, 0, ra
	GET_GLOBL_ADDR(mbtowc,LIBC_HANDLE,v0)
	beq	a0, 0, 1f	    # a0 is outstr - if nul it's mblen
 # call mbtowc

	PTR_L   t9, ( (_MIPS_SZPTR/8)* __MBWC_mbtowc ) (v0)
	PTR_L   a3, (_MIPS_SZPTR/8)* 0(v0) ;
	j       t9;
1:
 # call mblen
	or	a0, a1, zero
	or	a1, a2, zero
	PTR_L   t9, ( (_MIPS_SZPTR/8)* __MBWC_mblen ) (v0)
	PTR_L   a2, (_MIPS_SZPTR/8)* 0(v0)
	j	t9

	.end	mbtowc

