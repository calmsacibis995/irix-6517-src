#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/t6satmp.h>
#include <sys/endian.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <limits.h>
#include "debug.h"
#include "list.h"
#include "lrep.h"
#include "match.h"
#include "name.h"
#include "parse.h"
#include "server.h"

static Generic_list mappings;
static char NATIVE_MAP[] = "NATIVE_MAPPING";

static uid_t *
aid_from_text (const char *nr)
{
	uid_t *uid;

	uid = malloc (sizeof (*uid));
	if (uid == NULL)
		return (uid);
	if (name_to_uid (nr, uid) == -1)
	{
		free (uid);
		return (NULL);
	}
	return (uid);
}

static int
gid_cmp (const void *a, const void *b)
{
	const gid_t *ag = (const uid_t *) a;
	const gid_t *bg = (const uid_t *) b;

	if (*ag < *bg)
		return (-1);
	else if (*ag > *bg)
		return (1);
	return (0);
}

/*
 * The nr is "user-name group-name [optional group names]"
 */
static struct ids *
ids_from_text (const char *nr)
{
	char **ids, **tids;
	struct ids *idlist;
	unsigned int count;

	/*
	 * separate id list into separate user/group names
	 */
	tids = ids = parse_line (nr, (pl_quote_func) NULL, isspace, &count);
	if (ids == NULL || count < 2)
	{
		parse_free (ids);
		return (NULL);
	}

	/* allocate space for local representation */
	idlist = (struct ids *) malloc (sizeof (*idlist));
	if (idlist == NULL)
	{
		parse_free (ids);
		return (idlist);
	}
	idlist->groups.ngroups = count - 2;

	/* get user id */
	if (name_to_uid (*tids++, &idlist->user) == -1)
	{
		parse_free (ids);
		free (idlist);
		return (NULL);
	}

	/* get primary group id */
	if (name_to_gid (*tids++, &idlist->primary_group) == -1)
	{
		parse_free (ids);
		free (idlist);
		return (NULL);
	}

	/* fill supplementary group list */
	for (count = 0; count < idlist->groups.ngroups; count++)
	{
		if (name_to_gid (*tids++, &idlist->groups.groups[count]) == -1)
		{
			parse_free (ids);
			free (idlist);
			return (NULL);
		}
	}

	parse_free (ids);
	qsort ((void *) idlist->groups.groups, idlist->groups.ngroups,
	       sizeof (gid_t), gid_cmp);
	return (idlist);
}

void *
lrep_from_text (const char *nr, u_short attrid)
{
	if (nr == NULL)
		return (NULL);

	switch (attrid)
	{
		case SATMP_ATTR_SEN_LABEL:
		case SATMP_ATTR_CLEARANCE:
			return (name_to_msen (nr));
		case SATMP_ATTR_INTEGRITY_LABEL:
			return (name_to_mint (nr));
		case SATMP_ATTR_PRIVILEGES:
			return (cap_from_text (nr));
		case SATMP_ATTR_AUDIT_ID:
			return (aid_from_text (nr));
		case SATMP_ATTR_IDS:
			return (ids_from_text (nr));
		case SATMP_ATTR_ACL:
		case SATMP_ATTR_AUDIT_INFO:
		case SATMP_ATTR_INFO_LABEL:
		case SATMP_ATTR_NATIONAL_CAVEATS:
			break;
	}
	return (NULL);
}

static void
aid_free (void *lrep)
{
	free (lrep);
}

static void
ids_free (void *lrep)
{
	free (lrep);
}

void
destroy_lrep (void *lrep, u_short attrid)
{
	if (lrep == NULL)
		return;

	switch (attrid)
	{
		case SATMP_ATTR_SEN_LABEL:
		case SATMP_ATTR_CLEARANCE:
			msen_free (lrep);
			break;
		case SATMP_ATTR_INTEGRITY_LABEL:
			mint_free (lrep);
			break;
		case SATMP_ATTR_PRIVILEGES:
			cap_free (lrep);
			break;
		case SATMP_ATTR_AUDIT_ID:
			aid_free (lrep);
			break;
		case SATMP_ATTR_IDS:
			ids_free (lrep);
			break;
		case SATMP_ATTR_ACL:
		case SATMP_ATTR_AUDIT_INFO:
		case SATMP_ATTR_INFO_LABEL:
		case SATMP_ATTR_NATIONAL_CAVEATS:
			break;
	}
}

msen_t
msen_dup (msen_t msen)
{
	msen_t dup;

	dup = (msen_t) malloc (sizeof (*dup));
	if (dup == NULL)
		return (dup);
	*dup = *msen;
	return (dup);
}

mint_t
mint_dup (mint_t mint)
{
	mint_t dup;

	dup = (mint_t) malloc (sizeof (*dup));
	if (dup == NULL)
		return (dup);
	*dup = *mint;
	return (dup);
}

