/***************************************************************************
*									   *
* Copyright 1997 Adaptec, Inc.,  All Rights Reserved.			   *
*									   *
* This software contains the valuable trade secrets of Adaptec.  The	   *
* software is protected under copyright laws as an unpublished work of	   *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.	The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.							   *
*									   *
***************************************************************************/

#ifndef _LSEQ_OFF_H 
#define _LSEQ_OFF_H

#define Q_EMPTY_HEAD		0x002c
#define Q_HDRXFR_HEAD		0x0038
#define PRIMITIVE_REQ		0x0044
#define Q_SG_REQ_HEAD		0x0034
#define START_RCV_TCB		0x0040
#define Q_NEW_POINTER		0x001c
#define ARBI_REQ		0x004a
#define LIP_PRIM		0x004b
#define START_LIP_INIT		0x0000
#define START_SEND_HEAD		0x003c
#define DEST_PRIM		0x004c
#define START_LIP		0x0028
#define LINK_ERR_STATUS		0x0045
#define Q_EXE_HEAD		0x0024
#define Q_EXE_TAIL		0x0026
#define QUEUE_SIZE		0x0048
#define Q_DONE_HEAD		0x0030
#define Q_SEND_HEAD		0x0028
#define Q_DONE_BASE		0x0008
#define OS_TCB_ADDR		0x0000
#define SEQUENCER_PROGRAM	0x0000
#define Q_SEND_TAIL		0x002a
#define Q_DONE_PASS		0x0043

#endif /* _LSEQ_OFF_H */
