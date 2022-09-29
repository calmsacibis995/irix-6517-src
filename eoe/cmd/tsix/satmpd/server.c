#ident "$Revision: 1.4 $"

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/t6satmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include "list.h"
#include "match.h"
#include "parse.h"
#include "request.h"
#include "server.h"
#include "lrep.h"
#include "debug.h"
#include "ref.h"
#include "sem.h"

static struct server *srvr_handle;

u_int server_generation;

int
receive_get_tok_request (get_tok_request *req, get_tok_reply *rep)
{
	u_short i;
	int status = SATMP_REPLY_OK;

	rep->hostid = 0;
	rep->generation = srvr_handle->generation;
	rep->token_spec_count = 0;
	rep->token_spec = malloc (req->attr_spec_count *
				  sizeof (*rep->token_spec));
	if (rep->token_spec == NULL)
		return (SATMP_INTERNAL_ERROR);

	for (i = 0; i < req->attr_spec_count; i++)
	{
		u_int token;
		void *lr;

		/* find the token corresponding to this network
		   representation */
		lr = lrep_from_nrep (req->attr_spec[i].nr,
				     req->attr_spec[i].attribute,
				     req->attr_spec[i].domain);
		if (attr_to_token (req->attr_spec[i].domain,
				   req->attr_spec[i].attribute,
				   lr, &token))
		{
			destroy_lrep (lr, req->attr_spec[i].attribute);
			status = SATMP_TRANSLATION_ERROR;
			continue;
		}

		/* initialize new token_spec */
		rep->token_spec[rep->token_spec_count].attribute =
			req->attr_spec[i].attribute;
		rep->token_spec[rep->token_spec_count].mid =
			req->attr_spec[i].mid;
		rep->token_spec[rep->token_spec_count++].token = token;
	}

	return (status);
}

int
receive_get_lrtok_request (get_lrtok_request *req, get_lrtok_reply *rep)
{
	u_short i;

	rep->hostid = req->hostid;
	rep->generation = srvr_handle->generation;
	rep->token_spec_count = req->attr_spec_count;
	rep->token_spec = malloc (req->attr_spec_count *
				  sizeof (*rep->token_spec));
	if (rep->token_spec == NULL)
		return (SATMP_INTERNAL_ERROR);

	for (i = 0; i < req->attr_spec_count; i++)
	{
		u_int token;

		/* initialize new token_spec */
		rep->token_spec[i].attribute = req->attr_spec[i].attribute;
		rep->token_spec[i].mid = req->attr_spec[i].mid;
		rep->token_spec[i].token = 0;

		/* check for lack of domain */
		if (req->attr_spec[i].domain == NULL)
		{
			rep->token_spec[i].status = SATMP_NO_DOT;
			continue;
		}

		/* find the token corresponding to this network
		   representation */
		if (attr_to_token (req->attr_spec[i].domain,
				   req->attr_spec[i].attribute,
				   req->attr_spec[i].lr, &token))
		{
			rep->token_spec[i].status = SATMP_TRANSLATION_ERROR;
			continue;
		}

		rep->token_spec[i].status = SATMP_REPLY_OK;
		rep->token_spec[i].token = token;
	}

	return (SATMP_REPLY_OK);
}

/*
 * Map a token to the network representation of an entry in the database.
 * Not matching a database entry is an error. The database must be locked
 * before calling this function.
 */
static int
token_to_nr (const char *domain, u_short attrid, u_int token,
	     struct _nrep *nrepp)
{
	struct attrname *an;
	struct domain *d;
	struct attrval *av;
	int error = 0;

	/* search for matching attribute type */
	an = first_that (&srvr_handle->attribute_names, match_attrname_byid,
			 &attrid);
	if (an == NULL)
		return (1);

	/* find matching domain name */
	d = first_that (&an->domains, match_domain_byname, domain);
	decref (an);
	if (d == NULL)
		return (1);

	/* find matching nrep */
	av = first_that (&d->attrvals, match_attrval_bytoken, &token);
	decref (d);
	if (av == NULL)
		return (1);

