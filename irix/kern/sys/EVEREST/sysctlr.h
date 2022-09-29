/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * sysctlr.h -- Various definitions for the system controller/CC-uart
 * 	communications protocol.  All of the values are in hex because
 * 	the stupid assembler can't deal with character constants.
 */

#ifndef __SYS_EVEREST_SYSCTLR_H__
#define __SYS_EVEREST_SYSCTLR_H__

#ident "$Revision: 1.9 $"

/*
 * ioctl support for the /dev/sysctlr driver.
 */

#define SYSC_IOC	('s' << 8)
#define SYSC_QLEN	(SYSC_IOC|1)      /* returns length of read queue */
#define SYSC_RTOUT	(SYSC_IOC|2)      /* set the read timeout */
#define SYSC_WTOUT	(SYSC_IOC|3)      /* set the write timeout */

/*
 * Boot master arbitration protocol commands
 */

#define SC_BMREQ	0x40  /* '@' Sent by sysctlr to start bootmaster arb */
#define SC_MANUMODE	0x50  /* 'P' Sent by sysctlr in manufacturing mode */
#define SC_BMRESP	0x4b  /* 'K' Sent by CC uart to signal bootmaster */
#define SC_BMRESTART	0x12  /* (^R) Sent by CC uart to restart bm arb */

/*
 * Information acquisition commands.  The CC uart sends a get information
 * request, a sequence number, a unique identifying character which 
 * indicates what kind of information we want, and a NULL.  All responses
 * take the form: 'R' SEQUENCE_NUM DATA_BYTES NULL.
 */
#define SC_ESCAPE	0x18	/* Ctrl-X indicates following is a command */ 
#define SC_GET	 	0x47	/* 'G' Sent by CC uart to start a retrieval */
#define SC_RESP		0x52	/* 'R' First char of response from sysctlr */
#define SC_SET		0x53	/* 'S' Sent by CC uart to store a value */
#define SC_TERM		0x0a	/* LF System controller termination char */
				/* Responses are NULL ('\0') terminated */

/* The following can be both gotten and set */
#define SC_TIME		0x54	/* 'T' Time and date information */
#define SC_DEBUG	0x44	/* 'D' Magic debugging dip switch settings */
#define SC_ENV		0x45	/* 'E' Environment information */
#define SC_SERIAL	0x53	/* 'S' System serial number */
#define SC_CHECK	0x43	/* 'C' Check command lengths */

/* The following can only be gotten */
#define SC_LOG		0x4c	/* 'L' System controller event log */
#define SC_PANEL	0x50	/* 'P' Display panel type (E or T) */

/* The following can only be set */
#define SC_MESSAGE	0x4d	/* 'M' Boot Message string */
#define SC_OFF		0x4f	/* 'O' Switch off machine */
#define SC_PROCSTAT	0x50	/* 'P' Processor status information */
#define SC_HISTO 	0x48	/* 'H' Activity histogram */ 
#define SC_SNOOZE	0x5a	/* 'Z' Snooze (ask for more time) */
#define SC_KILLLOG	0x4b	/* 'K' Kill NVRAM event log */

/*
 * Alarms and warnings.  These are sent by the system controller if
 * something nasty happens.
 */

#define SC_ALARM	0x41	/* 'A' First character of alarm message */

#define SC_OVERTEMP	0x54	/* 'T' Machine is running hot */
#define SC_SWITCHOFF	0x4f	/* 'O' Front panel switch has been turned off */
#define SC_BLOWER	0x42	/* 'B' Blower failure */

#define SC_WARNING	0x57	/* 'W' A non-fatal warning event has occurred */

#define SC_COPTIMER	0x54	/* 'T' COP Timer Reset Error */
#define SC_OSC		0x43	/* 'C' Sysctlr crystal oscillator broken */
#define SC_ILLEGAL	0x49	/* 'I' Illegal op code interrupt. */
#define SC_VOLTAGE	0x56	/* 'V' Voltage exceeds inner tolerance */
#define SC_BLOWERWARN	0x42	/* 'B' Blower RPM warning */
#define SC_RESET	0x53	/* 'S' Firmware reset */

/* Status line characters */
#define SCSTAT_BOOTMASTER	'B'
#define SCSTAT_WORKING		'+'
#define SCSTAT_UNKNOWN		' '
#define SCSTAT_BROKEN		'X'
#define SCSTAT_DISABLED		'D'

#endif /* __SYS_EVEREST_SYSCTLR_H__ */
