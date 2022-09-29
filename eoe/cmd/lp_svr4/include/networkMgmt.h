/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/include/RCS/networkMgmt.h,v 1.1 1992/12/14 13:23:03 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ifndef	NETWORK_MGMT_H
#define	NETWORK_MGMT_H
/*==================================================================*/
/*
*/

#include	"_networkMgmt.h"
#include	"xdrMsgs.h"
#include	"lists.h"
#include	"boolean.h"

typedef	struct
{
	int	size;
	void	*data_p;

}  dataPacket;

/*------------------------------------------------------------------*/
/*
**	Interface definition.
*/
#if (defined(__STDC__) || (__SVR4__STDC))

int		SendJob (connectionInfo *, list *, list *, list *);
int		NegotiateJobClearance (connectionInfo *);
int		ReceiveJob (connectionInfo *, list **, list **, int);
char		*ReceiveFile (connectionInfo *, fileFragmentMsg *);
void		SetJobPriority (int);
void		FreeNetworkMsg (networkMsgType, void **);
void		FreeDataPacket (dataPacket **);
boolean		JobPending (connectionInfo *);
boolean		SendFile (connectionInfo *, boolean, char *, char *);
boolean		SendData (connectionInfo *, boolean, void *, int);
boolean		EncodeNetworkMsgTag (connectionInfo *, networkMsgType);
boolean		SendJobControlMsg (connectionInfo *, jobControlCode);
boolean		SendSystemIdMsg (connectionInfo *, void *, int);
boolean		SendFileFragmentMsg (connectionInfo *, boolean,
			fileFragmentMsg *);
dataPacket	*NewDataPacket (int);
networkMsgTag	*ReceiveNetworkMsg (connectionInfo *, void **);
networkMsgTag	*DecodeNetworkMsg (connectionInfo *, void **);

#else

int		SendJob ();
int		NegotiateJobClearance ();
int		ReceiveJob ();
char		*ReceiveFile ();
void		SetJobPriority ();
void		FreeNetworkMsg ();
void		FreeDataPacket ();
boolean		JobPending ();
boolean		SendFile ();
boolean		SendData ();
boolean		EncodeNetworkMsgTag ();
boolean		SendJobControlMsg ();
boolean		SendSystemIdMsg ();
boolean		SendFileFragmentMsg ();
dataPacket	*NewDataPacket ();
networkMsgTag	*ReceiveNetworkMsg ();
networkMsgTag	*DecodeNetworkMsg ();

#endif
/*==================================================================*/
#endif
