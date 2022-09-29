/*
 * ldap_init.c - reads the configuration file 
 */
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

extern int h_errno;
#include <lber.h>
#include <ldap.h>

#include "ldap_api.h"
#include "ldap_dhcp.h"

#define LOGLDIF_DEFAULT "/var/dhcp/ldap_dhcp_log.ldif"

/* default configuration for LDAP server */
#define OPEN_TIMEOUT	2
#define ACCESS_TIMEOUT	2
#define ERROR_TIMEOUT	5
#define MAX_REQUESTS	3

/* configuration file strings */
#define SCHEMA "schema"
#define CONFIG "config"
#define FILES "files"
#define FILESANDLDAP "files&ldap"
#define LOGLDIF "logldif"
#define LEASEONLINEUPD "online_leases"
#define SUBNET_REFRESH "subnet_refresh"
#define RANGE_REFRESH "range_refresh"
#define LEASES "leases"
#define ENTRY "entry"
#define SEARCH "search"
#define ADD "add"
#define MODIFY "modify"
#define MODRDN "modrdn"
#define DELETE "delete"
#define COMPARE "compare"
#define SERVER "server"
#define BINDDN "binddn"
#define BINDPWD "bindpwd"
#define BASE "base"
#define SCOPE "scope"
#define ONELEVEL "onelevel"
#define SUBTREE "subtree"
#define SBASE "sbase"
#define FILTER_LOOKUP "filter_lookup"
#define FILTER_LIST "filter_list"
#define GETATTR "getattr"
#define REQUIRE "require"
#define SETATTR "setattr"

#define LDAP_MAX_LINE		1024

#define NUM_COLS		5

#define SKIPSPC( l, p ) for(p = (l); ((*(p)) != '\0')&&(isspace(*(p))); (p)++)
#define SKIPALP( l, p ) for(p = (l); ((*(p)) != '\0')&&(!isspace(*(p))); (p)++)

/* globals */
_ld_t_ptr _ld_root = (_ld_t_ptr)0;	/* root to store config information */
int ldap_config_on = 0;
int ldap_update_on = 0;
static long ldapconf_modtime = 0; /* last modification time dhcp_ldap.conf */
extern FILE *log_fopen(char *, char *);
/*
 * strips quotes if any and returns the string
 */
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

void free_getattr(_lfmt_t_ptr f)
{
    _litm_t_ptr	itm, itm_s;
    int i = 0;
    
    if (f->line) free(f->line);
    
    itm = f->item;
    while (itm) {
	itm_s = itm->next;
	if (itm->name)	free(itm->name);
	free(itm);
	itm = itm_s;
    }
    for (i=0; f->attr[i]; i++) 
	free(f->attr[i]);
    
    free(f);
}

/*
 *
 */
void free_server(_lsrv_t_ptr sv)
{
    /* may not be complete */
    if (sv->name)		free(sv->name);
    if (sv->addr)		free(sv->addr);
    if (sv->base)		free(sv->base);
    if (sv->binddn)		free(sv->binddn);
    if (sv->bindpwd)	free(sv->bindpwd);
    if (sv->schema)		free(sv->schema);
    free(sv);
}
/*
 *
 */
void free_ent(_lent_t_ptr ent)
{
    _lpair_t_ptr	pair, pair_s;

    /* may not be complete */
    if (ent->name) 		free(ent->name);
    if (ent->filter_lookup) free(ent->filter_lookup);
    if (ent->filter_list)	free(ent->filter_list);
    
    pair = ent->require;
    while (pair) {
	pair_s = pair->next;
	if (pair->key) free(pair->key);
	free(pair);
	pair = pair_s;
    }
    
    if (ent->format) 	free_getattr(ent->format);
    
    free(ent);
}

/* 
 *
 */
void free_schema(_ld_t_ptr ldp)
{
    if (ldp) {
	if (ldp->schema)
	    free(ldp->schema);
	if (ldp->sls)
	    free_server(ldp->sls);
	if (ldp->ent)
	    free_ent(ldp->ent);
    }
    ldp = NULL;
}

