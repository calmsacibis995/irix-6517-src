#ident "$Revision: 1.4 $"

#include <sys/types.h>
#include <sys/endian.h>		/* needed by t6satmp.h */
#include <sys/t6satmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <netinet/in.h>		/* for INADDR_LOOPBACK */
#include "list.h"
#include "client.h"
#include "match.h"
#include "parse.h"
#include "server.h"
#include "lrep.h"
#include "debug.h"
#include "sem.h"
#include "request.h"

static struct client *clnt_handle;

static int
configure_reqattr (Generic_list *reqattr, const char *configdir)
{
	char configfile[PATH_MAX], buf[BUFSIZ];
	FILE *fp;
	int lineno = 0;

	/* open config file */
	sprintf (configfile, "%s/REQATTR", configdir);
	if (debug_on (DEBUG_FILE_OPEN))
		debug_print ("opening file \"%s\"\n", configfile);
	fp = fopen (configfile, "r");
	if (fp == NULL)
	{
		if (debug_on (DEBUG_OPEN_FAIL))
			debug_print ("can't open file \"%s\"\n", configfile);
		return (1);
	}

	while (fgets (buf, sizeof (buf), fp) != NULL)
	{
		char *end, *p;
		struct reqattr *ra;

		/* update line count */
		lineno++;

		/* skip empty lines, allow # as comment character */
		p = buf;
		while (*p != '\0' && isspace (*p))
			p++;
		if (*p == '\n' || *p == '#' || *p == '\0')
			continue;

		/* remove trailing whitespace */
		end = p + strlen (p);
		while (end != buf && isspace (*(end - 1)))
			*--end = '\0';

		/* allocate attribute id */
		ra = initialize_reqattr (attribute_from_text (p));
		if (ra == NULL)
		{
			debug_print ("%s (line %d): %s: cannot convert to attribute identifier\n", configfile, lineno, p);
			fclose (fp);
			return (1);
		}

		add_to_list (reqattr, ra);
		decref (ra);
	}

	fclose (fp);
	return (0);
}

static int
configure_attrwt (Generic_list *weights, const char *configdir)
{
	char weight_file[PATH_MAX], buf[BUFSIZ];
	FILE *fp;
	int lineno = 0;

	/* open config file */
	sprintf (weight_file, "%s/WEIGHTS", configdir);
	if (debug_on (DEBUG_FILE_OPEN))
		debug_print ("opening file \"%s\"\n", weight_file);
	fp = fopen (weight_file, "r");
	if (fp == NULL)
	{
		if (debug_on (DEBUG_OPEN_FAIL))
			debug_print ("can't open file \"%s\"\n", weight_file);
		return (1);
	}

	/* parse weights database */
	while (fgets (buf, sizeof (buf), fp) != NULL)
	{
		struct attrwt *aw;
		struct weight *wt;
		u_char value;
		u_short attrid;
		char *p, *attr, *domain, *valuep, *end;

		/* update line count */
		lineno++;

		/* skip empty lines, allow # as comment character */
		p = buf;
		while (*p != '\0' && isspace (*p))
			p++;
		if (*p == '\n' || *p == '#' || *p == '\0')
			continue;

		/* assign fields */
		attr = p;
		domain = skipfield (attr, ':');
		valuep = skipfield (domain, ':');

		/* remove trailing whitespace from attr */
		p = attr + strlen (attr);
		while (p != attr && isspace (*(p - 1)))
			*--p = '\0';

		/* remove leading & trailing whitespace from domain */
		while (*domain != '\0' && isspace (*domain))
			domain++;
		p = domain + strlen (domain);
		while (p != domain && isspace (*(p - 1)))
			*--p = '\0';

		/* remove leading & trailing whitespace from valuep */
		while (*valuep != '\0' && isspace (*valuep))
			valuep++;
		p = valuep + strlen (valuep);
		while (p != valuep && isspace (*(p - 1)))
			*--p = '\0';

		/* convert attribute value */
		attrid = attribute_from_text (attr);
		if (attrid == BAD_ATTRID)
		{
			debug_print ("%s (line %d): %s: cannot convert attribute name\n", weight_file, lineno, attr);
			fclose (fp);
			return (1);
		}

		/* convert to char */
		value = (u_char) strtol (valuep, &end, 10);
		if ((value == 0 && end == valuep) || *end != '\0')
		{
			debug_print ("%s (line %d): %s: cannot convert to short integer\n", weight_file, lineno, valuep);
			fclose (fp);
			return (1);
		}

		aw = first_that (weights, match_attrwt_byid, &attrid);
		if (aw == NULL)
		{
			aw = initialize_attrwt (attrid);
			if (aw == NULL)
			{
				debug_print ("%s (line %d): %hu: cannot initialize attribute weight\n", weight_file, lineno, attrid);
				fclose (fp);
				return (1);
			}
			add_to_list (weights, aw);
		}

		wt = initialize_weight (domain, value);
		if (wt == NULL)
		{
			debug_print ("%s (line %d): %s/%hu: cannot initialize domain/value pair\n", weight_file, lineno, domain, (u_short) value);
			decref (aw);
			fclose (fp);
			return (1);
		}
		add_to_list (&aw->weights, wt);
		decref (wt);
		decref (aw);
	}

	fclose (fp);
	return (0);
}

