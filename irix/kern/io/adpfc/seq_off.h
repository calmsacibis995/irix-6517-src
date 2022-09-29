/* $Header: /proj/irix6.5.7m/isms/irix/kern/io/adpfc/RCS/seq_off.h,v 1.10 1998/09/11 21:20:05 jeremy Exp $                                                               */
/***************************************************************************
*									   *
* Copyright 1998 Adaptec, Inc.,  All Rights Reserved.			   *
*									   *
* This software contains the valuable trade secrets of Adaptec.  The	   *
* software is protected under copyright laws as an unpublished work of	   *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.	The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.							   *
*									   *
***************************************************************************/

/* Version : 45i */ 

#ifndef _SEQ_OFF_H 
#define _SEQ_OFF_H

#ifdef _ADDRESS_WIDTH32
#define SG_CACHE_PTR 		0x00a3
#define Q_EMPTY_HEAD 		0x002c
#define Q_EMPTY_TAIL 		0x002e
#define ENABLE_TCB_CHECKSUM 		0x0062
#define SET_BRK_ADDR0 		0x0318
#define Q_HDRXFR_HEAD 		0x0038
#define PRIMITIVE_REQ 		0x0046
#define Q_SG_REQ_HEAD 		0x0034
#define Q_NEW_POINTER 		0x001c
#define ARBI_REQ 		0x004a
#define LIP_PRIM 		0x004b
#define START_RCV_HEAD 		0x0040
#define Q_RESIDUAL_HEAD 		0x004e
#define START_SEND_HEAD 		0x003c
#define DEST_PRIM 		0x004c
#define IDLE_LOOP_ENTRY 		0x0000
#define IDLE_LOOP0 		0x0090
#define LINK_ERR_STATUS 		0x0047
#define Q_EXE_HEAD 		0x0024
#define Q_EXE_TAIL 		0x0026
#define QUEUE_SIZE 		0x0048
#define RUN_STATUS 		0x00d7
#define Q_DONE_HEAD 		0x0030
#define Q_SEND_HEAD 		0x0028
#define Q_DONE_BASE 		0x0008
#define OS_TCB_ADDR 		0x0000
#define SEQUENCER_PROGRAM 		0x0000
#define Q_SEND_TAIL 		0x002a
#define Q_DONE_PASS 		0x0045
#else
#define SG_CACHE_PTR		0x00a3
#define Q_EMPTY_HEAD		0x002c
#define Q_EMPTY_TAIL		0x002e
#define ENABLE_TCB_CHECKSUM	0x0062
#define SET_BRK_ADDR0 		0x0318
#define Q_HDRXFR_HEAD		0x0038
#define PRIMITIVE_REQ		0x0046
#define Q_SG_REQ_HEAD		0x0034
#define Q_NEW_POINTER		0x001c
#define ARBI_REQ		0x004a
#define LIP_PRIM		0x004b
#define START_RCV_HEAD		0x0040
#define Q_RESIDUAL_HEAD		0x004e
#define START_SEND_HEAD		0x003c
#define DEST_PRIM		0x004c
#define IDLE_LOOP_ENTRY		0x0000
#define IDLE_LOOP0 		0x0090
#define LINK_ERR_STATUS		0x0047
#define Q_EXE_HEAD		0x0024
#define Q_EXE_TAIL		0x0026
#define QUEUE_SIZE		0x0048
#define RUN_STATUS		0x00d7
#define Q_DONE_HEAD		0x0030
#define Q_SEND_HEAD		0x0028
#define Q_DONE_BASE		0x0008
#define OS_TCB_ADDR		0x0000
#define SEQUENCER_PROGRAM	0x0000
#define Q_SEND_TAIL		0x002a
#define Q_DONE_PASS		0x0045
#endif /* _ADDRESS_WIDTH32 */

#endif /* _SEQ_OFF_H */