void
free_ld_root(void) 
{
    ldap_update_on = 0;
    ldap_config_on = 0;
    ldapconf_modtime = 0;
    if (_ld_root) free_schema(_ld_root);
    _ld_root = NULL;
}

void 
ldhcp_result_free(void *result)	
{
    _arg_ld_t_ptr res, restmp, ressave;
    
    res = (_arg_ld_t_ptr)result;
    for (restmp = res; restmp; restmp = ressave ) {
	ressave = restmp->next;
	ldhcp_arg_ld_free(restmp);
    }
}

/*
 *
 */
_ld_t_ptr new_schema(char *schema, int *lineno, FILE* f)
{
    _ld_t_ptr	ldp;
    int		tmp;
    char	*end, *line, line_b[LDAP_MAX_LINE];

    if ((ldp = malloc(sizeof(_ld_t))) == NULL) {
	syslog(LOG_ERR, "ldap: Out of memory");
	return NULL;
    }

    if ((ldp->schema = strdup(schema)) == NULL) {
	syslog(LOG_ERR, "ldap: Out of memory");
	return NULL;
    }

    ldp->config_ldap = 1;
    ldp->subnet_refresh = 0;
    ldp->range_refresh = 0;
    ldp->leases_store = 0;
    ldp->open_timeout = OPEN_TIMEOUT;
    ldp->access_timeout = ACCESS_TIMEOUT;
    ldp->error_timeout = ERROR_TIMEOUT;
    ldp->max_requests = MAX_REQUESTS;
    ldp->f_tries = 0;
    ldp->f_free = ldhcp_result_free;
    ldp->f_result = (_arg_ld_t_ptr)0;
    ldp->sls = 0;
    ldp->ent = 0;
    ldp->ldiflog = NULL;
    ldp->leases_online = LDAP_OP_ONLINE;

    /* get configuration information */
    while (fgets(line_b, LDAP_MAX_LINE, f) && line_b) {
	
	line = line_b;
	(*lineno)++;
	
	if (*line == '#' || *line == ';') continue;
	
	SKIPSPC(line, line);

	if (! *line) break;

	SKIPALP(line, end);
	SKIPSPC(end, end);

	if (strncasecmp(line, CONFIG, 6) == NULL) {
	    if ((line = get_string(end)) == NULL)
		goto _at_error;
	    if (strncasecmp(line, FILESANDLDAP, 10) == NULL)
		ldp->config_ldap = 1;
	    else if (strncasecmp(line, FILES, 5) == NULL)
		ldp->config_ldap = 0;
	    else 
		goto _at_error;
	}
	else if (strncasecmp(line, SUBNET_REFRESH, 14) == NULL) {
	    if ((line = get_string(end)) == NULL)
		goto _at_error;
	    if (sscanf(line, "%d", &tmp) != 1) 
		goto _at_error;
	    ldp->subnet_refresh = tmp;
	}
	else if (strncasecmp(line, RANGE_REFRESH, 13) == NULL) {
	    if ((line = get_string(end)) == NULL)
		goto _at_error;
	    if (sscanf(line, "%d", &tmp) != 1) 
		goto _at_error;
	    ldp->range_refresh = tmp;
	}
	else if (strncasecmp(line, LEASES, 6) == NULL) {
	    if ((line = get_string(end)) == NULL)
		goto _at_error;
	    if (sscanf(line, "%d", &tmp) != 1) 
		goto _at_error;
	    ldp->leases_store = tmp;
	}
	else if (strncasecmp(line, LOGLDIF, 7) == NULL) {
	    if ( ((line = get_string(end)) == NULL) || (*line == NULL) )
		/* open default ldif log file for append  */
		ldp->ldiflog = log_fopen(LOGLDIF_DEFAULT, "a");
	    else {
		ldp->ldiflog = log_fopen(line, "a");
		if (ldp->ldiflog == NULL) 
		    ldp->ldiflog = log_fopen(LOGLDIF_DEFAULT, "a");
	    }
	} else if (strncasecmp(line, LEASEONLINEUPD, 13) == NULL) {
	    if ((line = get_string(end)) == NULL)
		goto _at_error;
	    if (sscanf(line, "%d", &tmp) != 1) 
		goto _at_error;
	    if (tmp == 0)
		ldp->leases_online = 0;
	}
	else 
	    goto _at_error;
    }
    if (ldp->leases_store)
	ldap_update_on = 1;
    if (ldp->config_ldap == FILES_AND_LDAP)
	ldap_config_on = 1;
    return ldp;

 _at_error:
    if (ldp)
	free_schema(ldp);
    return 0;
    
}