static u_int
choose_server (init_request *req)
{
	switch (req->server_state)
	{
		case T6_IS_TOKEN_SERVER:
			return (req->hostid);
		case T6_USING_PRIMARY_SERVER:
			return (req->token_server);
		case T6_USING_BACKUP_SERVER:
			return (req->backup_server);
	}

	/* this should never happen */
	return (0);
}

static void
purge_client_internal (struct client *c, struct host *h)
{
	decref (remove_from_list (&c->hosts, h));
}

#if 0
void
purge_client_byhost (void *handle, u_int hostid)
{
	struct host *h;
	
	h = first_that (&clnt_handle->hosts, match_host_byid, &hostid);
	if (h != NULL)
		purge_client_internal (clnt_handle, h);
}
#endif

static init_request_dot_rep *
choose_dot (init_request_attr_format *af, Generic_list weights)
{
	static init_request_dot_rep invalid = {0, 0, 11, "INVALID_DOT"};
	u_short weight = 0, i;
	init_request_dot_rep *dr = &invalid;

	for (i = 0; i < af->dot_count; i++)
	{
		struct weight *w;

		/* we only accept ascii attribute representation */
		if (!(af->dot[i].flags & DOT_ASCII_ATTR_REP))
			continue;

		/* see if we support this domain */
		w = first_that (&weights, match_weight_bydomain,
				af->dot[i].dot_name);
		if (w == NULL)
			continue;

		if (af->dot[i].weight + w->weight > weight)
		{
			weight = af->dot[i].weight + w->weight;
			dr = &af->dot[i];
		}
		decref (w);
	}

	return (dr);
}

typedef struct reqattr_context
{
	Generic_list *list;
	int found;
} reqattr_context;

static void
reqattr_list_traverse (void *a, void *b)
{
	struct reqattr *ra = (struct reqattr *) a;
	reqattr_context *cp = (reqattr_context *) b;
	struct attrdot *ad;

	if (cp->found)
		return;
	ad = first_that (cp->list, match_attrdot_byid, &ra->attrid);
	if (ad == NULL)
		cp->found = 1;
	decref (ad);
}

static int
check_reqattr (Generic_list *reqattr, Generic_list *attrdots)
{
	reqattr_context context;

	context.list = attrdots;
	context.found = 0;
	perform_on_list (reqattr, reqattr_list_traverse, &context);
	return (context.found);
}