static uid_t *
aid_dup (uid_t *aid)
{
	uid_t *dup;

	dup = (uid_t *) malloc (sizeof (*dup));
	if (dup == NULL)
		return (dup);
	*dup = *aid;
	return (dup);
}

static struct ids *
ids_dup (struct ids *idlist)
{
	struct ids *dup;

	dup = (struct ids *) malloc (sizeof (*dup));
	if (dup == NULL)
		return (dup);
	*dup = *idlist;
	return (dup);
}

void *
lrep_copy (void *lrep, u_short attrid, size_t *sz)
{
	if (lrep == NULL)
		return (NULL);

	switch (attrid)
	{
		case SATMP_ATTR_SEN_LABEL:
		case SATMP_ATTR_CLEARANCE:
			*sz = sizeof (mac_b_label);
			return (msen_dup ((msen_t) lrep));
		case SATMP_ATTR_INTEGRITY_LABEL:
			*sz = sizeof (mac_b_label);
			return (mint_dup ((mint_t) lrep));
		case SATMP_ATTR_PRIVILEGES:
			*sz = sizeof (struct cap_set);
			return (cap_dup ((cap_t) lrep));
		case SATMP_ATTR_AUDIT_ID:
			*sz = sizeof (uid_t);
			return (aid_dup ((uid_t *) lrep));
		case SATMP_ATTR_IDS:
			*sz = sizeof (struct ids);
			return (ids_dup ((struct ids *) lrep));
		case SATMP_ATTR_ACL:
		case SATMP_ATTR_AUDIT_INFO:
		case SATMP_ATTR_INFO_LABEL:
		case SATMP_ATTR_NATIONAL_CAVEATS:
			break;
	}
	return (NULL);
}

static char *
aid_to_text (const uid_t *aid, size_t *lenp)
{
	char *s;
	size_t len;
	char *n, numbuf[32], namebuf[LOGNAME_MAX * 4];

	if (uid_to_name (*aid, namebuf) == -1)
		sprintf (n = numbuf, "%u", *aid);
	else
		n = namebuf;
	len = strlen (n);
	s = (char *) malloc (len + 1);
	if (s != NULL)
	{
		strcpy (s, n);
		if (lenp != NULL)
			*lenp = len;
	}
	return (s);
}

static char *
ids_to_text (const struct ids *idlist, size_t *lenp)
{
	char *s, *t;
	char *n, numbuf[32], namebuf[LOGNAME_MAX * 4];
	u_short i;

	/* allocate 64 characters per id plus NUL */
	t = s = (char *) malloc (((idlist->groups.ngroups + 2) * 64) + 1);
	if (s == NULL)
		return (s);

	/* print uid */
	if (uid_to_name (idlist->user, namebuf) == -1)
		sprintf (n = numbuf, "%u", idlist->user);
	else
		n = namebuf;
	sprintf (t, "%s ", n);
	t += strlen (t);

	/* print primary group id */
	if (gid_to_name (idlist->primary_group, namebuf) == -1)
		sprintf (n = numbuf, "%u", idlist->primary_group);
	else
		n = namebuf;
	sprintf (t, "%s ", n);
	t += strlen (t);

	/* print supplementary group ids */
	for (i = 0; i < idlist->groups.ngroups; i++)
	{
		if (gid_to_name (idlist->groups.groups[i], namebuf) == -1)
			sprintf (n = numbuf, "%u", idlist->groups.groups[i]);
		else
			n = namebuf;
		sprintf (t, "%s ", n);
		t += strlen (t);
	}

	/* remove trailing space */
	*(t - 1) = '\0';

	/* shrink buffer */
	s = (char *) realloc (s, t - s);

	/* record length of string */
	if (s && lenp)
		*lenp = --t - s;

	return (s);
}

char *
lrep_to_text (void *lrep, u_short attrid)
{
	size_t *sz = NULL;

	if (lrep == NULL)
		return (NULL);

	switch (attrid)
	{
		case SATMP_ATTR_SEN_LABEL:
		case SATMP_ATTR_CLEARANCE:
			return (msen_to_name ((msen_t) lrep, sz));
		case SATMP_ATTR_INTEGRITY_LABEL:
			return (mint_to_name ((mint_t) lrep, sz));
		case SATMP_ATTR_PRIVILEGES:
			return (cap_to_text ((cap_t) lrep, sz));
		case SATMP_ATTR_AUDIT_ID:
			return (aid_to_text ((uid_t *) lrep, sz));
		case SATMP_ATTR_IDS:
			return (ids_to_text ((struct ids *) lrep, sz));
		case SATMP_ATTR_ACL:
		case SATMP_ATTR_AUDIT_INFO:
		case SATMP_ATTR_INFO_LABEL:
		case SATMP_ATTR_NATIONAL_CAVEATS:
			break;
	}
	return (NULL);
}

