#include "synonyms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ns_api.h>
#include <sys/mac_label.h>
#include <sys/stat.h>
#include "mac_util.h"

#ident "$Revision: 1.3 $"

/*
 * Check a category or division set to ensure that all values are in
 * ascending order and each division or category appears only once.
 */
int
__check_setvalue(const unsigned short *list, unsigned short count)
{
	unsigned short i;

        for (i = 1; i < count ; i++)
                if (list[i] <= list[i-1])
                        return -1;
        return 0;
}

int
__segment_equal(const label_segment_p ls1, const label_segment_p ls2)
{
	if (ls1->level != ls2->level)
		return(0);
	if (ls1->count != ls2->count)
		return(0);
	if (ls1->count == 0)
		return(1);
	return(!memcmp(ls1->list, ls2->list,
		       ls1->count * sizeof(ls1->list[0])));
}

/*
 * Given two arrays of unsigned short integers, l1 and l2, of
 * respective dimensions c1 and c2, each sorted in ascending
 * sequence, return 1 iff l1 is a (proper or improper) subset of l2.
 * That is, return 1 is every element of l1 is in l2. Return 0 otherwise.
 *
 * This routine requires, but does not check nor ASSERT, that both
 * arrays be sorted. It does not return correct results if lists are
 * not sorted.
 */
static int
check_subset(const unsigned short *l1, int c1,
	     const unsigned short *l2, int c2)
{
	const unsigned short *l1e;
	const unsigned short *l2e;
	unsigned short i1;
	unsigned short i2;

	if (c1 == 0)
		return 1;
	l1e = l1 + c1;
	l2e = l2 + c2;
	while (l2 < l2e) {
		if ((i2 = *l2++) == (i1 = *l1)) {
			if (++l1 < l1e)
				continue;
			return 1;
		}
		if (i2 > i1)		/* i1 is missing from l2 */
			return 0;
	}
	return 0;
}

int
__tcsec_dominate(const label_segment_p ls1, const label_segment_p ls2)
{
	if (ls1->level < ls2->level)
		return(0);
	if (ls1->count < ls2->count)
		return(0);
	return(check_subset(ls2->list, ls2->count, ls1->list, ls1->count));
}

int
__biba_dominate(const label_segment_p ls1, const label_segment_p ls2)
{
	if (ls2->level < ls1->level)
		return(0);
	if (ls2->count < ls1->count)
		return(0);
	return(check_subset(ls1->list, ls1->count, ls2->list, ls2->count));
}

/*
 * Convert MSEN_TYPE or MINT_TYPE to the appropriate
 * field type, according to the current field type
 * being looked up.
 */
typedef enum {MSEN_TYPE, MINT_TYPE} label_type;
typedef enum {LBL_TYPE, LVLGRD, CATDIV} field_type;
typedef enum {MAC_MSEN, MAC_MINT, MAC_LEVEL, MAC_GRADE, MAC_CATEGORY,
	      MAC_DIVISION} field_value;
typedef struct
{
	field_value value;
	char *name;
} value_name;

static const value_name field[2][3] =
{
    {{MAC_MSEN, "msentype"}, {MAC_LEVEL, "level"}, {MAC_CATEGORY, "category"}},
    {{MAC_MINT, "minttype"}, {MAC_GRADE, "grade"}, {MAC_DIVISION, "division"}},
};

static const unsigned short bad_fieldvalue = ~0;
static const unsigned char type_offset = MSEN_MIN_LABEL_NAME;
static const char _PATH_LOCAL[] = "/etc/mac";
static ns_map_t _map_byname = {0};

/*
 * array indicates whether the msen/mint label type allows level/grade,
 * category/division exist, and also indicates whether it is a valid
 * msen/mint label type. The array is a direct mapping from character
 * 'A' to 'z', and depends upon the ASCII collating sequence.
 */
