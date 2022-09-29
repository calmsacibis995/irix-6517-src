/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: epi_id.h,v $
 * Revision 65.1  1997/10/20 19:20:18  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.11.1  1996/10/02  20:58:19  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:47:00  damon]
 *
 * $EndLog$
 */

/*
 *  epi_id.h - declarations and macros implementing the magical multi-sized
 *  identifiers to be used in DFS
 *
 * Copyright (C) 1994, 1991 Transarc Corporation
 * All rights reserved.
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/epi_id.h,v 65.1 1997/10/20 19:20:18 jdoak Exp $ */

#ifndef TRANSARC_EPI_ID_H
#define TRANSARC_EPI_ID_H 1

#include <dcedfs/param.h>
#include <dcedfs/stds.h>

typedef struct epi_uuid {
  u_int32		longField1;
  u_int16		shortField1;
  u_int16		shortField2;
  unsigned char		miscChars[8];
} epi_uuid_t;

#if defined(EPI_USE_FULL_ID)
typedef epi_uuid_t epi_principal_id_t;
#else /* defined(EPI_USE_FULL_ID) */
typedef u_int32 epi_principal_id_t;
#endif /* defined(EPI_USE_FULL_ID) */

/*
 * In the kernel, we get ids from struct ucreds, which are short, so that is all
 * we can compare.  In user space, we always get long ids, so we can compare the whole
 * thing.
 */
#if defined(EPI_SOURCE_IDS_ARE_32BITS)
typedef u_int32 epi_source_id_t;
#else /* defined(EPI_SOURCE_IDS_ARE_32BITS) */
typedef epi_uuid_t epi_source_id_t;	/* equiv to real uuid in size */
#endif /* defined(EPI_SOURCE_IDS_ARE_32BITS) */

/*
 * IMPORT void Epi_PrinId_ToUuid _TAKES((
 *				         epi_principal_id_t *	epiPrinIdP
 *				         epi_uuid_t *		epiUuidP
 *			               ));
 */
#define Epi_PrinId_ToUuid(epiPrinIdP, epiUuidP)			\
MACRO_BEGIN								\
  bzero((char *)(epiUuidP), sizeof(epi_uuid_t));			\
  bcopy((char *)(epiPrinIdP), (char *)(epiUuidP), sizeof(epi_principal_id_t));	\
MACRO_END     

/*
 * IMPORT void Epi_PrinId_FromUuid _TAKES((
 *				         epi_principal_id_t *		epiPrinIdP,
 *				         epi_uuid_t *			epiUuidP
 *				       ));
 */
#define Epi_PrinId_FromUuid(epiPrinIdP, epiUuidP)	\
  bcopy((char *)(epiUuidP), (char *)(epiPrinIdP), sizeof(epi_principal_id_t))


/*
 * IMPORT int Epi_PrinId_Cmp _TAKES((
 *			           (epi_principal_id_t|epi_uuid_t) *	idBlob1P,
 *			           (epi_principal_id_t|epi_uuid_t) *	idBlob2P
 *			         ));
 */
#define Epi_PrinId_Cmp(idBlob1P, idBlob2P)	\
  (bcmp((char *)(idBlob1P), (char *)(idBlob2P), sizeof(epi_source_id_t)))

/*
 * from epi_id.c:
 */
IMPORT void hton_epi_uuid _TAKES((epi_uuid_t *	epiUuidP));
IMPORT void ntoh_epi_uuid _TAKES((epi_uuid_t *	epiUuidP));
IMPORT void ntoh_epi_principal_id _TAKES((epi_principal_id_t *	epiPrinIdP));
IMPORT void hton_epi_principal_id _TAKES((epi_principal_id_t *	epiPrinIdP));

#endif /* TRANSARC_EPI_ID_H */