static char *
ids_to_nrep_native (const void *lrep, size_t *lenp)
{
	const struct ids *idlist = (const struct ids *) lrep;
	size_t len = 0;
	u_short t = idlist->groups.ngroups, count = htons (t);
	char *s = NULL;
	char *n, numbuf[32], namebuf[LOGNAME_MAX * 4];

	/* output user id */
	if (uid_to_name (idlist->user, namebuf) == -1)
		sprintf (n = numbuf, "%u", idlist->user);
	else
		n = namebuf;
	s = realloc (s, len + strlen (n) + 1);
	if (s == NULL)
		return (s);
	strcpy (s + len, n);
	len += strlen (n) + 1;

	/* output primary group id */
	if (gid_to_name (idlist->primary_group, namebuf) == -1)
		sprintf (n = numbuf, "%u", idlist->primary_group);
	else
		n = namebuf;
	s = realloc (s, len + strlen (n) + 1);
	if (s == NULL)
		return (s);
	strcpy (s + len, n);
	len += strlen (n) + 1;

	/* output group count */
	s = realloc (s, len + sizeof (count));
	if (s == NULL)
		return (s);
	memcpy (s + len, &count, sizeof (count));
	len += sizeof (count);

	/* output group list */
	for (count = 0; count < idlist->groups.ngroups; count++)
	{
		if (gid_to_name (idlist->groups.groups[count], namebuf) == -1)
			sprintf (n = numbuf, "%u",
				 idlist->groups.groups[count]);
		else
			n = namebuf;
		s = realloc (s, len + strlen (n) + 1);
		if (s == NULL)
			return (s);
		strcpy (s + len, n);
		len += strlen (n) + 1;
	}
	
	if (lenp != NULL)
		*lenp = len - 1;
	return (s);
}

static char *
lrep_to_nrep_native (void *lrep, u_short attrid, size_t *size)
{
	switch (attrid)
	{
		case SATMP_ATTR_SEN_LABEL:
		case SATMP_ATTR_CLEARANCE:
			return (msen_to_name ((msen_t) lrep, size));
		case SATMP_ATTR_INTEGRITY_LABEL:
			return (mint_to_name ((mint_t) lrep, size));
		case SATMP_ATTR_PRIVILEGES:
			return (cap_to_text ((cap_t) lrep, size));
		case SATMP_ATTR_AUDIT_ID:
			return (aid_to_text (lrep, size));
		case SATMP_ATTR_IDS:
			return (ids_to_nrep_native (lrep, size));
		case SATMP_ATTR_ACL:
		case SATMP_ATTR_AUDIT_INFO:
		case SATMP_ATTR_INFO_LABEL:
		case SATMP_ATTR_NATIONAL_CAVEATS:
			break;
	}
	return (NULL);
}

static char *
aid_to_nrep_mapped (void *lrep, size_t *lenp, Generic_list *map)
{
	uid_t *aid = (uid_t *) lrep;
	struct map *m;
	size_t len;
	char *s, namebuf[LOGNAME_MAX * 4];

	if (uid_to_name (*aid, namebuf) == -1)
		return (NULL);

	m = first_that (map, match_map_bysource, namebuf);
	if (m == NULL)
		return (NULL);

	len = strlen (m->dest);
	s = malloc (len + 1);
	if (s != NULL)
	{
		strcpy (s, m->dest);
		decref (m);
	}
	else
	{
		decref (m);
		return (s);
	}

	if (lenp)
		*lenp = len;

	return (s);
}

static int
iscomma (int c)
{
	return (c == ',');
}

