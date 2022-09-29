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

#ident "$Revision: 1.13 $"

#ifndef	_I2C_H_
#define	_I2C_H_

#if _STANDALONE
# include "rtc.h"
#else
# include "sys/clksupport.h"
# define rtc_time()	(GET_LOCAL_RTC * NSEC_PER_CYCLE / 1000)
# define rtc_sleep	us_delay
# define rtc_time_t	__uint64_t
#endif

typedef u_char			i2c_addr_t;	/* 7-bit address            */

int		i2c_init(nasid_t);

int		i2c_probe(nasid_t nasid, rtc_time_t timeout);

int		i2c_arb(nasid_t, rtc_time_t timeout, rtc_time_t *token_start);

int		i2c_master_xmit(nasid_t,
				i2c_addr_t addr,
				u_char *buf,
				int len_max,
				int *len_ptr,
				rtc_time_t timeout,
				int only_if_message);

int		i2c_master_recv(nasid_t,
				i2c_addr_t addr,
				u_char *buf,
				int len_max,
				int *len_ptr,
				int emblen,
				rtc_time_t timeout,
				int only_if_message);

int		i2c_master_xmit_recv(nasid_t,
				     i2c_addr_t addr,
				     u_char *xbuf,
				     int xlen_max,
				     int *xlen_ptr,
				     u_char *rbuf,
				     int rlen_max,
				     int *rlen_ptr,
				     int emblen,
				     rtc_time_t timeout,
				     int only_if_message);

char	       *i2c_errmsg(int code);

/*
 * Error codes
 */

#define I2C_ERROR_NONE		 0
#define I2C_ERROR_INIT		-1	/* Initialization error             */
#define I2C_ERROR_STATE		-2	/* Unexpected chip state	    */
#define I2C_ERROR_NAK		-3	/* Addressed slave not responding   */
#define I2C_ERROR_TO_ARB	-4	/* Timeout waiting for sysctlr arb  */
#define I2C_ERROR_TO_BUSY	-5	/* Timeout waiting for busy bus     */
#define I2C_ERROR_TO_SENDA	-6	/* Timeout sending address byte     */
#define I2C_ERROR_TO_SENDD	-7	/* Timeout sending data byte        */
#define I2C_ERROR_TO_RECVA	-8	/* Timeout receiving address byte   */
#define I2C_ERROR_TO_RECVD	-9	/* Timeout receiving data byte      */
#define I2C_ERROR_NO_MESSAGE	-10	/* No message was waiting	    */
#define I2C_ERROR_NO_ELSC	-11	/* ELSC is disabled for access 	    */ 	

#endif /* _I2C_H_ */