typedef enum {WITHOUT_LVLGRD, NEED_LVLGRD, INVALID_TYPE = -1} mac_type;
static const mac_type mac_type_info[] = {
	WITHOUT_LVLGRD,			/* MSEN_ADMIN_LABEL   'A' */
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	WITHOUT_LVLGRD,			/* MSEN_EQUAL_LABEL   'E' */
	INVALID_TYPE,
	INVALID_TYPE,				
	WITHOUT_LVLGRD,			/* MSEN_HIGH_LABEL    'H' */	
	WITHOUT_LVLGRD,			/* MLD_HIGH_LABEL     'I' */
	INVALID_TYPE,
	INVALID_TYPE,
	WITHOUT_LVLGRD,			/* MSEN_LOW_LABEL     'L' */
	NEED_LVLGRD,			/* MSEN_MLD_LABEL     'M' */
	WITHOUT_LVLGRD,			/* MSEN_MLD_LOW_LABEL 'N' */
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	NEED_LVLGRD,			/* MSEN_TCSEC_LABEL   'T' */
	WITHOUT_LVLGRD,			/* MSEN_UNKNOWN_LABEL 'U' */
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	NEED_LVLGRD,			/* MINT_BIBA_LABEL    'b' */
	INVALID_TYPE,
	INVALID_TYPE,
	WITHOUT_LVLGRD,			/* MINT_EQUAL_LABEL   'e' */
	INVALID_TYPE,
	INVALID_TYPE,
	WITHOUT_LVLGRD,			/* MINT_HIGH_LABEL    'h' */
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	WITHOUT_LVLGRD,			/* MINT_LOW_LABEL     'l' */
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE,
	INVALID_TYPE
};

static char *
parse_file_byname(const char *key, size_t keylen, char *buffer, size_t buflen)
{
	FILE *fp;
	char *p;

	fp = fopen(_PATH_LOCAL, "r");
	if (fp == NULL)
		return(NULL);
	while(fgets(buffer, (int) buflen, fp) != NULL)
	{
		/* nuke trailing newline */
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
			if (*(p + keylen) == ':')
			{
				fclose(fp);
				return(p + keylen + 1);
			}
		}
	}
	fclose(fp);
	return(NULL);
}

static char *
parse_file_byvalue(const char *key, size_t keylen, char *buffer, size_t buflen)
{
	FILE *fp;
	char *segment;
	char *p;

	fp = fopen(_PATH_LOCAL, "r");
	if (fp == NULL)
		return(NULL);
	while(fgets(buffer, (int) buflen, fp) != NULL)
	{
		/* nuke trailing newline */
		if ((p = strchr(buffer, '\n')) != NULL)
			*p = '\0';

		/* skip leading whitespace */
		for (p = buffer; *p != '\0' && isspace(*p); p++)
			/* empty loop */;

		/* ignore empty or comment lines */
		if (*p == '\0' || *p == '#')
			continue;

		/* match key in line */
		if ((segment = strstr(p, key)) != NULL)
		{
			fclose(fp);
			if (strlen(segment) != keylen || *(segment - 1) != ':')
				return(NULL);
			*(segment - 1) = '\0';
			return(p);
		}
	}
	fclose(fp);
	return(NULL);
}

static unsigned short
mac_get_value(const value_name *field, const char *str)
{
	char *key;
	char *value;
	size_t keylen;
	char buffer[1024];
	struct stat st;

	/* allocate and create key */
	key = (char *) malloc(strlen(field->name) + strlen(str) + 2);
	if (key == NULL)
		return(bad_fieldvalue);
	sprintf(key, "%s:%s", str, field->name);
	keylen = strlen(key);

	/* update cache time */
	if (stat(_PATH_LOCAL, &st) == 0)
		_map_byname.m_version = st.st_mtim.tv_sec;

	/* look up key in database */
	switch(ns_lookup(&_map_byname, 0, "mac.byname", key, 0,
			 buffer, sizeof(buffer)))
	{
		case NS_SUCCESS:
			free(key);
			if ((key = strchr(buffer, '\n')) != NULL)
				*key = '\0';
			if (*(buffer + keylen) != ':')
				return(bad_fieldvalue);
			else
				return((unsigned short) strtoul(buffer + keylen + 1, (char **) 0, 16));
		case NS_NOTFOUND:
			free(key);
			return(bad_fieldvalue);
	}

	/* fall back to local files */
	value = parse_file_byname(key, keylen, buffer, sizeof(buffer));
	free(key);
	return(value ? (unsigned short) strtoul(value, (char **) 0, 16) : bad_fieldvalue);
}

/* move out of function scope so we get a global symbol for use with data cording */
static ns_map_t _map_byvalue = {0};

