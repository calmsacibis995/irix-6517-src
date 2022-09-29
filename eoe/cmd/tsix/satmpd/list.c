#ident "$Revision: 1.4 $"

#include <sys/types.h>
#include <sys/t6satmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "debug.h"
#include "list.h"
#include "lrep.h"

/*
 * SATMPD CLIENT INITIALIZE/DESTROY/PRINT FUNCTIONS
 */

static void
destroy_attrdot (void *ptr)
{
	struct attrdot *ad = (struct attrdot *) ptr;

	free (ad->domain);
	free (ad);
}

struct attrdot *
initialize_attrdot (u_short attrid, const char *domain)
{
	struct attrdot *ad;

	ad = (struct attrdot *) malloc (sizeof (*ad));
	if (ad == (struct attrdot *) NULL)
		return (ad);

	ad->domain = (char *) malloc (strlen (domain) + 1);
	if (ad->domain == (char *) NULL)
	{
		free (ad);
		return ((struct attrdot *) NULL);
	}
	strcpy (ad->domain, domain);

	ad->attrid = attrid;
	iniref (ad, destroy_attrdot);

	return (ad);
}

/* ARGSUSED */
static void
print_attrdot (void *ptr, void *args)
{
	struct attrdot *ad = (struct attrdot *) ptr;

	debug_print ("print_attrdot (%hu, %s)\n", ad->attrid, ad->domain);
}

static void
destroy_host (void *ptr)
{
	struct host *h = (struct host *) ptr;

	destroy_list (&h->attrdots);
	free (h);
}

struct host *
initialize_host (u_int hostid, u_int server)
{
	struct host *h;

	h = (struct host *) malloc (sizeof (*h));
	if (h == (struct host *) NULL)
		return (h);

	h->hostid = hostid;
	h->token_server = server;
	initialize_list (&h->attrdots);
	iniref (h, destroy_host);

	return (h);
}

static void
print_host (void *ptr, void *args)
{
	struct host *h = (struct host *) ptr;

	debug_print ("print_host (%u, %u)\n", h->hostid, h->token_server);
	perform_on_list (&h->attrdots, print_attrdot, args);
}

static void
destroy_weight (void *ptr)
{
	struct weight *w = (struct weight *) ptr;

	free (w->domain);
	free (w);
}

struct weight *
initialize_weight (const char *domain, u_char weight)
{
	struct weight *w;

	w = (struct weight *) malloc (sizeof (*w));
	if (w == (struct weight *) NULL)
		return (w);

	w->domain = (char *) malloc (strlen (domain) + 1);
	if (w->domain == (char *) NULL)
	{
		free (w);
		return ((struct weight *) NULL);
	}
	strcpy (w->domain, domain);

	w->weight = weight;
	iniref (w, destroy_weight);

	return (w);
}

/* ARGSUSED */
static void
print_weight (void *ptr, void *args)
{
	struct weight *w = (struct weight *) ptr;

	debug_print ("print_weight (%s, %u)\n", w->domain,
		     (unsigned int) w->weight);
}

static void
destroy_attrwt (void *ptr)
{
	struct attrwt *aw = (struct attrwt *) ptr;

	destroy_list (&aw->weights);
	free (aw);
}

struct attrwt *
initialize_attrwt (u_short attrid)
{
	struct attrwt *aw;

	if (attrid == BAD_ATTRID)
		return ((struct attrwt *) NULL);

	aw = (struct attrwt *) malloc (sizeof (*aw));
	if (aw == (struct attrwt *) NULL)
		return (aw);

	aw->attrid = attrid;
	initialize_list (&aw->weights);
	iniref (aw, destroy_attrwt);

	return (aw);
}

static void
print_attrwt (void *ptr, void *args)
{
	struct attrwt *aw = (struct attrwt *) ptr;

	debug_print ("print_attrwt (%hu)\n", aw->attrid);
	perform_on_list (&aw->weights, print_weight, args);
}

static void
destroy_reqattr (void *ptr)
{
	free (ptr);
}