_lsrv_t_ptr alloc_server(void)
{
	_lsrv_t_ptr	sv;

	if ((sv = malloc(sizeof(_lsrv_t))) == NULL)
		return NULL;

	sv->fd 		= -1;
	sv->status 	= SRV_UNBOUND;
	sv->req 	= (_lrqst_t_ptr) 0;
	sv->flags 	= 0;
	sv->port 	= LDAP_PORT;
	sv->binddn	= (char *) 0;
	sv->bindpwd 	= (char *) 0;
	sv->base	= (char *) 0;
	sv->name 	= (char *) 0;
	sv->addr 	= (char *) 0;
	sv->schema 	= (char *) 0;
	sv->scope	= LDAP_SCOPE_SUBTREE;
	sv->parent 	= (_ld_t_ptr) 0;
	sv->ldap 	= (LDAP *) 0;
	sv->to_req 	= (_lrqst_t_ptr) 0;
	sv->bindid	= 0;
	sv->time 	= 0;
	sv->next 	= (_lsrv_t_ptr) 0;

	return sv;
}

/*
 * add_server
 */
int add_server(_ld_t_ptr ld, char *ln, int *lineno, FILE *file)
{
    char	*colon, *host, *end, *line, line_b[LDAP_MAX_LINE];
    _lsrv_t_ptr	d, d_s;
    _lsrv_t_ptr	sv;
    struct hostent  *he;
    
    if ((sv = alloc_server()) == NULL) 
	return -1;
    
    SKIPSPC(ln, ln);
    SKIPALP(ln, end);
    *end = '\0';
    if ((host = strdup(ln)) == NULL) goto _as_err;
    
    sv->parent = ld;

    while (fgets(line_b, LDAP_MAX_LINE, file) && line_b[0]) {

	line = line_b;
	(*lineno)++;
	
	if (*line == '#' || *line == ';') continue; 
	if (strcmp(line, "\n") == 0) break;
	
	SKIPALP(line, end);
	*end = (char) 0;
	
	if ((strcasecmp(line, BINDDN)) == 0) {
	    if ((line = get_option(end + 1)) == NULL) 
		continue;
	    if ((sv->binddn = strdup(line)) == NULL) 
		goto _as_err;
	} else if ((strcasecmp(line, BINDPWD)) == 0) {
	    if ((line = get_option(end + 1)) == NULL) 
		continue;
	    if ((sv->bindpwd = strdup(line)) == NULL)
		goto _as_err;
	} else if ((strcasecmp(line, BASE)) == 0) {
	    if ((line = get_option(end + 1)) == NULL)
		continue;
	    if ((sv->base = strdup(line)) == NULL)
		goto _as_err;
	} else if ((strcasecmp(line, SCOPE)) == 0) {
	    if ((line = get_option(end + 1)) == NULL)
		continue;
	    
	    if (strcasecmp(line, ONELEVEL) == 0) 
		sv->scope = LDAP_SCOPE_ONELEVEL;
	    else if (strcasecmp(line, SUBTREE) == 0)
		sv->scope = LDAP_SCOPE_SUBTREE;
	    else if (strcasecmp(line, SBASE) == 0) 
		sv->scope = LDAP_SCOPE_BASE;
	    else
		goto _as_err;
	}
    }
    
    if (! host) goto _as_err;
    
    if (colon = strchr(host, ':')) {
	sv->port = atoi(colon + 1);
	*colon = (char) 0;
    }
    
    if ((sv->name = strdup(host)) == NULL) goto _as_err;
    
    if ((he = gethostbyname(host)) == NULL) {
	syslog(LOG_ERR, 
	       "ldap: could not resolve servername: %s\n", 
	       host);
	goto _as_err2;
    }

    if ((sv->addr = strdup((char *)inet_ntoa(*((struct in_addr *)
					       (*he->h_addr_list))))) == NULL)
	goto _as_err;
    
    free(host);
    
    if (ld->schema)
	sv->schema = strdup(ld->schema);
    else sv->schema = (char *) 0;
    
    if (! ld->sls) {
	ld->sls = sv;
    } else {
	d = ld->sls;
	do {
	    d_s = d;
	    d = d->next;
	} while (d != ld->sls);
	d_s->next = sv;
    }
    
    sv->next = ld->sls; 

    return 0;

 _as_err:
    syslog(LOG_ERR, "ldap: failed malloc\n");

_as_err2:
    if (sv->name)		free(sv->name);
    if (sv->addr)		free(sv->addr);
    if (sv->base)		free(sv->base);
    if (sv->binddn)		free(sv->binddn);
    if (sv->bindpwd)	free(sv->bindpwd);
    if (sv->schema)		free(sv->schema);
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
				f_c = (char **) realloc(f_c, 
					++colbn * NUM_COLS * sizeof(char *));
			f_c[k] = p;
			f_c[k+1] = (char *) 0;
		}
		j++;
	}

	return 0;
}


