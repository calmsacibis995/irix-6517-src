#ifndef lint
static char sccsid[] = 	"@(#)ypserv_ancil.c	1.3 88/06/19 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

#include <dirent.h>
#include "ypsym.h"
#include "ypdefs.h"
USE_YPDBPATH
USE_DBM

bool onmaplist();

/*
 * This checks to see whether a domain name is present at the local node as a
 * subdirectory of ypdbpath
 */
bool
ypcheck_domain(domain)
	char *domain;
{
	DIR *dirp;
	struct dirent *dp;
	char path[MAXNAMLEN + 1];
	struct stat filestat;
	bool present = FALSE;

	if ( (dirp = opendir(ypdbpath) ) == NULL) {
		return (FALSE);
	}

	while ((dp = readdir(dirp)) != NULL) {
		if (strcasecmp(dp->d_name, domain) == 0) {
			strcpy(path, ypdbpath);
			strcat(path, "/");
			strcat(path, dp->d_name);

			if (stat(path, &filestat) < 0)
				continue;
			if (filestat.st_mode & S_IFDIR) {
				present = TRUE;
				break;
			}
		}
	}

	closedir(dirp);
	
	if (present)
		strcpy(domain, dp->d_name);

	return(present);
}

/*
 * This constructs a file name from a passed domain name, a passed map name,
 * and a globally known NIS data base path prefix.
 */
void
ypmkfilename(domain, map, path)
	char *domain;
	char *map;
	char *path;
{
	char name[MAXNAMLEN];
	char *p, *q;
	DIR *dp;
	struct dirent *de;

	if ( (strlen(domain) + strlen(map) + ypdbpath_sz + 3) 
	    > (MAXNAMLEN + 1) ) {
		fprintf(stderr, "%s: Map name string too long.\n", progname);
	}

	/*
	** Simple security check.  It was possible to overwrite any dbm
	** file on the system by putting ../ in the domainname.
	*/
	if ((*domain == '.') || (*map == '.')) {
		*path = (char)0;
	} else {
		/* database path */
		for (q = path, p = ypdbpath; *p; p++, q++) {
			*q = *p;
		}
		*q++ = '/';

		/* domain */
		ypcheck_domain(domain);
		for (p = domain; *p; p++, q++) {
			*q = *p;
		}
		*q++ = '/';
		*q = (char)0;

		/* map */
		yp_make_dbname(map, name, sizeof name);
		strcat(path, name);
	}
}

/*
 * This generates a list of the maps in a domain.
 */
int
yplist_maps(domain, list)
	char *domain;
	struct ypmaplist **list;
{
	DIR *dirp;
	struct dirent *dp;
	char domdir[MAXNAMLEN + 1];
	char path[MAXNAMLEN + 1];
	int error;
	char *ext, *realname;
	struct ypmaplist *map;
	int namesz;
	extern char *yp_real_mapname();

	*list = (struct ypmaplist *) NULL;

	if (!ypcheck_domain(domain) ) {
		return (YP_NODOM);
	}
	
	(void) strcpy(domdir, ypdbpath);
	(void) strcat(domdir, "/");
	(void) strcat(domdir, domain);

	if ( (dirp = opendir(domdir) ) == NULL) {
		return (YP_YPERR);
	}

	error = YP_TRUE;
	
	for (dp = readdir(dirp); error == YP_TRUE && dp != NULL;
	    dp = readdir(dirp) ) {
		/*
		 * If it''s possible that the file name is one of the two files
		 * implementing a map, remove the extension (dbm_pag or dbm_dir)
		 */
		namesz =  strlen(dp->d_name);

		if (namesz < sizeof (dbm_pag) - 1)
			continue;		/* Too Short */

		ext = &(dp->d_name[namesz - (sizeof (dbm_pag) - 1)]);

		if (strcmp (ext, dbm_pag) != 0 && strcmp (ext, dbm_dir) != 0)
			continue;		/* No dbm file extension */

		*ext = '\0';
		ypmkfilename(domain, dp->d_name, path);

		/* 
		 * Translate from the short name to the official map name.
		 */
		realname = yp_real_mapname(dp->d_name);
		
		/*
		 * At this point, path holds the map file base name (no dbm
		 * file extension), and realname holds the official map name.
		 */
		if (ypcheck_map_existence(path) &&
		    !onmaplist(realname, *list)) {

			if ((map = (struct ypmaplist *) malloc(
			    (unsigned) sizeof (struct ypmaplist)) ) == NULL) {
				error = YP_YPERR;
				break;
			}

			map->ypml_next = *list;
			*list = map;
			namesz = strlen(realname);

			if (namesz <= YPMAXMAP) {
				(void) strcpy(map->ypml_name, realname);
			} else {
				(void) strncpy(map->ypml_name, realname,
				    namesz);
				map->ypml_name[YPMAXMAP] = '\0';
			}
		}
	}
	
	closedir(dirp);
	return(error);
}

/*
 * This returns TRUE if map is on list, and FALSE otherwise.
 */
static bool
onmaplist(map, list)
	char *map;
	struct ypmaplist *list;
{
	struct ypmaplist *scan;

	for (scan = list; scan; scan = scan->ypml_next) {

		if (strcmp(map, scan->ypml_name) == 0) {
			return (TRUE);
		}
	}

	return (FALSE);
}