static void
cir_list_traverse (void *a, void *b)
{
	struct attrdot *ad = (struct attrdot *) a;
	init_request_attr_format **fmt = (init_request_attr_format **) b;

	if (*fmt == NULL)
		return;

	/* initialize attr_format */
	(*fmt)->attribute = ad->attrid;
	(*fmt)->dot_count = 1;
	(*fmt)->dot = malloc (sizeof (*(*fmt)->dot));
	if ((*fmt)->dot == NULL)
	{
		*fmt = NULL;
		return;
	}

	/* initialize dot_rep */
	(*fmt)->dot->weight = 0;
	(*fmt)->dot->flags = DOT_ASCII_ATTR_REP;
	(*fmt)->dot->length = strlen (ad->domain) + 1;
	(*fmt)->dot->dot_name = malloc ((*fmt)->dot->length);
	if ((*fmt)->dot->dot_name == NULL)
	{
		*fmt = NULL;
		return;
	}
	strcpy ((*fmt)->dot->dot_name, ad->domain);
	(*fmt)++;
}

static int
create_init_reply (init_reply *rep, Generic_list *attrdots)
{
	init_request_attr_format *format;

	/* initialize rep itself */
	rep->format = malloc (rep->format_count * sizeof (*rep->format));
	if (rep->format == NULL)
		return (1);
	rep->generation = server_generation;
	rep->version = T6_VERSION_NUMBER;
	rep->server_state = 0;
	rep->hostid = 0;
	rep->token_server = 0;
	rep->backup_server = 0;

	format = rep->format;
	perform_on_list (attrdots, cir_list_traverse, &format);
	if (format == NULL)
		destroy_init_reply (rep);
	return (format == NULL);
}

static void
purge_token_cache (struct tserver *ts, u_int generation)
{
	remove_all (&ts->domains);
	setatomic (&ts->generation, generation);
}

int
receive_init_request (init_request *req, init_reply *rep)
{
	struct host *h;
	struct tserver *ts;
	u_int server;
	u_short i;
	int r;

	/* check protocol version */
	if (req->version != T6_VERSION_NUMBER)
		return (SATMP_INVALID_VERSION);

	/* get the client's token server */
	server = choose_server (req);
	if (server == 0)
		return (SATMP_INTERNAL_ERROR);

	/* get client information if available, then purge it */
	h = first_that (&clnt_handle->hosts, match_host_byid, &req->hostid);
	if (h != NULL)
	{
		/*
		 * Always purge client information. If this transaction cannot
		 * be completed, no information will be saved anyway.
		 */
		decref (h);
		purge_client_internal (clnt_handle, h);
	}

	/* check token cache if available, initialize it if not */
	ts = first_that (&clnt_handle->servers, match_tserver_byid, &server);
	if (ts != NULL)
	{
		/* cache exists: check its generation number */
		if (req->generation < (u_int) getatomic (&ts->generation))
		{
			decref (ts);
			return (SATMP_INVALID_GENERATION);
		}
		if (req->generation > (u_int) getatomic (&ts->generation))
			purge_token_cache (ts, req->generation);
	}
	else
	{
		/* no cache: create one */
		ts = initialize_tserver (server, req->generation);
		if (ts == NULL)
			return (SATMP_INTERNAL_ERROR);
		add_to_list (&clnt_handle->servers, ts);
	}
	decref (ts);

	/* initialize host data */
	h = initialize_host (req->hostid, server);
	if (h == NULL)
		return (SATMP_INTERNAL_ERROR);

	rep->format_count = 0;
	for  (i = 0; i < req->format_count; i++)
	{
		struct attrwt *aw;
		struct attrdot *ad;
		init_request_dot_rep *dr;

		/* find attribute, if supported */
		aw = first_that (&clnt_handle->attrwt, match_attrwt_byid,
				 &req->format[i].attribute);
		if (aw == NULL)
			continue;

		/* choose a DOT for it */
		dr = choose_dot (&req->format[i], aw->weights);
		decref (aw);

		/* create entry for this host */
		ad = initialize_attrdot (req->format[i].attribute,
					 dr->dot_name);
		if (ad == NULL)
		{
			decref (h);
			return (SATMP_INTERNAL_ERROR);
		}

		add_to_list (&h->attrdots, ad);
		decref (ad);
		rep->format_count++;
	}

	/* ensure that required attributes are enforced */
	if (check_reqattr (&clnt_handle->reqattr, &h->attrdots))
	{
		decref (h);
		return (SATMP_INTERNAL_ERROR);
	}

	add_to_list (&clnt_handle->hosts, h);

	/*
	 * Generate reply
	 */
	r = create_init_reply (rep, &h->attrdots);
	decref (h);
	if (r)
		return (SATMP_INTERNAL_ERROR);

	return (SATMP_REPLY_OK);
}

