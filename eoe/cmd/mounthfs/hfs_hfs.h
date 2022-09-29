/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __HFS_HFS_H__
#define __HFS_HFS_H__

#ident "$Revision: 1.4 $"

#include <rpc/rpc.h>
#include <sys/param.h>
#include <sys/stat.h>

#define ROOTFID  2
#define ROOTPFID 1

/* Access checks definitions */

#define AREAD	04		/* Check read access */
#define AWRITE	02		/* Check write access */
#define AEXEC	01		/* Check execute (travers) access */


u_int to_unixtime(u_int);
u_int to_hfstime(u_int);


/* Well known global hfs_ routines */

int hfs_statfs(hfs_t *hp, statfsokres *sfr);
int hfs_access(hnode_t *hp, struct authunix_parms *aup, int amode);
int hfs_getattr(hnode_t *hp, fattr *fa);
int hfs_setattr(hnode_t *hp, sattr *fa);
int hfs_lookup(hnode_t *hp, char *name, hnode_t **chp);
int hfs_link();
int hfs_readlink();
int hfs_read(hnode_t *hp, u_int offset, u_int count, char *data, u_int *nread);
int hfs_write(hnode_t *hp, u_int offset, u_int count, char *data);
int hfs_create(hnode_t *hp, char *name, struct sattr *sa,
	       hnode_t **chp, struct authunix_parms *aup);
int hfs_remove(hnode_t *hp, char *name);
int hfs_rename(hnode_t *hp, char *name, hnode_t *thp, char *tname);
int hfs_symlink();
int hfs_mkdir(hnode_t *hp, char *name, struct sattr *sa,
	      hnode_t **chp, struct authunix_parms *aup);
int hfs_rmdir(hnode_t *hp, char *name);
int hfs_readdir(hnode_t *hp, nfscookie cookie, u_int count, struct entry *entries, u_int *eof, u_int *nread);

#endif









