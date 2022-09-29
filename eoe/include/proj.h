/*
*
* Copyright 1995, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/

#ident "$Revision: 1.4 $"

#ifndef __PROJ_H__
#define __PROJ_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <stdio.h>


/*
 * Structures and other information for project ID processing functions
 */

/* Project ID description */
#define MAXPROJNAMELEN 32
typedef struct projid {
	char	proj_name[MAXPROJNAMELEN];	/* project name */
	prid_t	proj_id;			/* project ID */
} projid_t;

typedef void * PROJ;		/* Projects descriptor */

/* Functions for changing the project associated with a given file */
extern int chproj (const char *path, prid_t project);
extern int lchproj (const char *path, prid_t project);
extern int fchproj (int fd, prid_t project);

/* Functions for opening & closing projects descriptors */
extern PROJ openproj(const char *, const char *);
extern void closeproj(PROJ);

/* Functions for project ID <-> project name conversion */
extern prid_t fprojid(PROJ, const char *);
extern int    fprojname(PROJ, prid_t, char *, size_t);

/* Functions for enumerating project IDs */
extern int    fgetprojuser(PROJ, const char *, projid_t *, int);
extern int    fgetprojall(PROJ, projid_t *, int);

/* Utility functions for getting a new project ID */
extern prid_t fgetdfltprojuser(PROJ, const char *);
extern prid_t fvalidateproj(PROJ, const char *, const char *);

/* Variations of the above functions that do NOT use projects    */
/* descriptors. These always go to the standard system locations */
/* for project information. These have more overhead associated  */
/* with them than the above functions and are most appropriate   */
/* when they only need to be called once or twice.		 */
extern prid_t projid(const char *);
extern int    projname(prid_t, char *, size_t);
extern int    getprojuser(const char *, projid_t *, int);
extern int    getprojall(projid_t *, int);
extern prid_t getdfltprojuser(const char *);
extern prid_t validateproj(const char *, const char *);


#ifdef __cplusplus
}
#endif

#endif /* !_PROJ_H__ */
