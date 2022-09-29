/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.4 $"

/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak sgi_getcapabilitybyname = _sgi_getcapabilitybyname
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <capability.h>
#include <ns_api.h>

#define FIELD_SIZE	((size_t) 255)
static struct user_cap user_cap;
/* move out of function scope so we get a global symbol for use with data cording */
static int allocated = 0;

static int
allocate_user_cap(void)
{

	if (!allocated)
	{
		user_cap.ca_name = (char *) malloc(FIELD_SIZE + 1);
		if (user_cap.ca_name == NULL)
			return(-1);

		user_cap.ca_default = (char *) malloc(FIELD_SIZE + 1);
		if (user_cap.ca_default == NULL)
		{
			free(user_cap.ca_name);
			return(-1);
		}

		user_cap.ca_allowed = (char *) malloc(FIELD_SIZE + 1);
		if (user_cap.ca_allowed == NULL)
		{
			free(user_cap.ca_name);
			free(user_cap.ca_default);
			return(-1);
		}

		allocated = 1;
	}
	return(0);
}

static struct user_cap *
copy_on_match(char *buffer, size_t keylen)
{
	char *name;
	char *dflt;
	char *allowed;

	/* name starts at the beginning of the line */
	name = buffer;

	/* colon must immediately follow key */
	if (*(name + keylen) != ':')
		return(NULL);

	/* assign dflt and zero out colon */
	dflt = name + keylen;
	*dflt++ = '\0';

	/* assign allowed and zero out colon */
	allowed = strchr(dflt, ':');
	if (allowed == NULL || (allowed - dflt) > FIELD_SIZE)
		return(NULL);
	*allowed++ = '\0';
	if (strlen(allowed) > FIELD_SIZE)
		return(NULL);

	/* copy into user buffer */
	strcpy(user_cap.ca_name, name);
	strcpy(user_cap.ca_default, dflt);
	strcpy(user_cap.ca_allowed, allowed);

	/* return user buffer */
	return(&user_cap);
}

/* move out of function scope so we get a global symbol for use with data cording */
static ns_map_t _map_byname;

struct user_cap *
sgi_getcapabilitybyname(const char *key)
{
	char buffer[1024];
	char *p;
	FILE *fp;
	size_t keylen;
	struct user_cap *cap;
	struct stat st;

	/* reject bogus arg and allocate user buffer */
	if (key == NULL || allocate_user_cap() == -1)
		return(NULL);

	/* get key length */
	keylen = strlen(key);
	if (keylen == 0 || keylen > FIELD_SIZE)
		return(NULL);

	/* update cache time */
	if (stat(USER_CAPABILITY, &st) == 0)
		_map_byname.m_version = st.st_mtim.tv_sec;

	/* look up key in database */
	switch(ns_lookup(&_map_byname, 0, "capability.byname", key, 0,
			 buffer, sizeof(buffer)))
	{
		case NS_SUCCESS:
			/* strip trailing NUL */
			if ((p = strchr(buffer, '\n')) != NULL)
				*p = '\0';

			/* return match */
			return(copy_on_match(buffer, keylen));
		case NS_NOTFOUND:
			return(NULL);
	}

	/* fall back to local files */
	fp = fopen(USER_CAPABILITY, "r");
	if (fp == NULL)
		return(NULL);
	while(fgets(buffer, (int) sizeof(buffer), fp) != NULL)
	{
		/* strip trailing NUL */
		if ((p = strchr(buffer, '\n')) != NULL)
			*p = '\0';

		/* skip leading whitespace */
		for (p = buffer; *p != '\0' && isspace(*p); p++)
			/* empty loop */;

		/* ignore empty or comment lines */
		if (*p == '\0' || *p == '#')
			continue;

		/* match key in line */
		if (strncmp(key, p, keylen) == 0)
		{
			if ((cap = copy_on_match(p, keylen)) == NULL)
				continue;
			fclose(fp);
			return(cap);
		}
	}
	fclose(fp);
	return(NULL);
}