int
receive_init_reply (init_reply *rep)
{
	struct host *h;
	struct tserver *ts;
	u_int server;
	u_short i;
	int error = SATMP_INTERNAL_ERROR;

	/* check protocol version */
	if (rep->version != T6_VERSION_NUMBER)
		return (SATMP_INVALID_VERSION);

	/* get the client's token server */
	server = choose_server (rep);
	if (server == 0)
		return (error);

	/* get client information if available, then purge it */
	h = first_that (&clnt_handle->hosts, match_host_byid, &rep->hostid);
	if (h != NULL)
	{
		/*
		 * Always purge client information. If this transaction cannot
		 * be completed, no information will be saved anyway.
		 */
		decref (h);
		purge_client_internal (clnt_handle, h);
	}

	/* check token cache if available, initialize it if not */
	ts = first_that (&clnt_handle->servers, match_tserver_byid, &server);
	if (ts != NULL)
	{
		/* cache exists: check its generation number */
		if (rep->generation < (u_int) getatomic (&ts->generation))
		{
			decref (ts);
			return (SATMP_INVALID_GENERATION);
		}
		if (rep->generation > (u_int) getatomic (&ts->generation))
			purge_token_cache (ts, rep->generation);
	}
	else
	{
		/* no cache: create one */
		ts = initialize_tserver (server, rep->generation);
		if (ts == NULL)
			return (SATMP_INTERNAL_ERROR);
		add_to_list (&clnt_handle->servers, ts);
	}
	decref (ts);

	/* initialize host data */
	h = initialize_host (rep->hostid, server);
	if (h == NULL)
		return (SATMP_INTERNAL_ERROR);

	for  (i = 0; i < rep->format_count; i++)
	{
		struct attrwt *aw;
		struct attrdot *ad;
		init_request_dot_rep *dr;

		/* find attribute, if supported */
		aw = first_that (&clnt_handle->attrwt, match_attrwt_byid,
				 &rep->format[i].attribute);
		if (aw == NULL)
			continue;

		/* choose a DOT for it */
		dr = choose_dot (&rep->format[i], aw->weights);
		decref (aw);

		/* create entry for this host */
		ad = initialize_attrdot (rep->format[i].attribute,
					 dr->dot_name);
		if (ad == NULL)
		{
			decref (h);
			return (SATMP_INTERNAL_ERROR);
		}

		add_to_list (&h->attrdots, ad);
		decref (ad);
	}

	/* ensure that required attributes are enforced */
	if (check_reqattr (&clnt_handle->reqattr, &h->attrdots))
	{
		decref (h);
		return (SATMP_INTERNAL_ERROR);
	}

	add_to_list (&clnt_handle->hosts, h);
	decref (h);

	return (SATMP_REPLY_OK);
}

typedef struct cir_context_two
{
	struct attrwt *aw;
	init_request_dot_rep *dot;
} cir_context_two;

static void
cir_list2_traverse (void *a, void *b)
{
	struct weight *w = (struct weight *) a;
	cir_context_two *cp = (cir_context_two *) b;

	if (cp->dot == NULL)
		return;
	cp->dot->weight = w->weight;
	cp->dot->flags = DOT_ASCII_ATTR_REP;
	cp->dot->length = strlen (w->domain) + 1;
	cp->dot->dot_name = malloc (cp->dot->length);
	if (cp->dot->dot_name == NULL)
	{
		cp->dot = NULL;
		return;
	}
	strcpy (cp->dot->dot_name, w->domain);
	cp->dot++;
}

