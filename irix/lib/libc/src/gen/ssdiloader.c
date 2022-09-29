/*
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

#ifndef DSHLIB
#ifdef __STDC__
	#pragma weak ssdi_get_config_and_load = _ssdi_get_config_and_load
#endif
#endif

#include "synonyms.h"

#ifndef _LIBC_NONSHARED 
/* make sense to use DSO facilities only for shared programs */
#include <libelf.h>
#include <unistd.h>
#include <limits.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <ssdi.h>


#ifdef DI_SSDILDRDEBUG
int ssdiloader_debug = 0;
#define dprintf(x) if (ssdiloader_debug) printf x
#else
#define dprintf(x)
#endif

typedef struct dsocache {
	char		srcname[_SSDI_MAXSRCNAME];
	int		*dsohndl;
} DsoCacheElem;

/*
 * All the following functions return
 * 	0 indicating success
 *	1 indicating failure.
 */

/*
 * Load the source for given database, using dynamic loading.
 * Put information about source in sip.
 */
static int
ssdi_load(struct ssdisrcinfo *sip, char *dbname, char *src)
{
	char 			dso[PATH_MAX];
	char 			ssdi_vec_sym[_SSDI_MAXSSDISYM+1];
	_SSDI_VOIDFUNC 		*ssdi_vec;
	static DsoCacheElem	*cache = NULL; /* ptr to 1 elem cache of open dso */

	if (cache == NULL) {
		cache = (DsoCacheElem *) malloc(sizeof(DsoCacheElem));
		cache->srcname[0] = NULL;
	}

	if (strcmp(cache->srcname,src) != 0) {
		/*
		 * Hard way. open the dso.
		 */
		 int 	*foo;


		sprintf(dso, "%s/%s.so", _SSDI_STD_SRC_DIR, src);
		dprintf(("ssdi_load -> dso is <%s>\n", dso));
		if ((foo = dlopen(dso, RTLD_LAZY)) == 0) {
			dprintf(("dlopen: %s: %s\n", dso, dlerror()));
			return(1);
		}
		strcpy(cache->srcname,src);
		cache->dsohndl = foo;
	}
	/*
	 * Either found in cache, or cache was updated appropriately.
	 */
	sprintf(ssdi_vec_sym, "%s_%s_ssdi_funcs", src, dbname);
	if ((ssdi_vec = dlsym(cache->dsohndl, ssdi_vec_sym)) == 0) {
		dprintf(("dlsym: %s %s: %s\n", dso, ssdi_vec_sym, dlerror()));
		return(1);
	} else {
		sip->ssdi_si_name = strdup(src);
		sip->ssdi_si_funcs = ssdi_vec;
		return(0);
	}
}



/*
 * Get config info. for given database. Only dynamic sources are
 * listed there. Ignores mal-formatted line, except if the mal-formatted
 * line contains given db-name. in this case (if line contains dbname,
 * but otherwise mal-formatted, then returns failure.
 * If db-name is not in conifg file, returns failure.
 * Otherwise, fills in srcs in dsrc list (after mallocing space),
 * and returns success.
 */
static int
get_dynamic_config(char *dbname, char **dsrclist)
{
	char 		*p1, *p2;
	int 		i, dbnamelen;
	char		format[32];
	static char 	**cfc = NULL;	/* contents of config file */
	static int	ncflines = 0;	/* how many valid lines in config file*/

	if (ncflines == 0) {
		char 		*line = NULL;
		int 		llen  = 0;
		FILE 		*fp; /* config file */
		extern char 	*__getline(FILE *, char *l, int *llen);

		if ((fp = fopen(_SSDI_CONFIG_FILE, "r")) == NULL) {
			/* Mark such that  we won't try file open again. */
			ncflines = -1;
			return 1;
		}
		if ((cfc = (char **) malloc(_SSDI_MAXDBS * sizeof(char *))) == NULL) {
			ncflines = -1;
			return 1;
		}
		while ((line = __getline(fp, line, &llen)) != NULL) {
			if (line[0] == '#')
				continue; /* Allocated line gets reused */
			cfc[ncflines++] = line;
			/* Force getline to allocate new line */
			line = NULL;
			llen = 0;
			if (ncflines == _SSDI_MAXDBS)
				break; /* Silently ignore more lines in cfile */
		}
		fclose(fp);
		if (ncflines == 0) {
			/* chronic case where all lines were comments */ 
			ncflines = -1; /* as good as cfile not being there */
			free(cfc);
			return 1;
		}
	}

	dbnamelen = (int) strlen(dbname);
	sprintf(format, "%%%ds", _SSDI_MAXSRCNAME);
	for (i = 0; i < ncflines; i++) {
		p1 = cfc[i];
		while (*p1 == ' ' || *p1 == '\t')
			p1++;
		if (strncmp(dbname, p1, dbnamelen) != 0)
			continue;
		if ((p2 = strchr(p1, ':')) == NULL)
			continue; /* mal-formatted line, ignore */

		p2++;	/* skip ':' */
		
		if ((*dsrclist = (char *) malloc(_SSDI_MAXSRCNAME)) == NULL)
			goto bad;
		while (sscanf(p2, format, *dsrclist) == 1) {
			while (*p2 == ' ' || *p2 == '\t')
				p2++;
			p2 = p2 + strlen(*dsrclist);
			if (*p2 == ' ' ||  *p2 == '\t' || *p2 == '\n' || *p2 == '\0') {
				dsrclist++;
				if ((*dsrclist = (char *) malloc(_SSDI_MAXSRCNAME)) == NULL)
					goto bad;
			} else
				goto bad; /* failure - mal-formatted line */
		}
		/* success - free last alloc'ed src; didn't read nothing there*/
		if (*dsrclist) {
			free(*dsrclist);
			*dsrclist = NULL;
		}
		return 0;
	}
bad:
	return 1;
}

/*
 * Return zero if successfully loaded a source
 */
int
ssdi_get_config_and_load(char *dbname, struct ssdiconfiginfo *ci, struct ssdisrcinfo *sip)
{
	if (!ci->ssdi_ci_got_config) {
		/* error or not, we got config */
		ci->ssdi_ci_got_config = 1;
		if (get_dynamic_config(dbname, ci->ssdi_ci_dsrcs))
			return 1;
		ci->ssdi_ci_currdsrc = &ci->ssdi_ci_dsrcs[0];
	}
	while (ci->ssdi_ci_currdsrc && *(ci->ssdi_ci_currdsrc) != NULL) {
		if (ssdi_load(sip, dbname, *(ci->ssdi_ci_currdsrc))) {
			dprintf(("<%s> => cannot load source <%s>\n", dbname, *(ci->ssdi_ci_currdsrc)));
			ci->ssdi_ci_currdsrc++;
		} else  {
			/* successfully loaded something */
			ci->ssdi_ci_currdsrc++;	/* point to next src to load */
			return 0;
		}
	}
	return 1;
}
#else /* _LIBC_NONSHARED */

#include <ssdi.h>

/* ARGSUSED */
int
ssdi_get_config_and_load(char *dbname, struct ssdiconfiginfo *ci, struct ssdisrcinfo *sip)
{ 
	/* always return failure --- no dso's supported */
	return 1;
}

#endif /* _LIBC_NONSHARED */