static char *
ids_to_nrep_mapped (void *lrep, size_t *lenp, Generic_list *map)
{
	struct ids *idlist = (struct ids *) lrep;
	struct map *m;
	char *s = NULL;
	char *n, numbuf[32], namebuf[LOGNAME_MAX * 4];
	size_t len = 0;
	u_short t = idlist->groups.ngroups, count = htons (t);

	/* output user id */
	if (uid_to_name (idlist->user, namebuf) == -1)
		sprintf (n = numbuf, "%u", idlist->user);
	else
		n = namebuf;
	m = first_that (map, match_map_asuser, n);
	if (m == NULL)
		return (NULL);
	s = realloc (s, len + strlen (m->dest) + 1);
	if (s != NULL)
	{
		strcpy (s + len, m->dest);
		len += strlen (m->dest) + 1;
		decref (m);
	}
	else
	{
		decref (m);
		return (s);
	}

	/* output primary group id */
	if (gid_to_name (idlist->primary_group, namebuf) == -1)
		sprintf (n = numbuf, "%u", idlist->primary_group);
	else
		n = namebuf;
	m = first_that (map, match_map_asgroup, n);
	if (m == NULL)
	{
		free (s);
		return (NULL);
	}
	s = realloc (s, len + strlen (m->dest) + 1);
	if (s != NULL)
	{
		strcpy (s + len, m->dest);
		len += strlen (m->dest) + 1;
		decref (m);
	}
	else
	{
		decref (m);
		return (s);
	}

	/* output group count */
	s = realloc (s, len + sizeof (count));
	if (s == NULL)
		return (s);
	memcpy (s + len, &count, sizeof (count));
	len += sizeof (count);

	/* output group list */
	for (count = 0; count < idlist->groups.ngroups; count++)
	{
		if (gid_to_name (idlist->groups.groups[count], namebuf) == -1)
			sprintf (n = numbuf, "%u",
				 idlist->groups.groups[count]);
		else
			n = namebuf;
		m = first_that (map, match_map_asgroup, n);
		if (m == NULL)
		{
			free (s);
			return (NULL);
		}
		s = realloc (s, len + strlen (m->dest) + 1);
		if (s != NULL)
		{
			strcpy (s + len, m->dest);
			len += strlen (m->dest) + 1;
			decref (m);
		}
		else
		{
			decref (m);
			return (s);
		}
	}
	
	if (lenp)
		*lenp = len - 1;

	return (s);
}

static char *
label_to_nrep_mapped (mac_b_label *label, size_t *lenp, Generic_list *map,
		      unsigned char tcsec_type,
		      char *(*to_text) (mac_b_label *, size_t *),
		      int (*free_text) (void *))
{
	struct map *m;
	char *s = NULL, *text, **words;
	size_t len = 0, slen;
	unsigned int count, i;

	/* parse text form of label */
	text = (*to_text) (label, (size_t *) NULL);
	if (text == NULL)
		return (NULL);
	words = parse_line (text, (pl_quote_func) NULL, iscomma, &count);
	(*free_text) ((mac_b_label *) text);
	if (words == NULL)
		return (NULL);

	/* mapping depends upon label type */
	if (label->b_type != tcsec_type)
	{
		/* get mapping for label type */
		m = first_that (map, match_map_aslbltype, words[0]);
		if (m == NULL)
		{
			parse_free (words);
			return (NULL);
		}
		len = strlen (m->dest);
		s = realloc (s, len + 1);
		if (s == NULL)
		{
			decref (m);
			parse_free (words);
			return (s);
		}
		strcpy (s, m->dest);
		decref (m);
	}
	else
	{
		/* catch bad labels: this should never happen */
		if (count < 2)
		{
			parse_free (words);
			return (NULL);
		}

		/* get mapping for label level */
		m = first_that (map, match_map_aslevel, words[1]);
		if (m == NULL)
		{
			parse_free (words);
			return (NULL);
		}
		slen = strlen (m->dest);
		s = realloc (s, len + slen + 2);
		if (s == NULL)
		{
			decref (m);
			parse_free (words);
			return (s);
		}
		strcpy (s + len, m->dest);
		decref (m);
		len += slen;
		strcpy (s + len++, " ");
 
		/* get mapping for label categories */
		for (i = 2; i < count; i++)
		{
			m = first_that (map, match_map_ascategory, words[i]);
			if (m == NULL)
			{
				parse_free (words);
				return (NULL);
			}
			slen = strlen (m->dest);
			s = realloc (s, len + slen + 2);
			if (s == NULL)
			{
				decref (m);
				parse_free (words);
				return (s);
			}
			strcpy (s + len, m->dest);
			decref (m);
			len += slen;
			strcpy (s + len++, " ");
		}

		/* remove trailing space */
		*(s + len - 1) = '\0';

		/* shrink buffer */
		s = realloc (s, len--);
		if (s == NULL)
		{
			parse_free (words);
			return (s);
		}
	}

	parse_free (words);

	/* record length of string */
	if (lenp != NULL)
		*lenp = len;

	return (s);
}

static char *
msen_to_nrep_mapped (void *lrep, size_t *size, Generic_list *map)
{
	return (label_to_nrep_mapped (lrep, size, map, MSEN_TCSEC_LABEL,
				      msen_to_name, msen_free));
}

static char *
mint_to_nrep_mapped (void *lrep, size_t *size, Generic_list *map)
{
	return (label_to_nrep_mapped (lrep, size, map, MINT_BIBA_LABEL,
				      mint_to_name, mint_free));
}

static int
cap_in_set (cap_value_t have, cap_value_t needed)
{
	return ((have & needed) == have);
}

typedef struct cap_to_nrep_context
{
	int failed;
	char *sp;
	size_t len;
	cap_t incap;
} cap_to_nrep_context;