typedef struct cir_context_one
{
	init_request *req;
	init_request_attr_format *fmt;
} cir_context_one;

static void
cir_list1_traverse (void *a, void *b)
{
	struct attrwt *aw = (struct attrwt *) a;
	cir_context_one *cp = (cir_context_one *) b;

	if (cp->fmt == NULL)
		return;
	cp->fmt->attribute = aw->attrid;
	cp->fmt->dot_count = num_of_objects (&aw->weights);
	cp->fmt->dot = malloc (cp->fmt->dot_count * sizeof (*cp->fmt->dot));
	if (cp->fmt->dot == NULL)
	{
		cp->req->format_count = cp->fmt - cp->req->format;
		cp->fmt = NULL;
		return;
	}

	{
		cir_context_two context;
		
		context.aw = aw;
		context.dot = cp->fmt->dot;
		perform_on_list (&aw->weights, cir_list2_traverse, &context);
		if (context.dot == NULL)
		{
			cp->fmt = NULL;
			return;
		}
	}
	cp->fmt++;
}

int
create_init_request (init_request *req)
{
	cir_context_one context;

	req->generation = server_generation;
	req->version = T6_VERSION_NUMBER;
	req->server_state = T6_IS_TOKEN_SERVER;
	req->token_server = 0;
	req->backup_server = 0;
	req->format_count = num_of_objects (&clnt_handle->attrwt);
	req->format = malloc (req->format_count * sizeof (*req->format));
	if (req->format == NULL)
		return (SATMP_INTERNAL_ERROR);

	context.req = req;
	context.fmt = req->format;
	perform_on_list (&clnt_handle->attrwt, cir_list1_traverse, &context);
	if (context.fmt == NULL)
		destroy_init_request (req);
	return (context.fmt != NULL ? SATMP_REPLY_OK : SATMP_INTERNAL_ERROR);
}

int
receive_get_attr_reply (get_attr_reply *rep)
{
	get_attr_reply_attr_spec *attr_spec;
	struct host *h = NULL;
	struct tserver *ts = NULL;
	u_short i;
	int error = SATMP_REPLY_OK;

	/* client must perform a handshake first */
	h = first_that (&clnt_handle->hosts, match_host_byid, &rep->hostid);
	if (h == NULL)
		return (SATMP_INTERNAL_ERROR);

	/* find this client's token server */
	ts = first_that (&clnt_handle->servers, match_tserver_byid,
			 &h->token_server);
	if (ts == NULL)
	{
		decref (h);
		return (SATMP_INTERNAL_ERROR);
	}

	/* add each element in the list to the token cache for this server */
	for (i = 0, attr_spec = rep->attr_spec;
	     i < rep->attr_spec_count; i++, attr_spec++)
	{
		struct attrdot *ad;
		struct tdomain *td;
		struct attrtoken *at;
		struct token *t;

		/* find the domain this client uses for this attribute */
		ad = first_that (&h->attrdots, match_attrdot_byid,
				 &attr_spec->attribute);
		if (ad == NULL)
			goto internal_error;

		/* find that domain for its server */
		td = first_that (&ts->domains, match_tdomain_byname,
				 ad->domain);
		if (td == NULL)
		{
			td = initialize_tdomain (ad->domain);
			if (td == NULL)
			{
				decref (ad);
				goto internal_error;
			}
			add_to_list (&ts->domains, td);
		}

		/* find the attributes for this domain */
		at = first_that (&td->attributes, match_attrtoken_byid,
				 &attr_spec->attribute);
		if (at == NULL)
		{
			at = initialize_attrtoken (attr_spec->attribute);
			if (at == NULL)
			{
				decref (ad);
				decref (td);
				goto internal_error;
			}
			add_to_list (&td->attributes, at);
		}
		decref (td);

		/* find the token list for these attributes */
		t = first_that (&at->tokens, match_token, &attr_spec->token);
		if (t == NULL)
		{
			void *lrep;

			lrep = lrep_from_nrep (attr_spec->nr, at->attrid,
					       ad->domain);
			t = initialize_token (attr_spec->token, at->attrid,
					      lrep);
			if (t == NULL)
			{
				destroy_lrep (lrep, at->attrid);
				decref (ad);
				decref (at);
				goto internal_error;
			}
			add_to_list (&at->tokens, t);
		}
		decref (ad);
		decref (t);
		decref (at);
	}

done:
	decref (h);
	decref (ts);
	return (error);
internal_error:
	error = SATMP_INTERNAL_ERROR;
	goto done;
}

