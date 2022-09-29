/*
** init.c
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <ns_api.h>
#include <ns_daemon.h>

#include "lber.h"

#ifdef _SSL
#include <bsafe.h>
#include <ssl.h>
#endif

#include "ldap.h"
#include "lookup.h"

#define LDAP_CONFIG_PATH	"/var/ns/ldap.conf"
#define LDAP_MAX_LINE		1024

#define NUM_COLS		5

#define SKIPSPC( l, p ) for(p = (l); ((*(p)) != '\0')&&(isspace(*(p))); (p)++)
#define SKIPALP( l, p ) for(p = (l); ((*(p)) != '\0')&&(!isspace(*(p))); (p)++)
#define SKIPALPANUM( l, p ) for(p = (l); ((*(p)) != '\0')&&(isalnum(*(p))); (p)++)

/* 06/99 gomez@engr.sgi.com
 * FIND_CLOSE_CHAR() && COPY_UP_TO_CLOSE() added to support regsub
 */
/* Finds the matching close for a given char: i.e. o='('==>c=')' */
#define FIND_CLOSE_CHAR( o, c )\
	switch (o) {\
	case '(':\
		c = ')';\
		break;\
	case '{':\
		c = '}';\
		break;\
	case '[':\
		c = ']';\
		break;\
	case '<':\
		c = '>';\
		break;\
	default:\
		c = o;\
}

/* Copies string in p to d till it finds the closing character 'c'.
 * turns \\ into \ and \c into c in the destination string.
 */
#define COPY_UP_TO_CLOSE(p, d, c)\
	for (; *p != '\0'; p++)\
		if ( (*p == '\\') && ((*(p+1) == '\\')||(*(p+1) == c )) )\
			*d++ = *(++p);\
		else if( *p != c)\
			*d++ =*p;\
		else if ( *p == c){\
			*d = '\0';\
			break;\
		}


extern _ldm_t_ptr _domain_root;

#ifdef _SSL
extern int	_ldap_security;
extern int	_ldap_cipher;
#endif






_lsrv_t_ptr alloc_server(void);

_ldm_t_ptr new_domain(char *name)
{
	int		i;
	_ldm_t_ptr	d, ds, ds_s;

	if ((d = nsd_malloc(sizeof(_ldm_t))) == NULL) {
		nsd_logprintf(1, "ldap: malloc failed\n");
		return NULL;
	}

	if (name && *name) 
		if ((d->name = nsd_strdup(name)) == NULL) {
			nsd_logprintf(1, "ldap: malloc failed\n");
			free(d);
			return NULL;
		}
	else
		d->name = (char *) 0;

	d->map = (_lmap_t_ptr) 0;
	d->sls = (_lsrv_t_ptr) 0;
	d->pos = (_lsrv_t_ptr) 0;
	d->next = (_ldm_t_ptr) 0;

	/* Init regsub related fields */
	d->regsub_ndx = 0;
	for ( i = 0; i < MAX_NUM_REGSUB; i++ )
	{

	    d->att_type[ i ] = NULL;
	    d->sub[ i ] = NULL;

	}

	return d;
}

int append_domain(_ldm_t_ptr d)
{
	_ldm_t_ptr	ds, ds_s;

	if (! _domain_root) 
		_domain_root = d;
	else {
		for (ds = _domain_root; ds != NULL; ds = ds->next) ds_s = ds;
		ds_s->next = d;
	}

	return 0;
}

char *get_option(char *p)
{
	char *q;

	SKIPSPC(p, p);
	if (q = strchr(p, '"')) {
		p = q + 1;
		if (! (q = strchr(p, '"'))) return NULL;
		*q = (char) 0;
	} else {
		SKIPALP(p, q);
		*q = (char) 0;
	}

	return p;
}

_lsrv_t_ptr alloc_server()
{
	_lsrv_t_ptr	sv;

	if ((sv = nsd_malloc(sizeof(_lsrv_t))) == NULL)
		return NULL;

	sv->fd 		= -1;
	sv->version	= 3;
	sv->status 	= SRV_UNBOUND;
	sv->req 	= (_lrqst_t_ptr) 0;
	sv->flags 	= 0;
	sv->port 	= LDAP_PORT;
	sv->binddn	= (char *) 0;
	sv->bindpwd 	= (char *) 0;
	sv->base	= (char *) 0;
	sv->name 	= (char *) 0;
	sv->addr 	= (char *) 0;
	sv->domain 	= (char *) 0;
	sv->scope	= LDAP_SCOPE_SUBTREE;
	sv->parent 	= (_ldm_t_ptr) 0;
	sv->ldap 	= (LDAP *) 0;
	sv->to_req 	= (_lrqst_t_ptr) 0;
	sv->bindid	= 0;
	sv->time 	= 0;
	sv->next 	= (_lsrv_t_ptr) 0;

	return sv;
}

