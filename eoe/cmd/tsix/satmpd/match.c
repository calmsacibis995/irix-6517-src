#ident "$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/t6satmp.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <ctype.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <string.h>
#include "list.h"
#include "lrep.h"
#include "match.h"
#include "parse.h"

/*
 * ATTRIBUTE MATCH FUNCTIONS
 */

#if 0
static int
match_acl (const void *ptr, const void *args)
{
	const struct attrval *av = (const struct attrval *) ptr;
	const acl_t a = (acl_t) av->_lr._lrep;
	const acl_t b = (acl_t) args;
	char *astr, *bstr;
	int status;

	astr = acl_to_text (a, (ssize_t *) NULL);
	if (astr == (char *) NULL)
		return (0);
	bstr = acl_to_text (b, (ssize_t *) NULL);
	if (bstr == (char *) NULL)
	{
		acl_free (astr);
		return (0);
	}
	status = (strcmp (astr, bstr) == 0);
	acl_free (astr);
	acl_free (bstr);

	return (status);
}
#endif

static int
match_cap (const void *ptr, const void *args)
{
	const struct attrval *av = (const struct attrval *) ptr;
	const cap_t a = (cap_t) av->_lr._lrep;
	const cap_t b = (cap_t) args;

	return (a->cap_effective == b->cap_effective &&
		a->cap_permitted == b->cap_permitted &&
		a->cap_inheritable == b->cap_inheritable);
}

static int
match_lblparts (mac_b_label *a, mac_b_label *b)
{
	/*
	 * check hierarchal component
	 * check non-hierarchal set count
	 */
	if (a->b_hier != b->b_hier || a->b_nonhier != b->b_nonhier)
		return (0);

	/* short-circuit check if there are 0 elements */
	if (a->b_nonhier == 0)
		return (1);
	return (memcmp (a->b_list, b->b_list,
			a->b_nonhier * sizeof (a->b_list[0])) == 0);
}

static int
match_msen (const void *ptr, const void *args)
{
	const struct attrval *av = (const struct attrval *) ptr;
	const msen_t a = (msen_t) av->_lr._lrep;
	const msen_t b = (msen_t) args;

	if (!msen_valid (a) || !msen_valid (b) || a->b_type != b->b_type)
		return (0);

	switch (a->b_type)
	{
		case MSEN_HIGH_LABEL:
		case MSEN_MLD_HIGH_LABEL:
		case MSEN_LOW_LABEL:
		case MSEN_MLD_LOW_LABEL:
		case MSEN_ADMIN_LABEL:
		case MSEN_EQUAL_LABEL:
			return (1);
		case MSEN_TCSEC_LABEL:
		case MSEN_MLD_LABEL:
			return (match_lblparts (a, b));
		default:
			return (0);
	}
}

static int
match_mint (const void *ptr, const void *args)
{
	const struct attrval *av = (const struct attrval *) ptr;
	const mint_t a = (mint_t) av->_lr._lrep;
	const mint_t b = (mint_t) args;

	if (!mint_valid (a) || !mint_valid (b) || a->b_type != b->b_type)
		return (0);

	switch (a->b_type)
	{
		case MINT_HIGH_LABEL:
		case MINT_LOW_LABEL:
		case MINT_EQUAL_LABEL:
			return (1);
		case MINT_BIBA_LABEL:
			return (match_lblparts (a, b));
		default:
			return (0);
	}
}

static int
match_aid (const void *ptr, const void *args)
{
	const struct attrval *av = (const struct attrval *) ptr;
	const uid_t *a = (const uid_t *) av->_lr._lrep;
	const uid_t *b = (const uid_t *) args;

	return (*a == *b);
}

static int
match_ids (const void *ptr, const void *args)
{
	const struct attrval *av = (const struct attrval *) ptr;
	const struct ids *aid = (const struct ids *) av->_lr._lrep;
	const struct ids *bid = (const struct ids *) args;
	u_short i;

	if (aid->user != bid->user ||
	    aid->primary_group != bid->primary_group ||
	    aid->groups.ngroups != bid->groups.ngroups)
		return (0);

	for (i = 0; i < aid->groups.ngroups; i++)
		if (aid->groups.groups[i] != bid->groups.groups[i])
			return (0);

	return (1);
}

/* ARGSUSED */
static int
match_none (const void *ptr, const void *args)
{
	return (0);
}

/*
 * Return a function, appropriate for this domain and attribute type, that
 * we can use to match an attribute value with one in the list.
 */
match_fn
match_attribute (u_short attrid)
{
	switch (attrid)
	{
		case SATMP_ATTR_SEN_LABEL:
			return (match_msen);
		case SATMP_ATTR_NATIONAL_CAVEATS:
			break;
		case SATMP_ATTR_INTEGRITY_LABEL:
			return (match_mint);
		case SATMP_ATTR_INFO_LABEL:
			break;
		case SATMP_ATTR_PRIVILEGES:
			return (match_cap);
		case SATMP_ATTR_AUDIT_ID:
			return (match_aid);
		case SATMP_ATTR_IDS:
			return (match_ids);
		case SATMP_ATTR_CLEARANCE:
			return (match_msen);
		case SATMP_ATTR_AUDIT_INFO:
			break;
		case SATMP_ATTR_ACL:
			break;
	}
	return (match_none);
}