int
xlate_get_attr_reply (get_attr_request *req, get_attr_reply *rep)
{
	u_short i;
	struct host *h = NULL;
	struct tserver *ts = NULL;
	int error = SATMP_REPLY_OK;

	rep->hostid = req->hostid;
	rep->generation = req->generation;
	rep->attr_spec_count = req->token_spec_count;

	/* client must perform a handshake first */
	h = first_that (&clnt_handle->hosts, match_host_byid, &rep->hostid);
	if (h == NULL)
		goto internal_error;

	/* find this client's token server */
	ts = first_that (&clnt_handle->servers, match_tserver_byid,
			 &h->token_server);
	if (ts == NULL)
		goto internal_error;

	/* allocate attribute list */
	rep->attr_spec = malloc (rep->attr_spec_count *
				 sizeof (*rep->attr_spec));
	if (rep->attr_spec == NULL)
		goto internal_error;

	for (i = 0; i < rep->attr_spec_count; i++)
	{
		struct attrdot *ad;
		struct tdomain *td;
		struct attrtoken *at;
		struct token *t;
		size_t lrep_size;

		/* initialize new attr_spec */
		rep->attr_spec[i].attribute = req->token_spec[i].attribute;
		rep->attr_spec[i].token = req->token_spec[i].token;
		rep->attr_spec[i].nr = NULL;
		rep->attr_spec[i].nr_length = 0;

		/* find the domain this client uses for this attribute */
		ad = first_that (&h->attrdots, match_attrdot_byid,
				 &rep->attr_spec[i].attribute);
		if (ad == NULL)
		{
			rep->attr_spec[i].status = SATMP_INTERNAL_ERROR;
			continue;
		}

		/* find that domain for its server */
		td = first_that (&ts->domains, match_tdomain_byname,
				 ad->domain);
		decref (ad);
		if (td == NULL)
		{
			rep->attr_spec[i].status = SATMP_NO_DOT;
			continue;
		}

		/* find the attributes for this domain */
		at = first_that (&td->attributes, match_attrtoken_byid,
				 &rep->attr_spec[i].attribute);
		decref (td);
		if (at == NULL)
		{
			rep->attr_spec[i].status = SATMP_NO_MAPPING;
			continue;
		}

		/* find the token list for these attributes */
		t = first_that (&at->tokens, match_token,
				&rep->attr_spec[i].token);
		decref (at);
		if (t == NULL)
		{
			rep->attr_spec[i].status = SATMP_NO_MAPPING;
			continue;
		}

		/* copy local representation */
		rep->attr_spec[i].nr = lrep_copy (t->lrep,
						  rep->attr_spec[i].attribute,
						  &lrep_size);
		decref (t);
		if (rep->attr_spec[i].nr == NULL)
		{
			rep->attr_spec[i].status = SATMP_INTERNAL_ERROR;
			continue;
		}

		/* success! */
		rep->attr_spec[i].nr_length = lrep_size;
		rep->attr_spec[i].status = SATMP_REPLY_OK;
	}

done:
	decref (h);
	decref (ts);
	return (error);
internal_error:
	error = SATMP_INTERNAL_ERROR;
	goto done;
}

static get_tok_request_attr_spec *
match_tok_request_bymid (get_tok_request *req, u_int mid)
{
	u_short i;

	for (i = 0; i < req->attr_spec_count; i++)
		if (req->attr_spec[i].mid == mid)
			return (&req->attr_spec[i]);
	return (NULL);
}