static void
ctn_list_traverse (void *a, void *b)
{
	struct map *m = (struct map *) a;
	cap_to_nrep_context *cp = (cap_to_nrep_context *) b;
	cap_t outcap;

	if (cp->failed)
		return;
	outcap = cap_from_text (m->source);
	if (outcap == NULL)
		return;
	if (cap_in_set (cp->incap->cap_effective, outcap->cap_effective))
	{
		cp->sp = realloc (cp->sp, cp->len + strlen (m->dest) + 2);
		if (cp->sp == NULL)
		{
			cap_free (outcap);
			cp->failed = 1;
			cp->len = 0;
			return;
		}
		strcpy (cp->sp + cp->len, m->dest);
		cp->len += strlen (m->dest);
		strcpy (cp->sp + cp->len++, " ");
	}
	cap_free (outcap);
}

static char *
cap_to_nrep_mapped (void *lrep, size_t *size, Generic_list *map)
{
	cap_to_nrep_context context;

	context.failed = 0;
	context.sp = NULL;
	context.len = 0;
	context.incap = (cap_t) lrep;
	perform_on_list (map, ctn_list_traverse, (void *) &context);
	if (context.failed)
		return (NULL);

	/* remove trailing space */
	*(context.sp + context.len - 1) = '\0';

	/* shrink buffer */
	context.sp = realloc (context.sp, context.len--);

	/* record length of string */
	if (context.sp != NULL && size != NULL)
		*size = context.len;

	return (context.sp);
}

/* ARGSUSED */
static char *
dummy_to_nrep_mapped (void *lrep, size_t *size, Generic_list *map)
{
	return (NULL);
}

char *
lrep_to_nrep (void *lrep, u_short attrid, const char *domain, size_t *size)
{
	struct map_head *mh;
	struct map_domain *md;
	void *x;
	u_short mh_attrid;
	char *(*mapfunc) (void *, size_t *, Generic_list *);

	if (lrep == NULL)
		return (NULL);

	/* find the attribute and domain */
	mh = first_that (&mappings, match_map_head_byid, &attrid);
	if (mh == NULL)
		return (NULL);
	mh_attrid = mh->attrid;
	md = first_that (&mh->domains, match_map_domain_bydomain, domain);
	decref (mh);
	if (md == NULL)
		return (NULL);

	/* see if native mapping is required */
	if (x = first_that (&md->remote_map, match_map_bysource, NATIVE_MAP))
	{
		decref (x);
		decref (md);
		return (lrep_to_nrep_native (lrep, attrid, size));
	}

	switch (mh_attrid)
	{
		case SATMP_ATTR_SEN_LABEL:
		case SATMP_ATTR_CLEARANCE:
			mapfunc = msen_to_nrep_mapped;
			break;
		case SATMP_ATTR_INTEGRITY_LABEL:
			mapfunc = mint_to_nrep_mapped;
			break;
		case SATMP_ATTR_PRIVILEGES:
			mapfunc = cap_to_nrep_mapped;
			break;
		case SATMP_ATTR_AUDIT_ID:
			mapfunc = aid_to_nrep_mapped;
			break;
		case SATMP_ATTR_IDS:
			mapfunc = ids_to_nrep_mapped;
			break;
		case SATMP_ATTR_ACL:
		case SATMP_ATTR_AUDIT_INFO:
		case SATMP_ATTR_INFO_LABEL:
		case SATMP_ATTR_NATIONAL_CAVEATS:
			mapfunc = dummy_to_nrep_mapped;
			break;
	}
	x = (void *) (*mapfunc) (lrep, size, &md->remote_map);
	decref (md);
	return ((char *) x);
}


static struct ids *
ids_from_nrep_native (const char *nr)
{
	struct ids *idlist;
	u_short count;
	uid_t uid;
	gid_t pgid;

	/* get user id */
	if (name_to_uid (nr, &uid) == -1)
		return ((struct ids *) NULL);
	nr += strlen (nr) + 1;

	/* get primary group id */
	if (name_to_gid (nr, &pgid) == -1)
		return ((struct ids *) NULL);
	nr += strlen (nr) + 1;

	/* get group count */
	memcpy (&count, nr, sizeof (count));
	count = ntohs (count);
	nr += sizeof (count);

	/* allocate space for local representation */
	idlist = (struct ids *) malloc (sizeof (*idlist));
	if (idlist == (struct ids *) NULL)
		return (idlist);
	idlist->groups.ngroups = count;
	idlist->user = uid;
	idlist->primary_group = pgid;

	/* fill supplementary group list */
	for (count = 0; count < idlist->groups.ngroups; count++)
	{
		if (name_to_gid (nr, &idlist->groups.groups[count]) == -1)
		{
			free (idlist);
			return ((struct ids *) NULL);
		}
		nr += strlen (nr) + 1;
	}

	qsort ((void *) idlist->groups.groups, idlist->groups.ngroups,
	       sizeof (gid_t), gid_cmp);
	return (idlist);
}