/*
 * CLIENT MATCH FUNCTIONS
 */

int
match_host_byid (const void *ptr, const void *args)
{
	const struct host *h = (const struct host *) ptr;
	const u_int *hid = (const u_int *) args;

	return (h->hostid == *hid);
}

int
match_attrdot_byid (const void *ptr, const void *args)
{
	const struct attrdot *ad = (const struct attrdot *) ptr;
	const u_short *attrid = (const u_short *) args;

	return (ad->attrid == *attrid);
}

int
match_tserver_byid (const void *ptr, const void *args)
{
	const struct tserver *ts = (const struct tserver *) ptr;
	const u_int *hostid = (const u_int *) args;

	return (ts->hostid == *hostid);
}

int
match_tdomain_byname (const void *ptr, const void *args)
{
	const struct tdomain *td = (const struct tdomain *) ptr;
	const char *domain = (const char *) args;

	return (strcmp (td->domain, domain) == 0);
}

int
match_domain_byname (const void *ptr, const void *args)
{
	const struct domain *d = (const struct domain *) ptr;
	const char *domain = (const char *) args;

	return (strcmp (d->domain, domain) == 0);
}

int
match_attrtoken_byid (const void *ptr, const void *args)
{
	const struct attrtoken *at = (const struct attrtoken *) ptr;
	const u_short *attrid = (const u_short *) args;

	return (at->attrid == *attrid);
}

int
match_token (const void *ptr, const void *args)
{
	const struct token *t = (const struct token *) ptr;
	const u_int *token = (const u_int *) args;

	return (t->token == *token);
}

int
match_attrwt_byid (const void *ptr, const void *args)
{
	const struct attrwt *aw = (const struct attrwt *) ptr;
	const u_short *attr = (const u_short *) args;

	return (aw->attrid == *attr);
}

int
match_weight_bydomain (const void *ptr, const void *args)
{
	const struct weight *w = (const struct weight *) ptr;
	const char *domain = (const char *) args;

	return (strcmp (w->domain, domain) == 0);
}

int
match_weight_byvalue (const void *ptr, const void *args)
{
	const struct weight *w = (const struct weight *) ptr;
	const u_char *weight = (const u_char *) args;

	return (w->weight == *weight);
}

/*
 * SERVER MATCH FUNCTIONS
 */

int
match_attrname_byid (const void *ptr, const void *args)
{
	const struct attrname *a = (const struct attrname *) ptr;
	const u_short *attrid = (const u_short *) args;

	return (a->attrid == *attrid);
}

int
match_attrname_byname (const void *ptr, const void *args)
{
	const struct attrname *a = (const struct attrname *) ptr;
	const char *name = (const char *) args;

	return (strcmp (a->name, name) == 0);
}

int
match_attrval_bytoken (const void *ptr, const void *args)
{
	const struct attrval *av = (const struct attrval *) ptr;
	const u_int *token = (const u_int *) args;

	return (av->token == *token);
}

/*
 * ATTRIBUTE MAPPING MATCH FUNCTIONS
 */

int
match_map_head_byid (const void *ptr, const void *args)
{
	const struct map_head *mh = (const struct map_head *) ptr;
	const u_short *attrid = (const u_short *) args;

	return (mh->attrid == *attrid);
}

int
match_map_domain_bydomain (const void *ptr, const void *args)
{
	const struct map_domain *md = (const struct map_domain *) ptr;
	const char *domain = (const char *) args;

	return (strcmp (md->domain, domain) == 0);
}

int
match_map_bysource (const void *ptr, const void *args)
{
	const struct map *m = (const struct map *) ptr;
	const char *source = (const char *) args;

	return (strcmp (m->source, source) == 0);
}

static int
iscomma (int c)
{
	return (c == ',');
}

static int
match_map_astype (const void *ptr, const void *args, const char *type,
		     unsigned int maxcount)
{
	const struct map *m = (const struct map *) ptr;
	const char *tofind = (const char *) args;
	char **words;
	unsigned int count;
	int result;

	words = parse_line (m->source, (pl_quote_func) NULL, iscomma, &count);
	if (words == NULL)
		return (0);
	if ((maxcount != 0 && count != maxcount) ||
	    strcmp (words[0], type) != 0)
		result = 0;
	else
		result = strcmp (words[1], tofind) == 0;
	parse_free (words);
	return (result);
}

int
match_map_asuser (const void *ptr, const void *args)
{
	return (match_map_astype (ptr, args, "user", 2));
}

int
match_map_asgroup (const void *ptr, const void *args)
{
	return (match_map_astype (ptr, args, "group", 2));
}

int
match_map_aslbltype (const void *ptr, const void *args)
{
	return (match_map_astype (ptr, args, "type", 2));
}

int
match_map_aslevel (const void *ptr, const void *args)
{
	return (match_map_astype (ptr, args, "level", 2));
}

int
match_map_ascategory (const void *ptr, const void *args)
{
	return (match_map_astype (ptr, args, "category", 2));
}