struct reqattr *
initialize_reqattr (u_short attrid)
{
	struct reqattr *ra;

	if (attrid == BAD_ATTRID)
		return ((struct reqattr *) NULL);

	ra = (struct reqattr *) malloc (sizeof (*ra));
	if (ra == (struct reqattr *) NULL)
		return (ra);

	ra->attrid = attrid;
	iniref (ra, destroy_reqattr);

	return (ra);
}

/* ARGSUSED */
static void
print_reqattr (void *ptr, void *args)
{
	struct reqattr *ra = (struct reqattr *) ptr;

	debug_print ("print_reqattr (%hu)\n", ra->attrid);
}

static void
destroy_token (void *ptr)
{
	struct token *t = (struct token *) ptr;

	destroy_lrep (t->lrep, t->attrid);
	free (t);
}

struct token *
initialize_token (u_int token, u_short attrid, void *lrep)
{
	struct token *t;

	t = (struct token *) malloc (sizeof (*t));
	if (t == (struct token *) NULL)
		return (t);

	t->token = token;
	t->lrep = lrep;
	t->attrid = attrid;
	if (t->lrep == (void *) NULL)
	{
		free (t);
		return ((struct token *) NULL);
	}
	iniref (t, destroy_token);

	return (t);
}

/* ARGSUSED */
static void
print_token (void *ptr, void *args)
{
	struct token *t = (struct token *) ptr;
	char *s = lrep_to_text (t->lrep, t->attrid);

	debug_print ("print_token (%u, %s)\n", t->token,
		     s ? s : "can't print this attribute");
	destroy_lrep ((void *) s, t->attrid);
}

static void
destroy_attrtoken (void *ptr)
{
	struct attrtoken *at = (struct attrtoken *) ptr;

	destroy_list (&at->tokens);
}

struct attrtoken *
initialize_attrtoken (u_short attrid)
{
	struct attrtoken *at;

	at = (struct attrtoken *) malloc (sizeof (*at));
	if (at == (struct attrtoken *) NULL)
		return (at);

	at->attrid = attrid;
	initialize_list (&at->tokens);
	iniref (at, destroy_attrtoken);

	return (at);
}

static void
print_attrtoken (void *ptr, void *args)
{
	struct attrtoken *at = (struct attrtoken *) ptr;

	debug_print ("print_attrtoken (%hu)\n", at->attrid);
	perform_on_list (&at->tokens, print_token, args);
}

static void
destroy_tdomain (void *ptr)
{
	struct tdomain *td = (struct tdomain *) ptr;

	destroy_list (&td->attributes);
	free (td->domain);
	free (td);
}

struct tdomain *
initialize_tdomain (const char *domain)
{
	struct tdomain *td;

	td = (struct tdomain *) malloc (sizeof (*td));
	if (td == NULL)
		return (td);

	td->domain = (char *) malloc (strlen (domain) + 1);
	if (td->domain == NULL)
	{
		free (td);
		return (NULL);
	}
	strcpy (td->domain, domain);

	initialize_list (&td->attributes);
	iniref (td, destroy_tdomain);

	return (td);
}

/* ARGSUSED */
static void
print_tdomain (void *ptr, void *args)
{
	struct tdomain *d = (struct tdomain *) ptr;

	debug_print ("print_tdomain (%s)\n", d->domain);
	perform_on_list (&d->attributes, print_attrtoken, args);
}

static void
destroy_tserver (void *ptr)
{
	struct tserver *ts = (struct tserver *) ptr;

	destroy_list (&ts->domains);
	free (ts);
}

struct tserver *
initialize_tserver (u_int hostid, u_int generation)
{
	struct tserver *ts;

	ts = (struct tserver *) malloc (sizeof (*ts));
	if (ts == (struct tserver *) NULL)
		return (ts);

	ts->hostid = hostid;
	iniatomic (&ts->generation, generation);
	initialize_list (&ts->domains);
	iniref (ts, destroy_tserver);

	return (ts);
}

static void
print_tserver (void *ptr, void *args)
{
	struct tserver *ts = (struct tserver *) ptr;

	debug_print ("print_tserver (%u, %lu)\n", ts->hostid,
		     (unsigned long) getatomic (&ts->generation));
	perform_on_list (&ts->domains, print_tdomain, args);
}

