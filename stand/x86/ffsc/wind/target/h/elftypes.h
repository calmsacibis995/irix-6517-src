/* elftypes.h - Extended Load Format types header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,29oct92,ajm  derived from Motorola delta88 elftypes.h
*/

#ifndef __INCelftypesh
#define __INCelftypesh

#ifdef __cplusplus
extern "C" {
#endif

/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

typedef unsigned long	Elf32_Addr;
typedef unsigned short	Elf32_Half;
typedef unsigned long	Elf32_Off;
typedef long		Elf32_Sword;
typedef unsigned long	Elf32_Word;

#ifdef __cplusplus
}
#endif

#endif /* __INCelftypesh */
