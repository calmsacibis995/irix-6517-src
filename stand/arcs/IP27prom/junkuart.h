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

#ifndef	_JUNKUART_H_
#define	_JUNKUART_H_

#define JUNKUART_FLASH		0x33

int	junkuart_probe(void);
void	junkuart_init(void *);
int	junkuart_poll(void);
int	junkuart_readc(void);
int	junkuart_getc(void);
int	junkuart_putc(int);
int	junkuart_puts(char *);
char   *junkuart_gets(char *, int);
void	junkuart_printf(const char *fmt, ...);

#endif /* _JUNKUART_H_ */