/*
 * add_format
 */
_lfmt_t_ptr add_getattr(char *line)
{
    char sepstring[] = ":,\t ";
    int numattrs, i;
    char **aattr;
    
    char *token,  *equaltype;
    _lfmt_t_ptr	f;
    _litm_t_ptr	item, itm;
    
    numattrs = 0;
    if ((f = (_lfmt_t_ptr) malloc(sizeof(_lfmt_t))) == NULL) 
	return NULL;
    
    f->line = (char *) 0;
    f->item = (_litm_t_ptr) 0;
    f->attr = (char**)0;
    
    if ((f->line = strdup(line)) == NULL) 
	return 0;
    
    token = strtok(line, sepstring);
    
    while (token && (*token)) {
	
	
	if ((item = (_litm_t_ptr) malloc(sizeof(_litm_t))) == NULL)
	    goto _af_error;
	item->type = 0;
	if ((equaltype = strchr(token, '=')) != NULL) {
	    *equaltype = NULL;
	    if (tolower(*(equaltype + 1)) == 'b')
		item->type = ITM_BERVAL;
	}
	item->name = (char *) strdup(token);
	if (item->name == NULL)
	    goto _af_error;
	item->next = (_litm_t_ptr) 0;
	numattrs++;
	if (! f->item)
	    f->item = item;
	else {
	    for (itm = f->item; itm->next; itm = itm->next) {
		;
	    }
	    itm->next = item;
	}
	
	token = strtok(0, sepstring);
    }
    
    if ((f->attr = (char **)malloc((numattrs+1) * sizeof(char*))) == NULL)
	goto _af_error;
    aattr = (f->attr);
    for (i = 0, itm = f->item; (i < numattrs) ; i++, itm = itm->next) {
	*aattr = strdup(itm->name);
	if (*aattr == NULL)
	    goto _af_error;
	aattr++;
    }
    *aattr = (char*)0;
    return f;
    
 _af_error:
    free_getattr(f);
    
    return NULL;	
}

/*
 * add_entry
 */
