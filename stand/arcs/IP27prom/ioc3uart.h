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

#ident "$Revision: 1.4 $"

#ifndef	_IOC3UART_H_
#define	_IOC3UART_H_

#define IOC3UART_FLASH		12

void	ioc3uart_init(void *);
int	ioc3uart_poll(void);
int	ioc3uart_readc(void);
int	ioc3uart_getc(void);
int	ioc3uart_putc(int);
int	ioc3uart_puts(char *);

#endif /* _IOC3UART_H_ */