int add_server(char *ln, _ldm_t_ptr dom, FILE *file)
{
	char		*colon, *host, *end, *line, line_b[LDAP_MAX_LINE];
	_lsrv_t_ptr	d, d_s;
	_lsrv_t_ptr	sv;
	struct hostent  *he;

	if ((sv = alloc_server()) == NULL) 
		return -1;

	SKIPSPC(ln, ln);
	SKIPALP(ln, end);
	if ((host = nsd_strdup(ln)) == NULL) goto _as_err;

	sv->parent = dom;

	while (1) {
		fgets(line_b, LDAP_MAX_LINE, file);
		line = line_b;

		if (*line == '#' || *line == ';') continue; 
	 	if (strcmp(line, "\n") == 0) break;

		SKIPALP(line, end);
		*end = (char) 0;

		if ((strcasecmp(line, "binddn")) == 0) {
			if ((line = get_option(end + 1)) == NULL) 
				continue;
			if ((sv->binddn = nsd_strdup(line)) == NULL) 
				goto _as_err;
		} else if ((strcasecmp(line, "bindpwd")) == 0) {
			if ((line = get_option(end + 1)) == NULL) 
				continue;
			if ((sv->bindpwd = nsd_strdup(line)) == NULL)
				goto _as_err;
		} else if ((strcasecmp(line, "base")) == 0) {
			if ((line = get_option(end + 1)) == NULL)
				continue;
			if ((sv->base = nsd_strdup(line)) == NULL)
				goto _as_err;
		} else if ((strcasecmp(line, "scope")) == 0) {
			if ((line = get_option(end + 1)) == NULL)
				continue;

			if (strcasecmp(line, "onelevel") == 0) 
				sv->scope = LDAP_SCOPE_ONELEVEL;
			else if (strcasecmp(line, "subtree") == 0)
				sv->scope = LDAP_SCOPE_SUBTREE;
			else if (strcasecmp(line, "sbase") == 0) 
				sv->scope = LDAP_SCOPE_BASE;
		} else if ((strcasecmp(line, "version")) == 0) {
			if ((line = get_option(end + 1)) == NULL)
				continue;
			if ((sv->version = atoi(nsd_strdup(line))) == NULL)
				goto _as_err;
		}
	}

	if (! host) goto _as_err;

	if (colon = strchr(host, ':')) {
		sv->port = atoi(colon + 1);
		*colon = (char) 0;
	}

	if ((sv->name = nsd_strdup(host)) == NULL) goto _as_err;

	if (isdigit(*host))
		if ((sv->addr = nsd_strdup(host)) == NULL) goto _as_err;
	else
		if (he = gethostbyname(host))
			if ((sv->addr = strdup((char *)inet_ntoa(*((struct 
				in_addr *)(*he->h_addr_list))))) == NULL)
				goto _as_err;

	free(host);
			
	if (dom->name)
		sv->domain = strdup(dom->name);
	else sv->domain = (char *) 0;

	if (! dom->sls) {
		dom->sls = sv;
	} else {
		d = dom->sls;
		do {
			d_s = d;
			d = d->next;
		} while (d != dom->sls);
		d_s->next = sv;
	}

	sv->next = dom->sls;

	return 0;

_as_err:
	nsd_logprintf(1, "ldap: failed malloc\n");

	if (sv->name)		free(sv->name);
	if (sv->addr)		free(sv->addr);
	if (sv->base)		free(sv->base);
	if (sv->binddn)		free(sv->binddn);
	if (sv->bindpwd)	free(sv->bindpwd);
	if (sv->domain)		free(sv->domain);
				free(sv);
	return -1;
}

int char_count(char *s, char t)
{
	int n = 0;

	while (s = strchr(s, t)) {
		n++;
		s++;
	}

	return n;
}