int add_entry(_ld_t_ptr ld, char *line, int *lineno, FILE *f)
{
    _lent_t_ptr	ent, e;
    _lpair_t_ptr	pair, pair_s;
    char		*end, line_b[LDAP_MAX_LINE];
    
    if ((ent = (_lent_t_ptr) malloc(sizeof(_lent_t))) == NULL)
	return -1;
    
    ent->next 		= (_lent_t_ptr) 0;
    ent->name		= (char *) 0;
    ent->format 	= (_lfmt_t_ptr) 0;
    ent->filter_lookup	= (char *) 0;
    ent->filter_list	= (char *) 0;
    ent->require 	= (_lpair_t_ptr) 0;
    ent->lookup_num	= 0;
    ent->lookup_len	= 0;
    ent->list_num	= 0;
    ent->list_len	= 0;
    ent->base_num	= 0;
    ent->base_len	= 0;
    ent->base		= (char*)0;
    ent->scope		= -1;	/* use default from server */
    
    SKIPSPC(line + 5, line);
    SKIPALP(line, end);
    *end = '\0';
    
    if (! *line) goto _at_error;

    /* look for search, modify, add, delete, modrdn, compare keywords */
    if (strncasecmp(line, SEARCH, 6) == 0) 
	ent->op = LDAP_REQ_SEARCH;
    else if (strncasecmp(line, MODIFY, 6) == 0) 
	ent->op = LDAP_REQ_MODIFY;
    else if (strncasecmp(line, ADD, 3) == 0) 
	ent->op = LDAP_REQ_ADD;
    else if (strncasecmp(line, DELETE, 6) == 0) 
	ent->op = LDAP_REQ_DELETE;
    else if (strncasecmp(line, MODRDN, 3) == 0) 
	ent->op = LDAP_REQ_MODRDN;
    else if (strncasecmp(line, COMPARE, 3) == 0) 
	ent->op = LDAP_REQ_COMPARE;
    else {
	ent->op = LDAP_REQ_SEARCH; goto no_operation;	/* assume search */
    }
    
    SKIPSPC(end + 1, end);
    line = end;
    SKIPALP(line, end);
    *end = '\0';

 no_operation:
    if (! *line) goto _at_error;
    if ((ent->name = strdup(line)) == NULL) goto _at_error;

    while (fgets(line_b, LDAP_MAX_LINE, f) && line_b) {

	line = line_b;
	(*lineno)++;

	if (*line == '#' || *line == ';') continue;
	
	SKIPSPC(line, line);

	if (! *line) break;
	
	SKIPALP(line, end);
	SKIPSPC(end, end);
	
	if (strncasecmp(line, FILTER_LOOKUP, 13) == NULL) {
	    
	    if ((line = get_string(end)) == NULL) goto _at_error;
	    
	    if ((ent->filter_lookup = strdup(line)) == NULL) 
		goto _at_error;
	    
	} else if (strncasecmp(line, FILTER_LIST, 11) == NULL) {
	    
	    if ((line = get_string(end)) == NULL) goto _at_error;
	    
	    if ((ent->filter_list = strdup(line)) == NULL)
		goto _at_error;
	    
	} else if (strncasecmp(line, GETATTR, 6) == NULL) {
	    
	    if ((line = get_string(end)) == NULL) goto _at_error;

	    if ((ent->format = add_getattr(line)) == NULL) 
		goto _at_error;
	    
	} else if (strncasecmp(line, SETATTR, 6) == NULL) {
	    
	    if ((line = get_string(end)) == NULL) goto _at_error;

	    if ((ent->format = add_getattr(line)) == NULL) 
		goto _at_error;
	} else if (strncasecmp(line, REQUIRE, 7) == NULL) {
	    /* we can have multiple require clauses per entry */
	    
	    if ((line = get_string(end)) == NULL) goto _at_error;
	    
	    if ((pair = (_lpair_t_ptr) malloc(sizeof(_lpair_t))) == NULL) 
		goto _at_error;
	    if ((pair->key = strdup(line)) == NULL) 
		goto _at_error;
	    pair->next = (_lpair_t_ptr) 0;
	    
	    if (! ent->require) ent->require = pair;
	    else {
		for (pair_s = ent->require; pair_s->next;
		     pair_s = pair_s->next) ;
		pair_s->next = pair;
	    }
	    
	} else if ((strncasecmp(line, BASE, 4)) == 0) {
	    if ((line = get_option(end)) == NULL)
		continue;
	    if ((ent->base = strdup(line)) == NULL)
		goto _at_error;
	} else if ((strncasecmp(line, SCOPE, 5)) == 0) {
	    if ((line = get_option(end)) == NULL)
		continue;
	    
	    if (strncasecmp(line, ONELEVEL, 8) == 0) 
		ent->scope = LDAP_SCOPE_ONELEVEL;
	    else if (strncasecmp(line, SUBTREE, 7) == 0)
		ent->scope = LDAP_SCOPE_SUBTREE;
 	    else if (strncasecmp(line, SBASE, 5) == 0) 
		ent->scope = LDAP_SCOPE_BASE;
	    else
		goto _at_error;
	}
    }
    
    if (((ent->op != LDAP_REQ_DELETE) && (! ent->format)) || 
	((ent->op == LDAP_REQ_SEARCH) && (! ent->filter_lookup)
	 && (! ent->filter_list))) 
	goto _at_error;
    
    if (ent->filter_lookup) {
	ent->lookup_num = char_count(ent->filter_lookup, '%');
	ent->lookup_len = strlen(ent->filter_lookup);
    }
    
    if (ent->filter_list) {
	ent->list_num = char_count(ent->filter_list, '%');
	ent->list_len = strlen(ent->filter_list);
    }
    
    if (ent->base) {
	ent->base_num = char_count(ent->base, '%');
	ent->base_len = strlen(ent->base);
    }
    
    if (! ld->ent) 
	ld->ent = ent;
    else {
	for (e = ld->ent; e->next; e = e->next) ;
	e->next = ent;
    }
    
    return 0;
    
 _at_error:
    if (ent->name) 		free(ent->name);
    if (ent->filter_lookup) free(ent->filter_lookup);
    if (ent->filter_list)	free(ent->filter_list);
    
    pair = ent->require;
    while (pair) {
	pair_s = pair->next;
	if (pair->key) free(pair->key);
	free(pair);
	pair = pair_s;
    }
    
    if (ent->format) 	free_getattr(ent->format);
    
    free(ent);
    
    return -1;
}


