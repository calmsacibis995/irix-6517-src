/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: compat_dfsnames.h,v $
 * Revision 65.1  1997/10/20 19:20:37  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.9.1  1996/10/02  17:54:23  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:42:08  damon]
 *
 * Revision 1.1.4.1  1994/06/09  14:13:17  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:26:28  annie]
 * 
 * Revision 1.1.2.4  1993/01/19  16:08:16  cjd
 * 	embedded copyright notice
 * 	[1993/01/19  15:11:45  cjd]
 * 
 * Revision 1.1.2.3  1992/11/24  17:55:04  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:09:59  bolinger]
 * 
 * Revision 1.1.2.2  1992/10/27  21:15:30  jaffe
 * 	Transarc delta: bab-ot5476-dfsauth-force-noauth-notify 1.3
 * 	  Selected comments:
 * 	    When it finds no default creds, the dfsauth package will now
 * 	    complete initialization in unauthenticated mode.  This delta also
 * 	    includes code to allow the bos command to take advantage of this
 * 	    feature.
 * 	    ot 5476
 * 	    Changed the name of the .p.h file.
 * 	    The first implementation wasn't really clean.  The place for the forced
 * 	    noauth mode is not really in dfsauth package, but in the client of that
 * 	    package.  The dfsauth package was, essentially restored; one routine was
 * 	    added.  And the burden of retrying in noauth mode if no credentials were
 * 	    found is moved to the end client.
 * 	    Need to set noauth for the name space, even in some cases in which
 * 	    noauth is not specified to rpc_locate_dfs_server.
 * 	[1992/10/27  14:36:55  jaffe]
 * 
 * Revision 1.1.2.2  1992/01/29  19:23:33  cfe
 * 	ensure that dfs-server principal names are used properly.
 * 	[1992/01/29  19:23:10  cfe]
 * 
 * Revision 1.1  1992/01/19  02:47:32  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/*
 *	compat_dfsnames.h -- header file for DFS name interconversion routine
 *
 * Copyright (C) 1991 Transarc Corporation
 * All rights reserved.
 */

#ifndef TRANSARC_COMPAT_DFSNAMES_H
#define TRANSARC_COMPAT_DFSNAMES_H 1

#include <dcedfs/param.h>
#include <dcedfs/stds.h>

typedef enum compat_dfs_name {
    compat_dfs_name_binding,
    compat_dfs_name_principal
} compat_dfs_name_t;


/*
 *  compat_MakeDfsName - takes a string and produces another string corresponding
 * to the input string and the type of DFS name required (binding or principal).
 * If the input string is zero-length or composed of only slashes ('/'), the string
 * is considered to be malformed and an error is returned.  If there is not enough
 * room in the output buffer to compose a name of the desired type, an error code
 * is returned.
 *
 * Parameters:
 *	baseNameP :	pointer to NULL-terminated name that is to be transformed
 * into a principal or binding name
 *	bufferP :	pointer to array of characters in which the constructed name
 * will be left
 *	bufferSize:	number of characters available in the output buffer (bufferP)
 *	desiredNameType: the type of name to be constructed in bufferP, dfs_name_binding
 * names end with the suffix "/self" and dfs_name_principal names end with the
 * suffix "/dfs-server"
 */
IMPORT long compat_MakeDfsName _TAKES((
				       char *			baseNameP,
				       char *			bufferP,
				       unsigned int		bufferSize,
				       compat_dfs_name_t	desiredNameType
				     ));

#endif /* TRANSARC_COMPAT_DFSNAMES_H */