static void *
lrep_from_nrep_native (char *nrepp, u_short attrid)
{
	switch (attrid)
	{
		case SATMP_ATTR_SEN_LABEL:
		case SATMP_ATTR_CLEARANCE:
			return (name_to_msen (nrepp));
		case SATMP_ATTR_INTEGRITY_LABEL:
			return (name_to_mint (nrepp));
		case SATMP_ATTR_PRIVILEGES:
			return (cap_from_text (nrepp));
		case SATMP_ATTR_AUDIT_ID:
			return (aid_from_text (nrepp));
		case SATMP_ATTR_IDS:
			return (ids_from_nrep_native (nrepp));
		case SATMP_ATTR_ACL:
		case SATMP_ATTR_AUDIT_INFO:
		case SATMP_ATTR_INFO_LABEL:
		case SATMP_ATTR_NATIONAL_CAVEATS:
			break;
	}
	return (NULL);
}

static struct map *
match_subset (char *s, unsigned int *nwords, Generic_list *map, match_fn fn)
{
	struct map *m;
	size_t len = strlen (s);

	/* find the smallest subset of words that matches the level name */
	do
	{
		/* find it */
		m = first_that (map, fn, s);
		if (m != NULL)
			break;

		/* remove last word */
		if (*nwords > 1)
		{
			while (!isspace (*(s + len)))
				len--;
			*(s + len) = '\0';
		}
	} while (--*nwords);

	return (m);
}

static char *
create_subset (char **words, unsigned int max)
{
	char *s = NULL;
	unsigned int i, len = 0;

	for (i = 0; i < max; i++)
	{
		s = realloc (s, len + strlen (words[i]) + 2);
		if (s == NULL)
		{
			parse_free (words);
			return (NULL);
		}
		strcpy (s + len, words[i]);
		len += strlen (words[i]);
		strcpy (s + len++, " ");
	}

	/* remove trailing space */
	*(s + --len) = '\0';

	return (s);
}

static void *
label_from_nrep_mapped (char *nrepp, Generic_list *map,
			mac_b_label *(*from_text) (const char *))
{
	mac_b_label *label;
	struct map *m;
	unsigned int count, countmax;
	char **words, *s = NULL, *t;
	size_t len = 0;

	/* match entire nrep against label type */
	m = first_that (map, match_map_aslbltype, nrepp);
	if (m != NULL)
	{
		void *x = (void *) (*from_text) (m->dest);
		decref (m);
		return (x);
	}

	/* break nrep into words */
	words = parse_line (nrepp, (pl_quote_func) NULL, isspace, &countmax);
	if (words == NULL)
		return (NULL);

	/* create subset */
	t = create_subset (words, countmax);
	if (t == NULL)
	{
		parse_free (words);
		return (NULL);
	}

	/* find the smallest subset of words that matches a level name */
	count = countmax;
	m = match_subset (t, &count, map, match_map_aslevel);
	free (t);
	if (m == NULL)
	{
		parse_free (words);
		return (NULL);
	}

	/* create level name */
	s = realloc (s, strlen (m->dest) + 2);
	if (s == NULL)
	{
		decref (m);
		parse_free (words);
		return (NULL);
	}
	strcpy (s + len, m->dest);
	len += strlen (m->dest);
	decref (m);
	strcpy (s + len++, " ");

	/* match categories */
	for (count++; count < countmax; count++)
	{
		unsigned int nwords;

		nwords = countmax - count;
		t = create_subset (&words[count], nwords);
		if (t == NULL)
		{
			free (s);
			parse_free (words);
			return (NULL);
		}

		/* find the smallest subset of words that matches a category */
		m = match_subset (t, &nwords, map, match_map_ascategory);
		free (t);
		if (m == NULL)
		{
			free (s);
			parse_free (words);
			return (NULL);
		}
		count += nwords;

		/* add category to label */
		s = realloc (s, len + strlen (m->dest) + 2);
		if (s == NULL)
		{
			decref (m);
			parse_free (words);
			return (NULL);
		}
		strcpy (s + len, m->dest);
		len += strlen (m->dest);
		decref (m);
		strcpy (s + len++, " ");
	}

	/* remove trailing space */
	*(s + --len) = '\0';

	/* get final label */
	label = (*from_text) (s);

	free (s);
	parse_free (words);
	return ((void *) label);
}

static void *
msen_from_nrep_mapped (char *nrepp, Generic_list *map)
{
	return (label_from_nrep_mapped (nrepp, map, name_to_msen));
}

static void *
mint_from_nrep_mapped (char *nrepp, Generic_list *map)
{
	return (label_from_nrep_mapped (nrepp, map, name_to_mint));
}

static int
cap_sep (int c)
{
	return (isspace (c) || iscomma (c));
}