	if (nrepp)
	{
		*nrepp = av->_nr;
		nrepp->_nrep = (char *) malloc (nrepp->_nrep_len);
		if (nrepp->_nrep != NULL)
			memcpy (nrepp->_nrep, av->_nr._nrep, nrepp->_nrep_len);
		else
			error = 1;
	}
	decref (av);

	return (error);
}

/*
 * Map a token to the local representation of an entry in the database.
 * Not matching a database entry is an error. The database must be locked
 * before calling this function.
 */
#if 0
static int
token_to_lrep (const char *domain, u_short attrid, u_int token,
	       struct _lrep **lrepp)
{
	struct attrname *an;
	struct domain *d;
	struct attrval *av;

	/* search for matching attribute type */
	an = first_that (&srvr_handle->attribute_names, match_attrname_byid,
			 &attrid);
	if (an == NULL)
		return (1);

	/* find matching domain name */
	d = first_that (&an->domains, match_domain_byname, domain);
	if (d == NULL)
		return (1);

	/* find matching lrep */
	av = first_that (&d->attrvals, match_attrval_bytoken, &token);
	if (av == NULL)
		return (1);

	if (lrepp)
		*lrepp = &av->_lr;

	return (0);
}
#endif

int
receive_get_attr_request (get_attr_request *req, get_attr_reply *rep)
{
	u_short i;
	int status = SATMP_REPLY_OK;

	rep->hostid = 0;
	rep->generation = req->generation;
	rep->attr_spec_count = 0;

	/* check generation */
	if (srvr_handle->generation != req->generation)
		return (SATMP_INVALID_GENERATION);

	rep->attr_spec = malloc (req->token_spec_count *
				 sizeof (*rep->attr_spec));
	if (rep->attr_spec == NULL)
		return (SATMP_INTERNAL_ERROR);

	for (i = 0; i < req->token_spec_count; i++)
	{
		struct _nrep nr;

		/* find the network representation corresponding to this
		   token */
		if (token_to_nr (req->token_spec[i].domain,
				 req->token_spec[i].attribute,
				 req->token_spec[i].token, &nr))
		{
			status = SATMP_NO_MAPPING;
			continue;
		}

		/* initialize new attr_spec */
		rep->attr_spec[rep->attr_spec_count].attribute =
			req->token_spec[i].attribute;
		rep->attr_spec[rep->attr_spec_count].nr_length = nr._nrep_len;
		rep->attr_spec[rep->attr_spec_count].token =
			req->token_spec[i].token;
		rep->attr_spec[rep->attr_spec_count].nr = (char *) malloc (rep->attr_spec[rep->attr_spec_count].nr_length);
		if (rep->attr_spec[rep->attr_spec_count].nr == NULL)
		{
			destroy_get_attr_reply (rep);
			return (SATMP_INTERNAL_ERROR);
		}
		memcpy (rep->attr_spec[rep->attr_spec_count++].nr, nr._nrep,
			nr._nrep_len);
		free ((void *) nr._nrep);
	}

	return (status);
}

/*
 * Map an attribute in native binary representation to a token in
 * the server database. Entries not in the database are added to it.
 */
int
attr_to_token (const char *domain, u_short attrid, void *attr, u_int *token)
{
	struct attrname *an;
	struct domain *d = NULL;
	struct attrval *av;

	if (domain == NULL || attr == NULL)
		return (1);

	/* search for matching attribute type */
	an = first_that (&srvr_handle->attribute_names, match_attrname_byid,
			 &attrid);
	if (an == NULL)
		goto out;

	/* search for matching domain */
	d = first_that (&an->domains, match_domain_byname, domain);
	if (d == NULL)
	{
		d = initialize_domain (domain, an->attrid);
		if (d == NULL)
		{
			decref (an);
			goto out;
		}
		add_to_list (&an->domains, d);
	}
	decref (an);

	/* see if this attribute value exists */
	av = first_that (&d->attrvals, match_attribute (attrid), attr);
	if (av == NULL)
	{
		char *nrepp;
		void *lr;
		size_t nrep_size, lrep_size;

		lr = lrep_copy (attr, attrid, &lrep_size);
		if (lr == NULL)
			goto out;

		nrepp = lrep_to_nrep (lr, attrid, domain, &nrep_size);
		if (nrepp == NULL)
		{
			destroy_lrep (lr, attrid);
			goto out;
		}

		av = initialize_attrval (lr, lrep_size, nrepp, nrep_size,
					 &d->seqno, attrid);
		free (nrepp);
		if (av == NULL)
		{
			destroy_lrep (lr, attrid);
			goto out;
		}
		add_to_list (&d->attrvals, av);
	}
	decref (d);

	if (token)
		*token = av->token;
	decref (av);

	return (0);
out:
	decref (d);
	return (1);
}