static void
destroy_client (void *ptr)
{
	struct client *c = (struct client *) ptr;

	destroy_list (&c->hosts);
	destroy_list (&c->servers);
	destroy_list (&c->attrwt);
	destroy_list (&c->reqattr);
	free (c);
}

struct client *
initialize_client (void)
{
	struct client *c;

	c = (struct client *) malloc (sizeof (*c));
	if (c == (struct client *) NULL)
		return (c);

	initialize_list (&c->hosts);
	initialize_list (&c->servers);
	initialize_list (&c->attrwt);
	initialize_list (&c->reqattr);
	iniref (c, destroy_client);

	return (c);
}

void
print_client (struct client *c)
{
	perform_on_list (&c->hosts, print_host, (void *) NULL);
	perform_on_list (&c->attrwt, print_attrwt, (void *) NULL);
	perform_on_list (&c->reqattr, print_reqattr, (void *) NULL);
	perform_on_list (&c->servers, print_tserver, (void *) NULL);
}

/*
 * SATMPD SERVER INITIALIZE/DESTROY/PRINT FUNCTIONS
 */

static void
destroy_attrval (void *ptr)
{
	struct attrval *av = (struct attrval *) ptr;

	destroy_lrep (av->_lr._lrep, av->attrid);
	free (av->nrep);
	free (av);
}

struct attrval *
initialize_attrval (void *_lrep, size_t _lrep_len,
		    char *_nrep, size_t _nrep_len,
		    atomic *seqno, u_short attrid)
{
	struct attrval *av;

	av = (struct attrval *) malloc (sizeof (*av));
	if (av == NULL)
		return (av);
	av->nrep = (char *) malloc (av->nrep_len = _nrep_len + 1);
	if (av->nrep == NULL)
	{
		free (av);
		return (NULL);
	}
	memcpy (av->nrep, _nrep, av->nrep_len);
	av->_lr._lrep = _lrep;
	av->_lr._lrep_len = _lrep_len;
	av->token = (u_int) incatomic (seqno);
	av->attrid = attrid;
	iniref (av, destroy_attrval);
	return (av);
}

/* ARGSUSED */
static void
print_attrval (void *ptr, void *args)
{
	struct attrval *av = (struct attrval *) ptr;
	char *s = lrep_to_text (av->_lr._lrep, av->attrid);

	debug_print ("print_attrval (\"%s\", \"%s\", %lu, %u)\n",
		     s ? s : "invalid lrep", av->nrep,
		     (unsigned long) av->nrep_len, av->token);
	destroy_lrep ((void *) s, av->attrid);
}

static void
destroy_domain (void *ptr)
{
	struct domain *d = (struct domain *) ptr;

	destroy_list (&d->attrvals);
	free (d->domain);
	free (d);
}

struct domain *
initialize_domain (const char *domain, u_short attrid)
{
	struct domain *d;

	d = (struct domain *) malloc (sizeof (*d));
	if (d == NULL)
		return (d);
	d->domain = (char *) malloc (strlen (domain) + 1);
	if (d->domain == NULL)
	{
		free (d);
		return (NULL);
	}
	strcpy (d->domain, domain);
	initialize_list (&d->attrvals);
	iniref (d, destroy_domain);
	iniatomic (&d->seqno, 1);
	d->attrid = attrid;
	return (d);
}

static void
print_domain (void *ptr, void *args)
{
	struct domain *d = (struct domain *) ptr;

	debug_print ("print_domain (%s, %lu)\n", d->domain,
		     (unsigned long) getatomic (&d->seqno));
	perform_on_list (&d->attrvals, print_attrval, args);
}

static void
destroy_attrname (void *ptr)
{
	struct attrname *a = (struct attrname *) ptr;

	destroy_list (&a->domains);
	free (a->name);
	free (a);
}

struct attrname *
initialize_attrname (const char *name, u_short attrid)
{
	struct attrname *a;

	if (attrid == BAD_ATTRID)
		return (NULL);
	a = (struct attrname *) malloc (sizeof (*a));
	if (a == NULL)
		return (a);
	a->name = (char *) malloc (strlen (name) + 1);
	if (a->name == NULL)
	{
		free (a);
		return (NULL);
	}
	strcpy (a->name, name);
	a->attrid = attrid;
	initialize_list (&a->domains);
	iniref (a, destroy_attrname);
	return (a);
}

