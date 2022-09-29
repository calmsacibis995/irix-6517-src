/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.6 $"

#ifndef	_NETUART_H_
#define	_NETUART_H_

#include "hub.h"

#define NETUART_FLASH		0x3f

#if _LANGUAGE_C

void	netuart_init(void *);
int	netuart_poll(void);
int	netuart_readc(void);
int	netuart_getc(void);
int	netuart_putc(int);
int	netuart_puts(char *);

void	netuart_enable(nasid_t nasid, int slice, int enable);
int	netuart_send(nasid_t nasid, int slice, int c);
int	netuart_recv(nasid_t nasid, int slice);

#endif /* _LANGUAGE_C */

#if _LANGUAGE_ASSEMBLY

#define NETUART_POLL()						\
	ld	a1, LOCAL_HUB(PI_CPU_NUM);			\
	dadd	a1, IP27PROM_INT_NETUART + 4;			\
	HUB_INT_TEST(a1);					\
	beqz	v0, 99f;						\
	 nop;							\
	ld	a1, LOCAL_HUB(PI_CPU_NUM);			\
	dadd	a1, IP27PROM_INT_NETUART + 2;			\
	HUB_INT_TEST(a1);					\
99:

#define NETUART_READC()						\
	ld	a0, LOCAL_HUB(NI_STATUS_REV_ID);		\
	dsrl	a0, NSRI_NODEID_SHFT;				\
	and	a0, NSRI_NODEID_MASK >> NSRI_NODEID_SHFT;	\
	dsll	a0, NASID_SHFT;					\
	daddu	a0, IP27PROM_NETUART + 8;			\
	ld	a1, LOCAL_HUB(PI_CPU_NUM);			\
	dsll	v0, a1, 2;					\
	dadd	a0, v0;						\
	lw	a0, 0(a0);					\
	dadd	a1, IP27PROM_INT_NETUART + 2;			\
	HUB_INT_CLEAR(a1);					\
	move	v0, a0

#endif /* _LANGUAGE_ASSEMBLY */

#endif /* _NETUART_H_ */
