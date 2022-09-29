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

#ident "$Revision: 1.5 $"

#ifndef	_HUBUART_H_
#define	_HUBUART_H_

#ifdef SABLE

#define HUBUART_FLASH		3

void	hubuart_init(void *);
int	hubuart_poll(void);
int	hubuart_readc(void);
int	hubuart_getc(void);
int	hubuart_putc(int);
int	hubuart_puts(char *);

#endif /* SABLE */

#endif /* _HUBUART_H_ */