int
receive_get_tok_reply (get_tok_request *req, get_tok_reply *rep)
{
	get_tok_reply_token_spec *token_spec;
	struct host *h = NULL;
	struct tserver *ts = NULL;
	u_short i;
	int error = SATMP_REPLY_OK;

	/* client must perform a handshake first */
	h = first_that (&clnt_handle->hosts, match_host_byid, &rep->hostid);
	if (h == NULL)
		goto internal_error;

	/* find this client's token server */
	ts = first_that (&clnt_handle->servers, match_tserver_byid,
			 &h->token_server);
	if (ts == NULL)
		goto internal_error;

	/* check for out-of-date generation; flush if so */
	if (rep->generation != (u_int) getatomic (&ts->generation))
		purge_token_cache (ts, rep->generation);

	/* add each element in the list to the token cache for this server */
	for (i = 0, token_spec = rep->token_spec;
	     i < rep->token_spec_count; i++, token_spec++)
	{
		struct attrdot *ad;
		struct tdomain *td;
		struct attrtoken *at;
		struct token *t;

		/* find the domain this client uses for this attribute */
		ad = first_that (&h->attrdots, match_attrdot_byid,
				 &token_spec->attribute);
		if (ad == NULL)
			goto internal_error;

		/* find that domain for its server */
		td = first_that (&ts->domains, match_tdomain_byname,
				 ad->domain);
		if (td == NULL)
		{
			td = initialize_tdomain (ad->domain);
			if (td == NULL)
			{
				decref (ad);
				goto internal_error;
			}
			add_to_list (&ts->domains, td);
		}

		/* find the attributes for this domain */
		at = first_that (&td->attributes, match_attrtoken_byid,
				 &token_spec->attribute);
		if (at == NULL)
		{
			at = initialize_attrtoken (ad->attrid);
			if (at == NULL)
			{
				decref (ad);
				decref (td);
				goto internal_error;
			}
			add_to_list (&td->attributes, at);
		}
		decref (td);

		/* find the token list for these attributes */
		t = first_that (&at->tokens, match_token, &token_spec->token);
		if (t == NULL)
		{
			get_tok_request_attr_spec *gtras;
			void *lrep;

			gtras = match_tok_request_bymid (req, token_spec->mid);
			if (gtras == NULL)
			{
				decref (ad);
				decref (at);
				goto internal_error;
			}

			lrep = lrep_from_nrep (gtras->nr, at->attrid,
					       ad->domain);
			t = initialize_token (token_spec->token, at->attrid,
					      lrep);
			if (t == NULL)
			{
				destroy_lrep (lrep, at->attrid);
				decref (ad);
				decref (at);
				goto internal_error;
			}
			add_to_list (&at->tokens, t);
		}
		decref (ad);
		decref (t);
		decref (at);
	}

done:
	decref (h);
	decref (ts);
	return (error);
internal_error:
	error = SATMP_INTERNAL_ERROR;
	goto done;
}

int
receive_flush_remote_cache (u_int hostid)
{
	struct host *h;
	struct tserver *ts;

	/* find this host's token server */
	h = first_that (&clnt_handle->hosts, match_host_byid, &hostid);
	if (h == NULL)
		return (SATMP_INTERNAL_ERROR);

	/* find the token cache for that server */
	ts = first_that (&clnt_handle->servers, match_tserver_byid,
			 &h->token_server);
	decref (h);
	if (ts == NULL)
		return (SATMP_INTERNAL_ERROR);

	purge_token_cache (ts, 0);
	decref (ts);

	return (SATMP_REPLY_OK);
}

char *
attr_to_domain (u_int host, u_short attr)
{
	struct host *h;
	struct attrdot *ad;
	char *s = NULL;

	h = first_that (&clnt_handle->hosts, match_host_byid, &host);
	if (h == NULL)
		return (s);

	ad = first_that (&h->attrdots, match_attrdot_byid, &attr);
	decref (h);
	if (ad == NULL)
		return (s);

	s = (char *) malloc (strlen (ad->domain) + 1);
	if (s != NULL)
		strcpy (s, ad->domain);
	decref (ad);
	return (s);
}

