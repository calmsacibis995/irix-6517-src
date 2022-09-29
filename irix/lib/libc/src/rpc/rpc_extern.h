/**************************************************************************
 *									  *
 * Copyright (C) 1986-1993 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __RPC_EXTERN_H__
#define __RPC_EXTERN_H__

/* openchild.c */
extern int _openchild(char *, FILE **, FILE **);

/* rpcdtablesize.c */
extern int _rpc_dtablesize(void);

/* get_myaddress.c */
extern void get_myaddress(struct sockaddr_in *);

/* pmap_clnt.c */
extern struct timeval _pmap_intertry_timeout;
extern struct timeval _pmap_percall_timeout;

#endif