char *get_string(char *s)
{
	char *e;

	if (*s == '"') {
		e = strchr(s + 1, '"');
		*e = '\0';
		return s + 1;
	} else {
		SKIPALP(s, e);
		*e = '\0';
		return s;
	}
}

void free_format(_lfmt_t_ptr f)
{
	int		i;
	_litm_t_ptr	itm, itm_s;

	if (f->start) free(f->start);
	if (f->end)	free(f->end);

	for (i=0; f->attr[i]; i++) 
		free(f->attr[i]);

	itm = f->item;
	while (itm) {
		itm_s = itm->next;
		if (itm->name)	free(itm->name);
		if (itm->sep)	free(itm->sep);
		if (itm->alt)	free(itm->alt);
		free(itm);
		itm = itm_s;
	}

	free(f);
}

_lfmt_t_ptr add_format(char *line, int *colbn)
{
	char alpha[] = "-_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	char		**cols, save, *end, *sp, sp_b[128];
	_lfmt_t_ptr	f;
	_litm_t_ptr	item, itm;
	int		keep_going = 1;
	int		skip, i, coln = 0, start = 0;

	if ((f = (_lfmt_t_ptr) nsd_malloc(sizeof(_lfmt_t))) == NULL) 
		return NULL;

	f->start = (char *) 0;
	f->end = (char *) 0;
	f->flags = 0;
	f->attr = (char **) 0;
	f->item = (_litm_t_ptr) 0;

	*colbn = 1;
	if ((cols = (char **) nsd_malloc(*colbn * NUM_COLS * sizeof(char *))) 
		== NULL) goto _af_error;
	
	end = line;
	while (*end != '(' && ! isalpha(*end)) { start = 1; end++; }
	if (start) {
		save = *end;
		*end = '\0';
		if ((f->start = nsd_strdup(line)) == NULL) goto _af_error;
		*end = save;
		line = end;
	} 

	while (keep_going) {
		if ((item = (_litm_t_ptr) nsd_malloc(sizeof(_litm_t))) == NULL)
			goto _af_error;
		item->flags = 0;
		item->alt = (char *) 0;
		item->sep = (char *) 0;
		item->name = (char *) 0;
		item->next = (_litm_t_ptr) 0;
	
		if (*line == '(') {
			item->flags |= ITEM_FLAG_PLUS;
			line++;
		}
		end = line;
		while (isalpha(*end)) end++;
		save = *end;
		*end = '\0';
		if (! line || ! *line) goto _af_error;
		if ((item->name = nsd_strdup(line)) == NULL) goto _af_error;

		for (skip = 0, i = 0; i < coln; i++)
			if (strcasecmp(cols[i], line) == NULL) skip = 1;

		if (! skip) {
			if (coln + 1 > *colbn * NUM_COLS)
			if ((cols = (char **) nsd_realloc(cols, ++(*colbn) 
				* NUM_COLS * sizeof(char *))) == NULL)
				goto _af_error;
			if ((cols[coln] = (char *) nsd_strdup(line)) == NULL)
				goto _af_error;
			coln++;
		}

		*end = save;
		line = end;

		if (*line == '|') {
			line++;
			end = line;
			while (isalpha(*end)) end++;
			save = *end;
			*end = '\0';
			if (! line || ! *line) goto _af_error;
			if ((item->alt = nsd_strdup(line)) == NULL)
				goto _af_error;
			*end = save;
			line = end;
		}

		if (item->flags & ITEM_FLAG_PLUS) {
			if (! *line) goto _af_error;
			if ((end = strchr(line, ')')) == NULL) goto _af_error;
			*end = '\0';
			item->sep = strdup(line);
			line = end + 2;
		} else {
			if (strpbrk(line, alpha) != NULL) {
				sp = sp_b;
				while (*end && ! isalpha(*end) && *end != '(') {
					if (*end == '\\') {
						*sp++ = '\t';
						end += 2;
					} else *sp++ = *end++;
				}
				*sp = '\0';
				if ((item->sep = nsd_strdup(sp_b)) == NULL)
					goto _af_error;
				line = end;
			}
		}

		if (! f->item)
			f->item = item;
		else {
			for (itm = f->item; itm->next; itm = itm->next) {
				if (strcasecmp(itm->name, item->name) == NULL)
					item->flags |= ITEM_FLAG_SKIP;
			}
			itm->next = item;
		}

		if (strpbrk(line, alpha) == NULL) {
			if (*line) 
				if ((f->end = nsd_strdup(line)) == NULL)
					goto _af_error;
			keep_going = 0;
		}
	}

        if (coln + 1 > *colbn * NUM_COLS)
                if ((cols = (char **) nsd_realloc(cols, ++(*colbn) * NUM_COLS 
			* sizeof(char *))) == NULL) goto _af_error;
        cols[coln] = (char *) 0;

	f->attr = cols;

	return f;

_af_error:
	free_format(f);

	return NULL;	
}