static void *
cap_from_nrep_mapped (char *nrepp, Generic_list *map)
{
	cap_t outcap;
	char **words;
	unsigned int i, count;

	/* allocate capability set */
	outcap = (cap_t) malloc (sizeof (*outcap));
	if (outcap == NULL)
		return (NULL);
	if (cap_clear (outcap) == -1)
	{
		free (outcap);
		return (NULL);
	}

	/* separate into individual words */
	words = parse_line (nrepp, (pl_quote_func) NULL, cap_sep, &count);
	if (words == NULL)
	{
		free (outcap);
		return (NULL);
	}

	/* map words into capabilities */
	for (i = 0; i < count; i++)
	{
		cap_t incap;
		struct map *m;

		m = first_that (map, match_map_bysource, words[i]);
		if (m == NULL)
		{
			free (outcap);
			return (NULL);
		}
		incap = cap_from_text (m->dest);
		decref (m);
		if (incap == NULL)
		{
			free (outcap);
			return (NULL);
		}
		outcap->cap_effective |= incap->cap_effective;
		cap_free (incap);
	}

	return ((void *) outcap);
}

static void *
ids_from_nrep_mapped (char *nr, Generic_list *map)
{
	struct ids *idlist;
	struct map *m;
	u_short count;
	uid_t uid;
	gid_t pgid;
	int r;

	/* get user id */
	m = first_that (map, match_map_asuser, nr);
	if (m == NULL)
		return (NULL);
	r = name_to_uid (m->dest, &uid);
	decref (m);
	if (r == -1)
		return (NULL);
	nr += strlen (nr) + 1;

	/* get primary group id */
	m = first_that (map, match_map_asgroup, nr);
	if (m == NULL)
		return (NULL);
	r = name_to_gid (m->dest, &pgid);
	decref (m);
	if (r == -1)
		return (NULL);
	nr += strlen (nr) + 1;

	/* get group count */
	memcpy (&count, nr, sizeof (count));
	count = ntohs (count);
	nr += sizeof (count);

	/* allocate space for local representation */
	idlist = (struct ids *) malloc (sizeof (*idlist));
	if (idlist == NULL)
		return ((void *) idlist);
	idlist->groups.ngroups = count;
	idlist->user = uid;
	idlist->primary_group = pgid;

	/* fill supplementary group list */
	for (count = 0; count < idlist->groups.ngroups; count++)
	{
		m = first_that (map, match_map_asgroup, nr);
		r = name_to_gid (m->dest, &idlist->groups.groups[count]);
		decref (m);
		if (r == -1)
		{
			free (idlist);
			return (NULL);
		}
		nr += strlen (nr) + 1;
	}

	qsort ((void *) idlist->groups.groups, idlist->groups.ngroups,
	       sizeof (gid_t), gid_cmp);
	return ((void *) idlist);
}

static void *
aid_from_nrep_mapped (char *nrepp, Generic_list *map)
{
	struct map *m;
	uid_t *aid;
	int r;

	aid = (uid_t *) malloc (sizeof (*aid));
	if (aid == NULL)
		return (NULL);

	m = first_that (map, match_map_bysource, nrepp);
	if (m == NULL)
	{
		free (aid);
		return (NULL);
	}

	r = name_to_uid (m->dest, aid);
	decref (m);
	if (r == -1)
	{
		free (aid);
		aid = NULL;
	}
	return ((void *) aid);
}

/* ARGSUSED */
void *
dummy_from_nrep_mapped (char *nrepp, Generic_list *map)
{
	return (NULL);
}

void *
lrep_from_nrep (char *nrepp, u_short attrid, const char *domain)
{
	struct map_head *mh;
	struct map_domain *md;
	u_short mh_attrid;
	void *x;
	void *(*mapfunc) (char *nrepp, Generic_list *map);

	if (nrepp == NULL)
		return (NULL);

	/* find the attribute and domain */
	mh = first_that (&mappings, match_map_head_byid, &attrid);
	if (mh == NULL)
		return (NULL);
	mh_attrid = mh->attrid;
	md = first_that (&mh->domains, match_map_domain_bydomain, domain);
	decref (mh);
	if (md == NULL)
		return (NULL);

	/* see if native mapping is required */
	if (x = first_that (&md->local_map, match_map_bysource, NATIVE_MAP))
	{
		decref (x);
		decref (md);
		return (lrep_from_nrep_native (nrepp, attrid));
	}

	switch (mh_attrid)
	{
		case SATMP_ATTR_SEN_LABEL:
		case SATMP_ATTR_CLEARANCE:
			mapfunc = msen_from_nrep_mapped;
			break;
		case SATMP_ATTR_INTEGRITY_LABEL:
			mapfunc = mint_from_nrep_mapped;
			break;
		case SATMP_ATTR_PRIVILEGES:
			mapfunc = cap_from_nrep_mapped;
			break;
		case SATMP_ATTR_AUDIT_ID:
			mapfunc = aid_from_nrep_mapped;
			break;
		case SATMP_ATTR_IDS:
			mapfunc = ids_from_nrep_mapped;
			break;
		case SATMP_ATTR_ACL:
		case SATMP_ATTR_AUDIT_INFO:
		case SATMP_ATTR_INFO_LABEL:
		case SATMP_ATTR_NATIONAL_CAVEATS:
			mapfunc = dummy_from_nrep_mapped;
			break;
	}
	x = (*mapfunc) (nrepp, &md->local_map);
	decref (md);
	return (x);
}