/*
 * returns 0 if no erorr, error -1
 *
 */


int parse_ldap_dhcp_config
(char *ldap_conf_file)
{
    struct stat	st;
    FILE        *file;
    _ld_t_ptr	ld;
    char        *end, *line, line_b[LDAP_MAX_LINE];
    int		lineno;
    
    if ((file = fopen(ldap_conf_file, "r")) == NULL) {
	syslog(LOG_ERR, "ldap: config file not found\n");
	return -1;
    }
    fstat(fileno(file), &st);
    if (st.st_mtime == ldapconf_modtime && st.st_nlink) {
	fclose(file);
	return 0;
    }
    if (ldapconf_modtime != 0) {
	ldap_update_on = 0;
	ldap_config_on = 0;
	free_ld_root();
    }
    ldapconf_modtime = st.st_mtime;
    ld = (_ld_t_ptr)0;
    lineno = 0;

    while (fgets(line_b, LDAP_MAX_LINE, file) && line_b) {
	
	line = line_b;
	lineno++;
	
	if (*line == '\n' || *line == '#' || *line == ';') 
	    continue;
	
	if (strncmp(line, SCHEMA, 6) == NULL) {
	    SKIPSPC(line + 6, line);
	    SKIPALP(line, end);
	    *end = '\0';
	    if ((ld = new_schema(line, &lineno, file)) == NULL) 
		goto _pc_err;
	    continue;
	}
	
	else if ((strncmp(line, ENTRY, 5)) == NULL) {
	    if (! ld) 
		continue;
	    if (add_entry(ld, line, &lineno, file) == -1)
		goto _pc_err;
	    continue;
	}
	
	else if ((strncmp(line, SERVER, 6)) == NULL) {
	    if (! ld) 
		continue;
	    if ((add_server(ld, line + 6, &lineno, file)) == -1) 
		goto _pc_err;
	    continue;
	}
	else 
	    syslog(LOG_ERR, "ldap: parse config file error on line %d\n", 
		   lineno);
    } 
    
    fclose(file);

    if (_ld_root)
	free_schema(_ld_root);
    _ld_root = ld;
    ld->pos = ld->sls;
    /* set alarm for next config read if needed */
    if (_ld_root->subnet_refresh > 0) {
	time_t t;
	struct tm* ptm;

	t = time(NULL);
	ptm = localtime(&t);
	alarm(((60 - ptm->tm_min) + (_ld_root->subnet_refresh - 1) * 60) * 60);
    }
    
    return 0;
    
 _pc_err:
    syslog(LOG_ERR, "ldap: parse config file error on line %d\n", lineno);
    fclose(file);
    
    return -1;
}