int add_prefix_attr(char **f_c, char **p_c, int colbn)
{
	int	add, j, k;
	char 	*p, *f;

	j = 0;
	while (p = p_c[j]) {
		add = 1;
		k = 0;
		while (f = f_c[k]) {
			if (strcasecmp(p, f) == NULL) add = 0;
			k++;
		}
		if (add) {
			if (k + 1  > colbn * NUM_COLS) 
				f_c = (char **) nsd_realloc(f_c, 
					++colbn * NUM_COLS * sizeof(char *));
			f_c[k] = p;
			f_c[k+1] = (char *) 0;
		}
		j++;
	}

	return 0;
}

/* Parses stuff like {Regexp}{Substitution} and places Regexp -> match
 * and Substitution -> sub
 * Assumes:
 *	*match = NULL;
 *	*sub = NULL;
 */

char * parse_regsub(char *line, char **match, char **sub) 
{

	char	o, c;	/* Open, Close char */
	char	*p;
	char	*d;

	if (!line || !match || !sub || !(*line))
		return NULL;

	/* Closing char c must be escaped using \c and '\' 
	 * must be escaped \\.
	 */

	o = *line;
	FIND_CLOSE_CHAR( o, c );

	p = line + 1;	/* Source string */
	d = *match;	/* Destination str */
	COPY_UP_TO_CLOSE(p, d, c);
	    
	if ( *p++ != c )
	    return NULL;
    
	o = *p;
	FIND_CLOSE_CHAR( o, c );

	p++;
	d = *sub;
	COPY_UP_TO_CLOSE(p, d, c);
	    
	if ( *p++ != c )
	    return NULL;

	return ++p;	/* Points to just after the closing char */

}/* char * parse_regsub() */

int add_regsub(char *line, _ldm_t_ptr d)
{

	char	*end;
	char	*att_type = NULL;
	char	*match = NULL;
	char	*sub = NULL;
	char	cs;
	regex_t reg;

	if (d->regsub_ndx >= MAX_NUM_REGSUB) {
	    
		/* Out of regsub entries for this domain */
		nsd_logprintf(0, "ldap: too many regsubs in domain %s\n",
			      (d->name ? d->name:"(NULL)"));
		return -1;

	}
	
	/* Parse the rest of the line that must look as follows:
	 * [SPC]*Attribute_Type{Regexp}{Substitution}
	 */

	nsd_logprintf(2, "ldap: add_regsub() line=%s\n",
		      (line?line:"(NULL)"));
		      
	SKIPSPC(line, line);
	SKIPALPANUM(line, end);	/* Skip attribute name */

	cs = *end;
	*end = '\0';

	nsd_logprintf(2, "ldap: after skip add_regsub() line=%s, char=%c\n",
		      (line?line:"(NULL)"), cs);


	if (!(*line))
		goto _rs_error;

	if ((att_type = nsd_strdup(line)) == NULL)
		goto _rs_error;
	
	*end = cs;

	/* Parses {Match}{Substitution} string */

	
	if( ((match = nsd_strdup(end)) == NULL)\
	    || 
	    ((sub = nsd_strdup(end)) == NULL) )
		goto _rs_error;

	if (parse_regsub(end, &match, &sub) == NULL) {
		nsd_logprintf(0, "ldap: parse_regsub() failed for %s\n",
			      (end?end:"(NULL)"));
		goto _rs_error;
	}

	if (regcomp( &reg, match, REG_EXTENDED ) != 0 ){
		nsd_logprintf(0, "ldap: regcomp failed for %s\n",
			      (match?match:"(NULL)"));
		goto _rs_error;
	}

	/* Here we have everything so we just place it in the domain */

	d->att_type[d->regsub_ndx] = att_type;
	d->match[d->regsub_ndx] = reg;
	d->sub[d->regsub_ndx] = sub;

	d->regsub_ndx++;

	free(match);

	return 0;

_rs_error:
	nsd_logprintf(0, "ldap: parse error in regsub\n");
	if (att_type) free(att_type);
	if (match) free(match);
	if (sub) free(sub);

	return -1;

}/* int add_regsub() */

