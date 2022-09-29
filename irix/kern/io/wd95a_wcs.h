/***********************************************************************\
*	File:		wd95a_wcs.h					*
*									*
*	Contains the defines for the Control Store for the wd95a	*
*									*
*	NOTE:  This file needs to maintain strict coordianation with	*
*	       wd95a_wcs.c						*
*									*
\***********************************************************************/

#ifndef _SYS_WD95A_WCS_
#define	_SYS_WD95A_WCS_

#ident "$Revision: 1.12 $"

/* WCS - info (Initiator) */
#define	WCS_I_SEL	0		/* initiator starts sel */
#define	WCS_I_SEL_NOMSG	0x01		/* where we tend to stop without msgs*/
#define	WCS_I_ITL_NEXUS	0x03		/* go do just a straight id */
#define	WCS_I_RESEL	0x0a		/* initiator starts resel */
#define	WCS_I_RSL_UNK	0x0a		/* unknown message on resel */
#define	WCS_I_RSL_ID	0x0b		/* ID message */
#define	WCS_I_RSL_QT	0x0c		/* que tag type message */
#define	WCS_I_RSL_QUE	0x0d		/* que tag message */
#define	WCS_I_DISPATCH	0x0e		/* initiator phase change handler */
#define	WCS_I_DATA	0x1b		/* in the midst of the data phase */
#define	WCS_I_UNK_MSG	0x24		/* unknown message in phase */
					/* possibly save/restore pointer */
#define	WCS_I_MSG_CTQ	0x25		/* wcs loc of the CTQ stop point */
#define	WCS_I_MSGI_0	0x27		/* extended message in with 0 len */
#define	WCS_I_MSGI_1	0x29		/* extended message in with 1 len */
#define	WCS_I_MSGI_2	0x2b		/* extended message in with 2 len */
#define	WCS_I_MSGI_3	0x2d		/* extended message in with 3 len */
#define	WCS_I_MSGI_4	0x2f		/* extended message in with 4 len */
#define	WCS_I_MSGI_5	0x30		/* extended message in with 5 len */
#define	WCS_I_DISC	0x33		/* disconnected from bus */
#define	WCS_I_CONCLUDE	0x46		/* starting the conclusion sequence */
#define	WCS_I_EOP_FREE	0x4d		/* waiting for bus free */
#define	WCS_I_COMPLETE	0x4e		/* completed command */
#define	WCS_I_NATN	0x64		/* negate attention, go to dispatch */
#define	WCS_I_SEL_EXT	0x70		/* selection with extended message */
#define	WCS_I_SEL_E_NM	0x71		/* selection with extended but no msg*/
#define	WCS_I_NRESEL	0x75		/* new initiator starts resel */
#define WCS_I_RSLCONT	0x75		/* after reselection, continue here */
#define	WCS_I_NRSL_ID	0x76		/* just simple id reconnect */
#define	WCS_I_SEL_NATN	0x7d		/* select without atn */

/* Initiator flag values */
#define	WCS_I_DATA_OK	(1<<0)		/* data phase ok */
#define	WCS_I_DIR	(1<<1)		/* data phase direction (set == in) */
#define	WCS_I_MULTI_MSG	(1<<2)		/* multiple byte message out */
#define	WCS_I_QUE	(1<<3)		/* queue tagged cmd */

/* Initiator Dual port reg locations */
#define	WCS_I_MSG_OUT	1		/* message out block */
#define	WCS_I_TAG_OUT	2		/* tag message for initial conn. */
#define	WCS_I_QUE_OUT	3		/* tag value for initial conn. */
#define	WCS_I_ID_OUT	0		/* Identify message out */
#define	WCS_I_MSG_O_LST	6		/* last byte of message out seq */
#define	WCS_I_MSG_OUT_L	7		/* message out length */

#define	WCS_I_ID_IN	8		/* identify message in for reconn. */
#define	WCS_I_TAG_IN	9		/* tag message for reconn. */
#define	WCS_I_QUE_IN	10		/* queue message for reconn. */

#define	WCS_I_STATUS	11		/* status byte */
#define	WCS_I_MSG_IN	12		/* message in block */

#define	WCS_I_CMD_BLK	20		/* command bytes */

/* WCS - info (Target) */
#define	WCS_T_SELECTED	0		/* where to start when sel */
#define	WCS_T_CMD_FIN	0x26		/* finish after command read */
#define	WCS_T_DATA	0x31		/* data transfer location */

/* CMP register values */
#define	WCS_T_IDENT_IDX	0		/* Identify compare regs */
#define	WCS_T_QUE_IDX	1		/* queue tag compare regs */
#define	WCS_T_CMP_TAG_IDX	2	/* compare tag index */
#define	WCS_T_CMP_LUN_IDX	3	/* compare lun index */
#define	WCS_T_GRP_7_IDX		4	/* Group 7 index */
#define	WCS_T_ID_DISC_IDX	5	/* Identify with disc regs */

#define	WCS_T_ID_AND	0xf8		/* mask value for ID regs */
#define	WCS_T_ID_XOR	0x80		/* compare value for ID regs */

#define	WCS_T_QUE_AND	0xfc		/* mask value for QUEUE regs */
#define	WCS_T_QUE_XOR	0x20		/* compare value for QUEUE regs */

#define	WCS_T_ID_D_AND	0xc0		/* mask value for ID/disc regs */
#define	WCS_T_ID_D_XOR	0xc0		/* compare value for ID/disc regs */

/* Target flag values */
#define	WCS_T_QT	(1<<0)		/* que'd tag ok */
#define	WCS_T_DWD	(1<<1)		/* disconnect on w data */
#define	WCS_T_DISC_OK	(1<<2)		/* disconnect ok */
#define	WCS_T_SDP	(1<<3)		/* SDP before Disconn. */
#define	WCS_T_TTD	(1<<4)		/* transfer data */
#define	WCS_T_DIR_OUT	(1<<5)		/* data phase dir is out */
#define	WCS_T_LINK	(1<<6)		/* Linking ok */
#define	WCS_T_LAST	(1<<7)		/* high bit set */

/* Target Dual Port Reg locations */
#define	WCS_T_MSG_OUT	0x03		/* message out block */
#define	WCS_T_ID_OUT	0x00		/* id message for initial conn. */
#define	WCS_T_TAG_OUT	0x02		/* tag message for initial conn. */
#define	WCS_T_QUE_OUT	0x01		/* tag value for initial conn. */

#define	WCS_T_ID_IN	0x07		/* id message for reconn. */
#define	WCS_T_TAG_IN	0x09		/* tag message for reconn. */
#define	WCS_T_QUE_IN	0x08		/* tag value for reconn. */

#define	WCS_T_CMD_CMPL	0x0b		/* image of message for sending */
#define	WCS_T_DISC	0x06		/* image of disconnect for sending */
#define	WCS_T_SP_MSG	0x05		/* image of save pointer for send */

#define	WCS_T_MSG_IN	0x04		/* message in block location */
#define	WCS_T_MSG_IN_L	0x0c		/* message in length */
#define	WCS_T_CMD	0x14		/* command bytes (up to 12) */
#define	WCS_T_STATUS	0x0a		/* loc for status */

#define	WCS_SIZE	128		/* maximum size in the wcs */

/* WCS values */
#define	WCS_INITIATOR	0		/* set up as initiator */
#define	WCS_SCIP	1		/* set up as target */

extern unsigned int _wcs_ini_load[], _wcs_scip_load[];

#endif	/* _SYS_WD95A_WCS_ */
