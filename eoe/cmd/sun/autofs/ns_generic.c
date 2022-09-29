/*
 *	ns_generic.c
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <sys/param.h>
#include "autofsd.h"

extern void init_files(char **, char ***);
extern int getmapent_files(char *, char *, struct mapline *, char **, char ***);
extern int getnetmask_files();
extern int loaddirect_files(char *, char *, char *, char **, char ***);
extern int loadmaster_files(char *, char *, char **, char ***);

extern void init_nis();
extern int getmapent_nis(char *, char *, struct mapline *, char **, char ***);
extern int getnetmask_nis();
extern int loaddirect_nis(char *, char *, char *, char **, char ***);
extern int loadmaster_nis(char *, char *, char **, char ***);


getmapent(key, mapname, ml, stack, stkptr)
	char *key, *mapname;
	struct mapline *ml;
	char **stack, ***stkptr;
{
	if (strcmp(mapname, "-hosts") == 0 || 
	    strncmp(mapname,"-remount", strlen("-remount")) == 0) {
		strcpy(ml->linebuf, mapname);
		return (0);
	}

	if (*mapname == '/') 		/* must be a file */
		return (getmapent_files(key, mapname, ml, stack, stkptr));

#ifdef USE_UNS
	return(getmapent_uns(key, mapname, ml, stack, stkptr));
#else
	return(getmapent_nis(key, mapname, ml, stack, stkptr));
#endif

}

/*
 * XXX This routine should be in libnsl
 * and be supported by the ns switch.
 */
getnetmask_byaddr(netname, mask)
	char *netname, **mask;
{
	if (getnetmask_files(netname, mask))
#ifdef USE_UNS
		return(getnetmask_uns(netname, mask));
#else
		return(getnetmask_nis(netname, mask));
#endif
	return(0);
}

loadmaster_map(mapname, defopts, stack, stkptr)
	char *mapname, *defopts;
	char **stack, ***stkptr;
{
	if (*mapname == '/')		/* must be a file */
		return (loadmaster_files(mapname, defopts, stack, stkptr));

#ifdef USE_UNS
	return(loadmaster_uns(mapname, defopts, stack, stkptr));
#else
	return(loadmaster_nis(mapname, defopts, stack, stkptr));
#endif

}

loaddirect_map(mapname, localmap, defopts, stack, stkptr)
	char *mapname, *localmap, *defopts;
	char **stack, ***stkptr;
{
	if (*mapname == '/')		/* must be a file */
		return (loaddirect_files(mapname, localmap, defopts, stack, stkptr));

#ifdef USE_UNS
	return(loaddirect_uns(mapname, localmap, defopts, stack, stkptr));
#else 
	return(loaddirect_nis(mapname, localmap, defopts, stack, stkptr));
#endif
}