int add_table(_ldm_t_ptr d, char *line, FILE *f)
{
	char 		**cols;
	int		single_line = 0, colbn, dummy;
	_lmap_t_ptr	map, m;
	_lpair_t_ptr	pair, pair_s;
	char		*end, line_b[LDAP_MAX_LINE];

	if ((map = (_lmap_t_ptr) nsd_malloc(sizeof(_lmap_t))) == NULL)
		return -1;

	map->next 		= (_lmap_t_ptr) 0;
	map->name		= (char *) 0;
	map->format 		= (_lfmt_t_ptr) 0;
	map->filter_lookup	= (char *) 0;
	map->filter_list	= (char *) 0;
	map->require 		= (_lpair_t_ptr) 0;
	map->def 		= (char *) 0;
	map->prefix		= (_lfmt_t_ptr) 0;
	map->lookup_num		= 0;
	map->lookup_len		= 0;
	map->list_num		= 0;
	map->list_len		= 0;
	
	SKIPSPC(line + 5, line);
	SKIPALP(line, end);
	*end = '\0';

	if (! *line) goto _at_error;
	if ((map->name = nsd_strdup(line)) == NULL) goto _at_error;
 
	while (fgets(line_b, LDAP_MAX_LINE, f) && line_b) {

		line = line_b;

		if (*line == '#' || *line == ';') continue;
	
		SKIPSPC(line, line);

		if (! *line) break;

		SKIPALP(line, end);
		SKIPSPC(end, end);

		if (strncasecmp(line, "FILTER_LOOKUP", 13) == NULL) {

			if ((line = get_string(end)) == NULL) goto _at_error;

			if ((map->filter_lookup = nsd_strdup(line)) == NULL) 
				goto _at_error;

		} else if (strncasecmp(line, "FILTER_LIST", 11) == NULL) {

			if ((line = get_string(end)) == NULL) goto _at_error;

			if ((map->filter_list = nsd_strdup(line)) == NULL)
				goto _at_error;

		} else if (strncasecmp(line, "SINGLE_LINE", 6) == NULL) {

			if ((line = get_string(end)) == NULL) goto _at_error;
		
			if ((map->prefix = add_format(line, &dummy)) == NULL)
				goto _at_error;

		} else if (strncasecmp(line, "FORMAT", 6) == NULL) {

			if ((line = get_string(end)) == NULL) goto _at_error;

			if ((map->format = add_format(line, &colbn)) == NULL) 
				goto _at_error;

		} else if (strncasecmp(line, "REQUIRE", 7) == NULL) {
	
			if ((line = get_string(end)) == NULL) goto _at_error;

			if ((pair = (_lpair_t_ptr) nsd_malloc(sizeof(_lpair_t)))
				== NULL) goto _at_error;
			if ((pair->key = nsd_strdup(line)) == NULL) 
				goto _at_error;
			pair->next = (_lpair_t_ptr) 0;

			if (! map->require) map->require = pair;
			else {
				for (pair_s = map->require; pair_s->next;
					pair_s = pair_s->next) ;
				pair_s->next = pair;
			}

		} else if (strncasecmp(line, "DEFAULT", 7) == NULL) {
	
			if ((line = get_string(end)) == NULL) goto _at_error;

			if ((map->def = nsd_strdup(line)) == NULL) 
				goto _at_error;
			
		}
	}

	if (! map->format || (! map->filter_lookup && ! map->filter_list)) 
		goto _at_error;

	if (map->prefix) add_prefix_attr(map->format->attr, 
		map->prefix->attr, colbn);

	if (single_line) map->format->flags |= FORMAT_FLAG_SINGLE;

	if (map->filter_lookup) {
		map->lookup_num = char_count(map->filter_lookup, '%');
		map->lookup_len = strlen(map->filter_lookup);
	}

	if (map->filter_list) {
		map->list_num = char_count(map->filter_list, '%');
		map->list_len = strlen(map->filter_list);
	}

	if (! d->map) 
		d->map = map;
	else {
		for (m = d->map; m->next; m = m->next) ;
		m->next = map;
	}

	return 0;

_at_error:
	if (map->name) 		free(map->name);
	if (map->filter_lookup) free(map->filter_lookup);
	if (map->filter_list)	free(map->filter_list);
	if (map->def)		free(map->def);

	pair = map->require;
	while (pair) {
		pair_s = pair->next;
		if (pair->key) free(pair->key);
		free(pair);
		pair = pair_s;
	}

	if (map->format) 	free_format(map->format);

	free(map);

	return -1;
}

