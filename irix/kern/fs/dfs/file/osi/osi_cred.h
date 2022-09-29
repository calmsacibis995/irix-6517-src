/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */

/* Copyright (C) 1995, 1992 Transarc Corporation - All rights reserved */

#ifndef TRANSARC_OSI_CRED_H
#define	TRANSARC_OSI_CRED_H
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#include <dcedfs/osi_cred_mach.h>

#if OSI_HAS_CR_PAG
#define OSI_AVAILGROUPS	OSI_MAXGROUPS
#else
#define OSI_AVAILGROUPS	(OSI_MAXGROUPS - 1)
#endif

#define OSI_IS_PAG(pagval)	((((pagval) >> 24) & 0xff) == 'A')
#define	OSI_NOPAG		0xffffffff /* no-pag value */

/* Exported by osi_pag.c */

extern osi_cred_t *osi_credp;

extern u_long osi_genpag(void);
extern u_long osi_getpag(void);
extern u_long osi_GetPagFromCred(osi_cred_t *);
extern long osi_SetPagInCred(osi_cred_t *, u_long);
extern u_long osi_GetAFSPagFromCred(osi_cred_t *);
extern long osi_SetAFSPagInCred(osi_cred_t *, u_long);
#ifdef SGIMIPS
extern int  AFS_PagInCred(osi_cred_t *credp);
#endif /* SGIMIPS */

#endif /* TRANSARC_OSI_CRED_H */