static int
configure_lrep_mappings_internal (const char *mapfile, int remote)
{
	char buf[BUFSIZ];
	FILE *fp;
	int lineno = 0;

	/* open config file */
	if (debug_on (DEBUG_FILE_OPEN))
		debug_print ("opening file \"%s\"\n", mapfile);
	fp = fopen (mapfile, "r");
	if (fp == NULL)
	{
		if (debug_on (DEBUG_OPEN_FAIL))
			debug_print ("can't open file \"%s\"\n", mapfile);
		return (1);
	}

	while (fgets (buf, sizeof (buf), fp) != NULL)
	{
		struct map_head *mh;
		struct map_domain *md;
		struct map *m;
		char *attrname, *dot, *src, *dst, *p;
		u_short attrid;

		/* update line count */
		lineno++;

		/* skip empty lines, allow # as comment character */
		p = buf;
		while (*p != '\0' && isspace (*p))
			p++;
		if (*p == '\n' || *p == '#' || *p == '\0')
			continue;

		/* assign fields */
		attrname = p;
		dot = skipfield (attrname, ':');
		src = skipfield (dot, ':');
		dst = skipfield (src, ':');

		/* remove trailing whitespace from attrname */
		p = attrname + strlen (attrname);
		while (p != attrname && isspace (*(p - 1)))
			*--p = '\0';

		/* remove leading & trailing whitespace from dot */
		while (*dot != '\0' && isspace (*dot))
			dot++;
		p = dot + strlen (dot);
		while (p != dot && isspace (*(p - 1)))
			*--p = '\0';

		/* remove leading & trailing whitespace from src */
		while (*src != '\0' && isspace (*src))
			src++;
		p = src + strlen (src);
		while (p != src && isspace (*(p - 1)))
			*--p = '\0';

		/* remove leading & trailing whitespace from dst */
		while (*dst != '\0' && isspace (*dst))
			dst++;
		p = dst + strlen (dst);
		while (p != dst && isspace (*(p - 1)))
			*--p = '\0';

		/* convert attribute value */
		attrid = attribute_from_text (attrname);
		if (attrid == BAD_ATTRID)
		{
			debug_print ("%s (line %d): %s: cannot convert attribute name\n", mapfile, lineno, attrname);
			fclose (fp);
			return (1);
		}

		mh = first_that (&mappings, match_map_head_byid, &attrid);
		if (mh == NULL)
		{
			mh = initialize_map_head (attrid);
			if (mh == NULL)
			{
				debug_print ("%s (line %d): %hu: cannot initialize from attribute id\n", mapfile, lineno, attrid);
				fclose (fp);
				return (1);
			}
			add_to_list (&mappings, mh);
		}

		md = first_that (&mh->domains, match_map_domain_bydomain, dot);
		if (md == NULL)
		{
			md = initialize_map_domain (dot);
			if (md == NULL)
			{
				debug_print ("%s (line %d): %s: cannot initialize from domain\n", mapfile, lineno, dot);
				decref (mh);
				fclose (fp);
				return (1);
			}
			add_to_list (&mh->domains, md);
		}
		decref (mh);

		m = initialize_map (src, dst);
		if (m == NULL)
		{
			debug_print ("%s (line %d): %s/%s: cannot initialize map from src/dst\n", mapfile, lineno, src, dst);
			fclose (fp);
			return (1);
		}
		add_to_list (remote ? &md->remote_map : &md->local_map, m);
		decref (md);
		decref (m);
	}

	fclose (fp);
	return (0);
}

int
configure_lrep_mappings (const char *configdir)
{
	char mapfile[PATH_MAX];

	initialize_list (&mappings);

	/* open local map file */
	sprintf (mapfile, "%s/localmap", configdir);
	if (configure_lrep_mappings_internal (mapfile, 0))
		return (1);

	/* open remote map file */
	sprintf (mapfile, "%s/remotemap", configdir);
	if (configure_lrep_mappings_internal (mapfile, 1))
		return (1);

	if (debug_on (DEBUG_STARTUP))
		perform_on_list (&mappings, print_map_head, (void *) NULL);
	return (0);
}

void
deconfigure_lrep_mappings (void)
{
	destroy_list (&mappings);
}
