/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: volreg.h,v $
 * Revision 65.1  1997/10/20 19:21:48  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.430.1  1996/10/02  21:11:02  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:51:16  damon]
 *
 * $EndLog$
 */

#ifndef	_TRANSARC_VOLREG_H_
#define	_TRANSARC_VOLREG_H_

/*
 * (C) Copyright 1996, 1990 Transarc Corporation
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 *
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/volreg/RCS/volreg.h,v 65.1 1997/10/20 19:21:48 jdoak Exp $
 *
 * volreg.h
 *	Definitions/interface for manipulating the Volume Registry.
 *
 * This package now allows volumes to be registered by name as well as ID.
 * In the past, the name arguments to volreg_Enter and volreg_Delete were
 * not used; now they are.  There is also a new lookup interface,
 * volreg_LookupByName.  The original lookup routine, volreg_Lookup, is
 * unchanged; it still returns a volume given a volume ID.
 */

#include <dcedfs/volume.h>		/*Volume definitions*/


extern void volreg_Init(void);
    /*
     * Summary:
     *	  Initialize the Volume Registry package.
     *
     * Args: None.
     *
     * Returns:  None.
     */

extern long volreg_Enter(afs_hyper_t *avolid, struct volume *avolP,
			 char *avolname);
    /*
     * Summary:
     *	  Enter a volume and its information into the Volume Registry under
     *	  the given keys (`avolid' and `avolname').
     *    
     * Args:
     *    IN afs_hyper_t *avolid	  : Volume ID to enter.
     *	  IN struct volume *avolP : Ptr to above volume's descriptor.
     *	  IN char *avolname	  : Volume name to enter.
     *
     * Returns:
     *	  0		if everything went well.
     *    EEXIST	if a volume with the either the same ID or name is
     *			already in the Volume Registry.
     */

extern long volreg_ChangeIdentity(afs_hyper_t *oldvolid, afs_hyper_t *newvolid,
				  char *oldvolname, char* newvolname);
    /*
     * Summary:
     *	  Change the identity of a volume by entering it under new id and
     *	  name keys.
     *
     * Args:
     *    IN afs_hyper_t *oldvolid :
     *    IN afs_hyper_t *newvolid :
     *    IN char *oldvolname   :
     *    IN char *newvolname   :
     *
     * Returns:
     *    0		if everything went well.
     *    EEXIST	if either newvolid or newvolname is already in use.
     */

extern long volreg_Delete(afs_hyper_t *avolid, char *avolname);
    /*
     * Summary:
     *    Delete a volume entry from the Volume Registry.
     *
     * Args:
     *	  IN afs_hyper_t avolid : Volume ID to delete.
     *	  IN char *avolname  : Name of above volume to delete.
     *
     * Returns:
     *    0		if everything went well.
     *    ENODEV	if the volume wasn't found.
     */

extern long volreg_Lookup(struct afsFid *afidP, struct volume **avolPP);
    /*
     * Summary:
     *    Given a file ID, find the entry in the Volume Registry
     *	  corresponding to the volume containing the file and return a
     *	  pointer to the associated volume structure.
     *
     * Args:
     *	  IN  struct afsFid *afidP   : Ptr to the FID to look up.
     *	  OUT struct volume **avolPP : Set to the address of the volume desc
     *					associated with the given file.
     *
     * Returns:
     *	  0	 if everything went well,
     *	  ENODEV if the fid wasn't found.
     */

extern long volreg_LookupByName(char *avolname, struct volume **avolPP);
    /*
     * Summary:
     *    Given a volume name, find the entry in the Volume Registry
     *	  corresponding to that name volume containing the file and return
     *	  a pointer to the associated volume structure
     *
     * Args:
     *	  IN  char *avolname         : Ptr to the name to look up.
     *	  OUT struct volume **avolPP : Set to the address of the volume desc
     *					associated with the given file.
     *
     * Returns:
     *	  0	 if everything went well,
     *	  ENODEV if the name wasn't found.
     */

/*
 * Declare the linkage table.  If we are inside the core component, we
 * will initialize it at compile-time; outside the core, we initialize
 * at module init time by copying the core version.
 */
#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
extern void *volreg_symbol_linkage[];
#endif /* _KERNEL && AFS_SUNOS5_ENV */

#endif	/* _TRANSARC_VOLREG_H_ */