static char *
lookup_byvalue(const char *key, size_t keylen, char *buffer, size_t buflen)
{
	char *name;
	struct stat st;

	/* update cache time */
	if (stat(_PATH_LOCAL, &st) == 0)
		_map_byvalue.m_version = st.st_mtim.tv_sec;

	/* look up key in database */
	switch(ns_lookup(&_map_byvalue, 0, "mac.byvalue", key, 0,
			 buffer, buflen))
	{
		case NS_SUCCESS:
			name = strchr(buffer, ':');
			if (name == NULL)
				return(NULL);
			*name = '\0';
			return(strdup(buffer));
		case NS_NOTFOUND:
			return(NULL);
	}

	/* fall back to local files */
	name = parse_file_byvalue(key, keylen, buffer, buflen);
	return(name != NULL ? strdup(name) : NULL);
}

static char *
mac_get_name(const value_name *field, unsigned short value)
{
	char *key;
	char *name;
	char buffer[1024];

	/* allocate and create key */
	key = (char *) malloc(strlen(field->name) + 10 + 2);
	if (key == NULL)
		return(NULL);
	sprintf(key, "%s:%#hx", field->name, value);
	name = lookup_byvalue(key, strlen(key), buffer, sizeof(buffer));
	free(key);
	return(name);
}

/*
 * sort an array of short integers into ascending order.
 * Based on Algorithm 201, ACM by J. Boothroyd (1963).
 * Also known as a "shell" sort.
 * Chosen for compactness and speed.
 */
static void
sort_shorts(unsigned short *shorts, unsigned short count)
{
	unsigned short *p1;
	unsigned short *p2;
	unsigned short *start;
	unsigned short *limit;
	unsigned short middle;

        middle = count;
        for (;;) {
                if ((middle >>= 1) == 0)
                        return;
                limit = &shorts[count - middle];
                for (start = shorts; start < limit; ++start) {
			unsigned short tmp1, tmp2;

                        p1 = start;
                        p2 = p1 + middle;
                        while (p1 >= shorts && (tmp1 = *p1) > (tmp2 = *p2)) {
                                *p1 = tmp2; *p2 = tmp1;
                                p2 = p1;
                                p1 -= middle;
                        }
                }
        }
        /* NOTREACHED */
}

static int
get_catdiv_value(label_type type, label_segment_p ls, char *str)
{
	char *comma;
	char *catdiv;
	char *tmpstr = str;
	unsigned short value;
	unsigned short count = 0;
	fd_set set;

	FD_ZERO(&set);
	while (tmpstr != NULL)
	{
		if ((comma = strchr(catdiv = tmpstr, ',')) != NULL)
		{
			*comma = '\0';
			tmpstr = comma + 1;
		}
		else
			tmpstr = NULL;

		value = mac_get_value(&field[type][CATDIV], catdiv);
		if (value == bad_fieldvalue || count >= MAC_MAX_SETS)
			return(-1);

		if (!FD_ISSET(value, &set))
			ls->list[count++] = value;
		FD_SET(value, &set);
	}

	/* sort category/division list */
	sort_shorts(ls->list, ls->count = count);

	/* Success! */
	return(0);
}

static int
get_catdiv_name(label_type type, label_segment_p ls, char *str)
{
	char *current = str;
	char *value;
	unsigned short count = 0;

	for (count = 0; count < ls->count; count++)
	{
		value = mac_get_name(&field[type][CATDIV], ls->list[count]);
		if (value == NULL)
			return(-1);
		*current = ',';
		strcpy(current + 1, value);
		current += strlen(current);
		free(value);
	}

	/* Success! */
	return(0);
}

static int
get_lvlgrd_value(label_type type, label_segment_p ls, char *str)
{
	char *comma;
	char *catdiv = NULL;
	unsigned short value;

	if ((comma = strchr(str, ',')) != NULL)
	{
		catdiv = comma + 1;
		*comma = '\0';
	}

	value = mac_get_value(&field[type][LVLGRD], str);
	if (value == bad_fieldvalue)
		return(-1);
	else
		ls->level = value;

	if (catdiv != NULL && get_catdiv_value(type, ls, catdiv) == -1)
		return(-1);

	/* Success! */
	return(0);
}

static int
get_lvlgrd_name(label_type type, label_segment_p ls, char *str)
{
	char *current = str;
	char *value;

	value = mac_get_name(&field[type][LVLGRD], ls->level);
	if (value == NULL)
		return(-1);
	*current = ',';
	strcpy(current + 1, value);
	current += strlen(current);
	free(value);

	if (ls->count > 0 && get_catdiv_name(type, ls, current) == -1)
		return(-1);

	/* Success! */
	return(0);
}