int parse_config(void)
{
	FILE 		*file;
	_ldm_t_ptr	domain;
	_lmap_t_ptr	map, ma, mb;
	char		*end, *line, line_b[LDAP_MAX_LINE];

	if ((file = fopen(LDAP_CONFIG_PATH, "r")) == NULL) {
		nsd_logprintf(0, "ldap: config file not found\n");
		return -1;
	}

	domain = (_ldm_t_ptr) 0;

	while (fgets(line_b, LDAP_MAX_LINE, file) && line_b) {

		line = line_b;
		
		if (*line == '\n' || *line == '#' || *line == ';') continue;

#ifdef _SSL
		if (strncmp(line, "security", 8) == NULL) {
			SKIPSPC(line + 8, line);
			SKIPALP(line, end);
			*end = '\0';
			if (strcasecmp(line, "SSL") == NULL)
				_ldap_security = LDAP_SECURITY_SSL;
			continue;
		}

		if (strncmp(line, "cipher", 6) == NULL) {
			SKIPSPC(line + 6, line);
			SKIPALP(line, end);
			*end = '\0';

			if (strcasecmp(line, "RSA_NULL_MD5") == NULL)
				_ldap_cipher = SSL3_RSA_NULL_MD5;
			else if (strcasecmp(line, "RSA_RC4_40_MD5") == NULL)
				_ldap_cipher = SSL3_RSA_RC4_40_MD5;
			else if (strcasecmp(line, "RSA_RC4_MD5") == NULL)
				_ldap_cipher = SSL3_RSA_RC4_MD5;
			else if (strcasecmp(line, "RSA_RC2_40_MD5") == NULL)
				_ldap_cipher = SSL3_RSA_RC2_40_MD5;
			else {
				nsd_logprintf(0, "ldap: bad cipher\n");
				goto _pc_err;
			}

			continue;
		}
#endif

		if (strncmp(line, "domain", 6) == NULL) {
			if (domain && domain->sls) append_domain(domain);
			SKIPSPC(line + 6, line);
			SKIPALP(line, end);
			*end = '\0';
			if ((domain = new_domain(line)) == NULL) goto _pc_err;
			continue;
		}
		
		if ((strncmp(line, "table", 5)) == NULL) {
			add_table(domain, line, file);
			/* XXX: Must do error check and continue here */
		}

		if ((strncmp(line, "server", 6)) == NULL) {
			if (! domain) continue;
			if ((add_server(line + 6, domain, file)) == -1) 
				goto _pc_err;
			continue;
		}

		/* 06/08/99 gomez@engr.sgi.com
		 * Added to cover RFC-2307 {crypt|md5...}AGSHgssh,.
		 * password syntax. The next few lines support the 
		 * 'regsub	Attribute_type{Match}{Substitution}'
		 * syntax in the configuration file.
		 * The occurences of '}' and '\' in 'Match' and
		 * 'Substitution' strings are escaped by using '\'. 
		 */
		if ((strncmp(line, "regsub", 6)) == NULL) {
			if (! domain) continue;
			if (add_regsub(line + 6, domain) == -1)
				goto _pc_err;
			continue;
		}
		

	} 

	fclose(file);

	if (domain && domain->sls) append_domain(domain);

	for (domain = _domain_root; domain != NULL; domain = domain->next)
		domain->pos = domain->sls;

	return 0;

_pc_err:
	nsd_logprintf(0, "ldap: parse config file error\n");
	fclose(file);

	return -1;
}
	

int init(char *map)
{
	_lsrv_t_ptr	sv;

	nsd_logprintf(2, "ldap: init called with map: %s\n", map);

	if (parse_config() == -1) return NSD_ERROR;

	return NSD_OK;
}