#if 0
char *
attribute_to_text (u_short attrid)
{
	struct attrname *a;
	char *unknown = "unknown attribute";

	a = first_that (&srvr_handle->attribute_names, match_attrname_byid,
			&attrid);
	return (a != NULL ? a->name : unknown);
}
#endif

u_short
attribute_from_text (const char *name)
{
	struct attrname *a;
	u_short attrid;

	a = first_that (&srvr_handle->attribute_names, match_attrname_byname,
			name);
	attrid = (a != NULL ? a->attrid : BAD_ATTRID);
	decref (a);
	return (attrid);
}

/*
 * create attribute<->attribute name map
 */
static int
configure_attribute_names (Generic_list *name_map, const char *configdir)
{
	char attrfile[PATH_MAX], buf[BUFSIZ];
	FILE *fp;
	int lineno = 0;

	/* open config file */
	sprintf (attrfile, "%s/ATTRIDS", configdir);
	if (debug_on (DEBUG_FILE_OPEN))
		debug_print ("opening file \"%s\"\n", attrfile);
	fp = fopen (attrfile, "r");
	if (fp == NULL)
	{
		if (debug_on (DEBUG_OPEN_FAIL))
			debug_print ("can't open file \"%s\"\n", attrfile);
		return (1);
	}

	while (fgets (buf, sizeof (buf), fp) != NULL)
	{
		struct attrname *a;
		char *name, *valuep, *end, *p;
		u_short value;

		/* update line count */
		lineno++;

		/* skip empty lines, allow # as comment character */
		p = buf;
		while (*p != '\0' && isspace (*p))
			p++;
		if (*p == '\n' || *p == '#' || *p == '\0')
			continue;

		/* assign fields */
		name = p;
		valuep = skipfield (name, ':');

		/* remove trailing whitespace from name */
		p = name + strlen (name);
		while (p != name && isspace (*(p - 1)))
			*--p = '\0';

		/* remove leading & trailing whitespace from valuep */
		while (*valuep != '\0' && isspace (*valuep))
			valuep++;
		p = valuep + strlen (valuep);
		while (p != valuep && isspace (*(p - 1)))
			*--p = '\0';

		/* convert to short */
		value = (u_short) strtoul (valuep, &end, 10);
		if ((value == 0 && end == valuep) || *end != '\0')
		{
			debug_print ("%s (line %d): %s: cannot convert to short integer\n", attrfile, lineno, valuep);
			fclose (fp);
			return (1);
		}

		a = initialize_attrname (name, value);
		if (a == NULL)
		{
			debug_print ("%s (line %d): %s/%hu: cannot convert name/value pair\n", attrfile, lineno, name, value);
			fclose (fp);
			return (1);
		}

		add_to_list (name_map, a);
		decref (a);
	}

	fclose (fp);
	return (0);
}

int
configure_server (const char *configdir)
{
	const char *default_configdir = "/etc/satmpd";

	if (configdir == NULL)
		configdir = default_configdir;

	srvr_handle = initialize_server ();
	if (srvr_handle == NULL)
		return (1);
	server_generation = srvr_handle->generation;

	if (configure_attribute_names (&srvr_handle->attribute_names,
				       configdir))
	{
		deconfigure_server ();
		return (1);
	}

	if (debug_on (DEBUG_STARTUP))
		print_server (srvr_handle);

	return (0);
}

void
deconfigure_server (void)
{
	if (srvr_handle != NULL)
	{
		decref (srvr_handle);
		srvr_handle = NULL;
	}
}