static int
get_type_value(label_type type, label_segment_p ls, char *str)
{
	char *comma;
	char *lvlgrd = NULL;
	unsigned short value;

	/* separate into label type & level/grade+cats/divs */
	if ((comma = strchr(str, ',')) != NULL)
	{
		lvlgrd = comma + 1;
		*comma = '\0';
	}

	value = mac_get_value(&field[type][LBL_TYPE], str);
	if (value == bad_fieldvalue)
	{
		/*
		 * Assume `str' must be a level name, making the
		 * level type implied. If this assumption is not
		 * correct, get_lvlgrd_value() will abuse us.
		 */
		switch(type)
		{
			case MSEN_TYPE:
				value = MSEN_TCSEC_LABEL;
				break;
			case MINT_TYPE:
				value = MINT_BIBA_LABEL;
				break;
		}
		lvlgrd = str;
	}
	ls->type = value;

	if (lvlgrd != NULL)
	{
		if (mac_type_info[value - type_offset] == WITHOUT_LVLGRD)
			return(-1);
		if (get_lvlgrd_value(type, ls, lvlgrd) == -1)
			return(-1);
	}
	else
		if (mac_type_info[value - type_offset] == NEED_LVLGRD)
			return(-1);

	/* Success! */
	return(0);
}

static int
get_type_name(label_type type, label_segment_p ls, char *str)
{
	char *current = str;
	char *value;

	value = mac_get_name(&field[type][LBL_TYPE], ls->type);
	if (value == NULL)
		return(-1);
	strcpy(current, value);
	current += strlen(current);
	free(value);

	if ((ls->level == 0 && mac_type_info[ls->type - type_offset] == NEED_LVLGRD) || ls->level != 0)
		if (get_lvlgrd_name(type, ls, current) == -1)
			return(-1);

	/* Success! */
	return(0);
}

char *
__mac_spec_from_alias(const char *alias)
{
	char *key;
	char *value;
	const char *keyseg = "alias";
	size_t keylen;
	char buffer[1024];
	struct stat st;

	/* allocate and create key */
	key = (char *) malloc(strlen(alias) + strlen(keyseg) + 2);
	if (key == NULL)
		return(NULL);
	sprintf(key, "%s:%s", alias, keyseg);
	keylen = strlen(key);

	/* update cache time */
	if (stat(_PATH_LOCAL, &st) == 0)
		_map_byname.m_version = st.st_mtim.tv_sec;

	/* look up key in database */
	switch(ns_lookup(&_map_byname, 0, "mac.byname", key, 0,
			 buffer, sizeof(buffer)))
	{
		case NS_SUCCESS:
			free(key);
			if ((key = strchr(buffer, '\n')) != NULL)
				*key = '\0';
			if (*(buffer + keylen) != ':')
				return(NULL);
			else
				return(strdup(buffer + keylen + 1));
		case NS_NOTFOUND:
			free(key);
			return(NULL);
	}

	/* fall back to local files */
	value = parse_file_byname(key, keylen, buffer, sizeof(buffer));
	free(key);
	return(value != NULL ? strdup(value) : NULL);
}

char *
__mac_alias_from_spec(const char *spec)
{
	char *key;
	char *name;
	const char *keyseg = "alias";
	char buffer[1024];

	/* allocate and create key */
	key = (char *) malloc(strlen(spec) + strlen(keyseg) + 2);
	if (key == NULL)
		return(NULL);
	sprintf(key, "%s:%s", keyseg, spec);
	name = lookup_byvalue(key, strlen(key), buffer, sizeof(buffer));
	free(key);
	return(name);
}

int
__msen_from_text(label_segment_p ls, char *str)
{
	return(get_type_value(MSEN_TYPE, ls, str));
}

int
__mint_from_text(label_segment_p ls, char *str)
{
	return(get_type_value(MINT_TYPE, ls, str));
}

int
__msen_to_text(label_segment_p ls, char *str)
{
	return(get_type_name(MSEN_TYPE, ls, str));
}

int
__mint_to_text(label_segment_p ls, char *str)
{
	return(get_type_name(MINT_TYPE, ls, str));
}