int
create_loopback_reply (init_reply *rep)
{
	struct attrwt *aw;
	struct weight *w = NULL;
	u_short attr;
	const u_char weight = DOT_NATIVE_WEIGHT;

	/* initialize rep */
	rep->generation = server_generation;
	rep->version = T6_VERSION_NUMBER;
	rep->server_state = 0;
	rep->hostid = INADDR_LOOPBACK;
	rep->token_server = 0;
	rep->backup_server = 0;
	rep->format_count = 0;
	rep->format = NULL;

	for (attr = 0; attr < SATMP_ATTR_LAST + 1; attr++)
	{
		init_request_attr_format *iraf;
		init_request_dot_rep *irdr;

		aw = first_that (&clnt_handle->attrwt, match_attrwt_byid,
				 &attr);
		if (aw == NULL)
			continue;
		w = first_that (&aw->weights, match_weight_byvalue, &weight);
		decref (aw);
		if (w == NULL)
			continue;

		rep->format = realloc (rep->format, ++rep->format_count * sizeof (*rep->format));
		if (rep->format == NULL)
			goto error2;

		iraf = &rep->format[rep->format_count - 1];
		iraf->attribute = attr;
		iraf->dot_count = 1;
		iraf->dot = malloc (sizeof (*iraf->dot));
		if (iraf->dot == NULL)
			goto error;

		irdr = &iraf->dot[0];
		irdr->weight = weight;
		irdr->flags = DOT_ASCII_ATTR_REP;
		irdr->length = strlen (w->domain) + 1;
		irdr->dot_name = malloc (irdr->length);
		if (irdr->dot_name == NULL)
			goto error;
		memcpy (irdr->dot_name, w->domain, irdr->length);
		decref (w);
	}

	return (SATMP_REPLY_OK);
error:
	destroy_init_reply (rep);
error2:
	decref (w);
	return (SATMP_INTERNAL_ERROR);
}

int
get_client_generation (u_int host, u_int *generation)
{
	struct host *h;
	struct tserver *ts;

	/* client must perform a handshake first */
	h = first_that (&clnt_handle->hosts, match_host_byid, &host);
	if (h == NULL)
		return (1);

	/* find the token cache for that client */
	ts = first_that (&clnt_handle->servers, match_tserver_byid,
			 &h->token_server);
	decref (h);
	if (ts == NULL)
		return (1);

	if (generation != NULL)
		*generation = (u_int) getatomic (&ts->generation);
	decref (ts);

	return (0);
}

int
get_client_server (u_int host, struct sockaddr_in *server)
{
	struct host *h;
	struct tserver *ts;

	/* client must perform a handshake first */
	h = first_that (&clnt_handle->hosts, match_host_byid, &host);
	if (h == NULL)
		return (1);

	/* find the token cache for that client */
	ts = first_that (&clnt_handle->servers, match_tserver_byid,
			 &h->token_server);
	decref (h);
	if (ts == NULL)
		return (1);

	if (server != NULL)
		server->sin_addr.s_addr = ts->hostid;
	decref (ts);

	return (0);
}

int
configure_client (const char *configdir)
{
	const char *default_configdir = "/etc/satmpd";

	if (configdir == NULL)
		configdir = default_configdir;

	clnt_handle = initialize_client ();
	if (clnt_handle == NULL)
		return (1);

	if (configure_attrwt (&clnt_handle->attrwt, configdir) ||
	    configure_reqattr (&clnt_handle->reqattr, configdir))
	{
		deconfigure_client ();
		return (1);
	}

	if (debug_on (DEBUG_STARTUP))
		print_client (clnt_handle);

	return (0);
}

void
deconfigure_client (void)
{
	if (clnt_handle != NULL)
	{
		decref (clnt_handle);
		clnt_handle = NULL;
	}
}