/* ARGSUSED */
static void
print_attrname (void *ptr, void *args)
{
	struct attrname *a = (struct attrname *) ptr;

	debug_print ("print_attrname (%s, %hu)\n", a->name, a->attrid);
	perform_on_list (&a->domains, print_domain, &a->attrid);
}

static void
destroy_server (void *ptr)
{
	struct server *s = (struct server *) ptr;

	destroy_list (&s->attribute_names);
	free (s);
}

struct server *
initialize_server (void)
{
	struct server *s;

	s = (struct server *) malloc (sizeof (*s));
	if (s == NULL)
		return (s);

	s->generation = (u_int) time ((time_t *) NULL);
	s->generation = (s->generation >> 8) & 0x00FFFFFF;
	initialize_list (&s->attribute_names);
	iniref (s, destroy_server);

	return (s);
}

void
print_server (struct server *s)
{
	debug_print ("print_server (%u)\n", s->generation);
	perform_on_list (&s->attribute_names, print_attrname, (void *) NULL);
}

/*
 * SATMPD ATTRIBUTE MAPPINGS INITIALIZE/DESTROY/PRINT FUNCTIONS
 */

static void
destroy_map (void *ptr)
{
	struct map *m = (struct map *) ptr;

	free (m->source);
	free (m->dest);
	free (m);
}

struct map *
initialize_map (const char *source, const char *dest)
{
	struct map *m;

	m = (struct map *) malloc (sizeof (*m));
	if (m == NULL)
		return (m);

	m->source = (char *) malloc (strlen (source) + 1);
	if (m->source == NULL)
	{
		free (m);
		return (NULL);
	}
	strcpy (m->source, source);

	m->dest = (char *) malloc (strlen (dest) + 1);
	if (m->dest == NULL)
	{
		free (m->source);
		free (m);
		return (NULL);
	}
	strcpy (m->dest, dest);
	iniref (m, destroy_map);

	return (m);
}

/* ARGSUSED */
static void
print_map (void *ptr, void *args)
{
	struct map *m = (struct map *) ptr;

	debug_print ("print_map (%s, %s)\n", m->source, m->dest);
}

static void
destroy_map_domain (void *ptr)
{
	struct map_domain *md = (struct map_domain *) ptr;

	destroy_list (&md->remote_map);
	destroy_list (&md->local_map);
	free (md->domain);
	free (md);
}

struct map_domain *
initialize_map_domain (const char *domain)
{
	struct map_domain *md;

	md = (struct map_domain *) malloc (sizeof (*md));
	if (md == NULL)
		return (md);
	md->domain = (char *) malloc (strlen (domain) + 1);
	if (md->domain == NULL)
	{
		free (md);
		return (NULL);
	}
	strcpy (md->domain, domain);
	initialize_list (&md->remote_map);
	initialize_list (&md->local_map);
	iniref (md, destroy_map_domain);
	return (md);
}

static void
print_map_domain (void *ptr, void *args)
{
	struct map_domain *md = (struct map_domain *) ptr;

	debug_print ("print_map_domain (%s)\n", md->domain);
	perform_on_list (&md->remote_map, print_map, args);
	perform_on_list (&md->local_map, print_map, args);
}

static void
destroy_map_head (void *ptr)
{
	struct map_head *mh = (struct map_head *) ptr;

	destroy_list (&mh->domains);
	free (mh);
}

struct map_head *
initialize_map_head (u_short attrid)
{
	struct map_head *mh;

	mh = (struct map_head *) malloc (sizeof (*mh));
	if (mh == (struct map_head *) NULL)
		return (mh);
	mh->attrid = attrid;
	initialize_list (&mh->domains);
	iniref (mh, destroy_map_head);
	return (mh);
}

void
print_map_head (void *ptr, void *args)
{
	struct map_head *mh = (struct map_head *) ptr;

	debug_print ("print_map_head (%hu)\n", mh->attrid);
	perform_on_list (&mh->domains, print_map_domain, args);
}
