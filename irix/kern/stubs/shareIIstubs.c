/* IDENT-C */
/*
 *	$Id: shareIIstubs.c,v 1.22 1998/01/10 02:40:04 ack Exp $
 *
 *	Copyright (C) 1989-1997 Softway Pty Ltd, Sydney Australia.
 *	All Rights Reserved.
 *
 *	This is unpublished proprietary source code of Softway Pty Ltd.
 *	The contents of this file are protected by copyright laws and
 *	international copyright treaties, as well as other intellectual
 *	property laws and treaties.  These contents may not be extracted,
 *	copied,	modified or redistributed either as a whole or part thereof
 *	in any form, and may not be used directly in, or to assist with,
 *	the creation of derivative works of any nature without prior
 *	written permission from Softway Pty Ltd.  The above copyright notice
 *	does not evidence any actual or intended publication of this file.
 */
#ident	"$Revision: 1.22 $"
/*
 *	shareIIstubs.c
 *
 *	Share System Kernel Hooks stubs file.
 */
#ifdef _SHAREII

#include	<sys/shareIIstubs.h>
#include	<sys/cmn_err.h>
/*
 *	This pointer is NULL while the ShareII module is not loaded
 *	or has not initialised yet. During initialisation it is set
 *	to point at a structure populated with addresses of the hooks.
 */
struct _shareii_interface	*shareii_hook;
const char _shareii_interface_version[] =  _SHAREII_INTERFACE_VERSION;

#endif /* _SHAREII */
