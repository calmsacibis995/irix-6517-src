/*
** lookup.c
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/filio.h>
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

#define LDAP_BUF_SIZE 	1024

#define LDAP_MORE(x)  (x->ld_sb.ptr < x->ld_sb.end)	

_ldm_t_ptr 	_domain_root;
_lsrv_t_ptr	_referral_servers = (_lsrv_t_ptr) 0;

#ifdef _SSL
int		_ldap_security = LDAP_SECURITY_NONE;
int		_ldap_cipher;
#endif

int zero_timeout(nsd_file_t **rq, nsd_times_t *to);
void add_requests(_lsrv_t_ptr sv, _lrqst_t_ptr req);
void dump_state(int n);


/*****************************************************
**
** helper routines
**
*****************************************************/


#include <libgen.h>
#include <ctype.h>

#define COPY(s1, s2, l, n) { if ((n) > l) { memcpy((s1), (s2), (l)); (s1) += (l); (n) -= (l); } else { return REG_ESPACE; } }
#define CAT(s, p, n) { if ((n) > 1) { *(s)++ = (p); (n)--; } else { return REG_ESPACE; } }

/* 06/99 gomez@engr.sgi.com
** (Shamelessly stolen from John's Schimmel directory)
** Substitute match based on pattern in sub.  & = match, \n = match n
*/
int
re_sub(regex_t *rp, char *s, char *sub, char *buf, size_t len)
{
        char *p, *q;
        int j, result;
        regmatch_t pm[10];

        if (! rp || ! s || ! sub || ! buf) {
                return -1;
        }

        result = regexec(rp, s, 10, pm, 0);

        p = buf;
        if (result == 0) {
                j = pm[0].rm_so;
                if (j) {
                        COPY(p, s, j, len);
                }
                for (q = sub; *q; q++) {
                        if (*q == '&') {
                                j = pm[0].rm_eo - pm[0].rm_so;
                                COPY(p, s + pm[0].rm_so, j, len);
                        } else if (*p == '\\') {
                                q++;
                                if (! *q) {
                                        break;
                                }
                                if (isdigit(*q)) {
                                        j = *q - '0';
                                        COPY(p, s + pm[j].rm_so,
                                            pm[j].rm_eo - pm[j].rm_so, len);
                                } else {
                                        CAT(p, *q, len);
                                }
                        } else {
                                CAT(p, *q, len);
                        }
                }
                COPY(p, s + pm[0].rm_eo, strlen(s + pm[0].rm_eo), len);
        } else {
                COPY(p, s, strlen(s), len);
                return 0;
        }
        *p = 0;

        return result;

}/* int re_sub() */

_ldm_t_ptr get_domain(char *dom)
{
	_ldm_t_ptr	d;

	if (! dom || ! *dom)
		return _domain_root;

	for (d = _domain_root->next; d != NULL; d = d->next)
		if (! strcmp(d->name, dom)) return d;

	return NULL;
}

_lrqst_t_ptr make_request(_lmap_t_ptr s, char *table, char *key, int l)
{
	_lrqst_t_ptr 	r;
	_lmap_t_ptr	map;
	_lfmt_t_ptr	fmt;
	char		*f, *p, *ks, *kt;
	int		def_l, num, len;

	if ((r = nsd_malloc(sizeof(_lrqst_t))) == NULL) {
		nsd_logprintf(0, "ldap: make_request: malloc failed\n");
		return NULL;
	}

	r->table 	= nsd_strdup(table);
	if (l) r->key  	= (char *) 0;
	else   r->key 	= nsd_strdup(key);
	r->msgid 	= -1;
	r->toc		= 0;
	r->num_sent 	= 0;
	r->status 	= REQ_WAITING;
	r->filter	= (char *) 0;
	r->ref_list 	= (_lref_t_ptr) 0;
	r->ref_pos 	= (_lref_t_ptr) 0;
	r->flags 	= 0;
	r->map 		= (_lmap_t_ptr) 0;
	r->next 	= (_lrqst_t_ptr) 0;

	for (map = s; map != NULL; map = map->next) {
		if (strcasecmp(map->name, r->table) == 0) {
			if (l) {
				f = map->filter_list;
				num = map->list_num;
				len = map->list_len;
			} else {
				f = map->filter_lookup;
				num = map->lookup_num;
				len = map->lookup_len;
			}

			if (! f) continue;

			r->map = map;
			
			switch (num) {
	
			case 0:
				r->filter = nsd_strdup(f);
				break;

			case 1:
				r->filter = nsd_malloc(len + strlen(key) + 1);	
				sprintf(r->filter, f, key);
				break;

			case 2:
				r->filter = nsd_malloc(len + strlen(key) + 1);
				ks = (char *) alloca(strlen(key) + 1);
				strcpy(ks, key);
				if (kt = strpbrk(ks, "./,")) *kt++ = (char) 0;
				else if ((kt = map->def) == NULL) goto _mr_err;
				sprintf(r->filter, f, ks, kt);
				break;

			}

			goto _mr_jump;
		}
	}

_mr_jump:		
	if (! r->map) goto _mr_err;
	if (! r->table || (! l && ! r->key) || ! r->filter) goto _mr_err;

	return r;

_mr_err:
	if (r->table)	free(r->table);
	if (r->key)	free(r->key);
	if (r->filter) 	free(r->filter);
			free(r);

	return NULL;
}

_lsrv_t_ptr lookup_server(int fd)
{
	_lsrv_t_ptr 	srv;
	_ldm_t_ptr	d;

	for (d = _domain_root; d != NULL; d = d->next) {
		srv = d->sls;
		do {
			if (srv->fd == fd) return srv;
			srv = srv->next;
		} while (srv != d->sls);
	}

	for (srv = _referral_servers; srv != NULL; srv = srv->next)
		if (srv->fd == fd) return srv;

	return NULL;
}

_lrqst_t_ptr lookup_request(int msgid)
{
	_lsrv_t_ptr	s;
	_lrqst_t_ptr	req;
	_ldm_t_ptr	d;

	s = _domain_root->sls;
	for (d = _domain_root; d != NULL; d = d->next) {
		s = d->sls;
		do {
			for (req = s->req; req != NULL; req = req->next)
				if (req->msgid == msgid) return req;
			s = s->next;
		} while (s != d->sls);
	}

	for (s = _referral_servers; s != NULL; s = s->next)
		for (req = s->req; req != NULL; req = req->next)
			if (req->msgid == msgid) return req;

	return NULL;
}

void my_free_request(_lrqst_t_ptr req)
{
	_lref_t_ptr	p, pn;

	if (req->key) free(req->key);
	free(req->table);
	free(req->filter);
	
	p = req->ref_list;
	while (p) {
		pn = p->next;
		free(p);
		p = pn;
	}

	free(req);
}

int remove_request(_lrqst_t_ptr req, _ldm_t_ptr *d)
{
	_lsrv_t_ptr	s;
	_lrqst_t_ptr	r;

	if (req->flags & REQ_FLAG_REF) {
		for (s = _referral_servers; s != NULL; s = s->next) {
			if (s->req == req) { 
				s->req = req->next;
				return 0;
			} else {
				for (r = s->req; r != NULL; r = r->next) {
					if (r->next == req) {
						r->next = req->next;
						return 0;
					}
				}
			}
		}

		return -1;
	}

	for (*d = _domain_root; *d != NULL; *d = (*d)->next) {
		s = (*d)->sls;
		nsd_logprintf(3, "remove_request: %s\n", req->key);
		do {
			if (! s->req) {
				s = s->next;	
				continue;
			}

			if (s->req == req) {
				s->req = s->req->next;
				return 0;
			}

			for (r = s->req; r->next != NULL; r = r->next) {
				if (r->next == req) {
					r->next = r->next->next;
					return 0;
				}
			}
			s = s->next;
		} while (s != (*d)->sls);
	}

	return -1;
}


/*****************************************************
**
** referral helper routines
**
*****************************************************/

char *parse_referral(obj_string r_o, char **b)
{
	char *p, *url;

	url = r_o.data;

	if (! url || ! *url) return NULL;

	*b = (char *) 0;

	if ((url = strchr(url, '/')) == NULL) return NULL;
	if ((url = strchr(++url, '/')) == NULL) return NULL;
	if (p = strchr(++url, '/')) {
		*p++ = (char) 0;
		if (*p) *b = strdup(p);
	}

	return (url);
}

void add_referrals(obj_string r, int id, _ldm_t_ptr d)
{
	_lrqst_t_ptr	req;
	_lref_t_ptr	ref, rfp;
	_lsrv_t_ptr	sv, sv_p;
	char		*colon, *name, *base;
	struct hostent	*he;
	short		port;

	if ((req = lookup_request(id)) == NULL) {
		nsd_logprintf(1, "ldap: add_referrals: bad id: %d\n", id);
		return;
	}

	if ((name = parse_referral(r, &base)) == NULL) {
		nsd_logprintf(1, "ldap: add_referrals: bad referral: %s\n", 
			r.data);
		return;
	}

	if (colon = strchr(name, ':')) {
		port = atoi(colon + 1);
		*colon = (char) 0;
	} else port = LDAP_PORT;

	for (sv = _referral_servers; sv != NULL; sv = sv->next) {
		sv_p = sv;
		if (port == sv->port && strcmp(name, sv->name) == 0) break;
	}

	if (! sv) {
		if ((sv = alloc_server()) == NULL) return;

		if ((sv->name = nsd_strdup(name)) == NULL) {
			free(sv);
			return;
		}

		if (isdigit(*name)) {
			if ((sv->addr = nsd_strdup(name)) == NULL) {
				free(sv->name);
				free(sv);
				return;
			}
		} else {
			if (he = gethostbyname(name)) {
				if ((sv->addr = strdup((char *)inet_ntoa(*(
					(struct in_addr *)(*he->h_addr_list)))))
					== NULL) {
					free(sv->name);
					free(sv);
					return;
				}
			}
		}

		/* what do we inheret? */
		sv->parent = d;
		sv->base = base;
		sv->port = port;
		sv->domain = (char *) 0;
		sv->binddn = (char *) 0;
		sv->bindpwd = (char *) 0;
		sv->flags = SRV_FLAG_REF;
		sv->next = (_lsrv_t_ptr) 0;

		if (_referral_servers) sv_p->next = sv;
		else _referral_servers = sv;
	}

	if ((ref = (_lref_t_ptr) nsd_malloc(sizeof(_lref_t))) == NULL) {
		nsd_logprintf(1, "ldap: malloc failed\n");
		return;
	}
	ref->sv = sv;
	ref->num = 0;
	ref->next = (_lref_t_ptr) 0;

	if (! req->ref_list) 
		req->ref_list = ref;
	else {
		for (rfp = req->ref_list; rfp->next != NULL; rfp = rfp->next) ;
		rfp->next = ref;
	}
}


/*****************************************************
**
** handling a callback
**
*****************************************************/

int append_prefix(_lrqst_t_ptr req, LDAP *ld, LDAPMessage *e)
{
	char		**vals;
	_litm_t_ptr	item;
	int		i;

	if (req->map->prefix->start) 
		nsd_append_result(req->rq, NS_SUCCESS, req->map->prefix->start,
			strlen(req->map->prefix->start));

	for (item = req->map->prefix->item; item != NULL; 
		item = item->next) {

		if (strcmp(item->name, "KEY") == 0) {
			vals = (char **) malloc(2 * sizeof(char *));
			vals[0] = strdup(req->key);
			vals[1] = (char *) 0;
		} else
			vals = ldap_get_values(ld, e, item->name);

		if (! vals && item->alt) 
			vals = ldap_get_values(ld, e, item->alt);

		if (vals && vals[0]) {
			if (item->flags & ITEM_FLAG_SKIP)
				goto _new_lazy_jump;

			if (*(vals[0]) && nsd_append_result(req->rq, NS_SUCCESS,
				vals[0], strlen(vals[0])) != NSD_OK) 
				goto _ap_err;

_new_lazy_jump:
			if (item->flags & ITEM_FLAG_PLUS) {
			    for (i=1; vals[i] != NULL; i++) {
				if ((i != 1 || 
				    ! (item->flags & ITEM_FLAG_SKIP)) 
				    && item->sep
				    && (nsd_append_result(req->rq, NS_SUCCESS,
				    item->sep, strlen(item->sep)) != NSD_OK))
					goto _ap_err;
				if (nsd_append_result(req->rq, NS_SUCCESS,
				    vals[i], strlen(vals[i])) != NSD_OK)
					goto _ap_err;
			    }
			}

			if ((! (item->flags & ITEM_FLAG_PLUS)) &&
			    item->sep &&
			    (nsd_append_result(req->rq, NS_SUCCESS, item->sep,
				strlen(item->sep)) != NSD_OK)) goto _ap_err;

			ldap_value_free(vals);
		}
	}

	if (req->map->prefix->end) 
		nsd_append_result(req->rq, NS_SUCCESS, req->map->prefix->end, 
			strlen(req->map->prefix->end));

	return 0;

_ap_err:
	return 1;
}

#define VALUE_SZ	255	/* Allowed size for attr value after regsub */

/* Do regsub in attribute value if
 * necessary.
 * Returns out_val if substitution succeeds
 *	   in_val if no substitution
 *         NULL if regsub fails
 */
	

char * 
_do_regsub(_litm_t_ptr item, _ldm_t_ptr d, char * in_val, char * out_val)
{
	int	n;
	
	for( n = 0; n < d->regsub_ndx; n++)
		if ( (item->name && !(strcasecmp( item->name, d->att_type[n] )))
		     || 
		     (item->alt && !(strcasecmp( item->alt, d->att_type[n] )))){
	    
			/* Apply regsub to result in
			 * vals[0]
			 */
		    
			if ( re_sub( &(d->match[n]),
				     in_val,
				     d->sub[n],
				     out_val,
				     VALUE_SZ )!= 0 ) {
				return NULL;
			}
			
			return out_val;
		}
	    
	return in_val;

}/* char * _do_regsub() */
		
int make_response(LDAP *ld, _lrqst_t_ptr req, LDAPMessage *result)
{

	LDAPMessage	*e;
	char		**vals, c_ret = '\n';
	int		first = 1, el, pl, rc, i, num=0, st, nb, rl, cb;
	_litm_t_ptr	item;
	_lpair_t_ptr	r;
	_lfmt_t_ptr	fmt;
	nsd_file_t 	**rq;
        int 		dummy;

	/* 06/99 gomez@engr.sgi.com 
	 * The following declarations were added to support 
	 * regular expressions substitutions.
	 */
	_ldm_t_ptr	d;		/* Domain for current request */
	char		value[VALUE_SZ];	/* Value after regsub */
	char		*nsd_val;	/* Value to hand to nsd */

	nsd_logprintf(3, "make_response:->\n");	
	rq = &(req->rq);

	nb = 1;
	fmt = req->map->format;

	d = req->dptr; /* Domain under which request was issued. JCGV */
	
	for (e = ldap_first_entry(ld, result); e != NULL;
		e = ldap_next_entry(ld, result, e)) {

		cb = (*rq)->f_used;

		if (req->map->prefix && first && (req->status == REQ_SENT)
			&& (cb == 0)) 
			if (append_prefix(req, ld, e)) goto _sr_err;

		if (! first && req->map->prefix)
			nsd_append_result(*rq, NS_SUCCESS, fmt->end,
				strlen(fmt->end));

		num++;

		st = 1;
		first = 0;

		if (fmt->start) 
			nsd_append_result(*rq, NS_SUCCESS, fmt->start,
				strlen(fmt->start));

		for (item = fmt->item; item != NULL; 
			item = item->next) {

			if (strcmp(item->name, "KEY") == 0) {
				vals = (char **) malloc(2 * sizeof(char *));
				vals[0] = strdup(req->key);
				vals[1] = (char *) 0;
			} else
				vals = ldap_get_values(ld, e, item->name);

			if (! vals && item->alt) 
				vals = ldap_get_values(ld, e, item->alt);

			if (vals && vals[0]) {
				if (item->flags & ITEM_FLAG_SKIP)
					goto _lazy_jump;

				if (*(vals[0])) {

					/* 06/99 gomez@engr.sgi.com 
					 * The following code was 
					 * added to support regular
					 * expressions substitutions.
					 */

					if ( (nsd_val =
					      _do_regsub(item,
							 d,
							 vals[0],
							 value)) == NULL )
						goto _sr_err;

					if (nsd_append_result(*rq, 
							      NS_SUCCESS,
							      nsd_val,
							      strlen(nsd_val)) 
					    != NSD_OK)
						goto _sr_err;

				}/* *(vals[0]) */
_lazy_jump:
				if (item->flags & ITEM_FLAG_PLUS) {
				    for (i=1; vals[i] != NULL; i++) {
					if ((i != 1 ||
					     ! (item->flags & ITEM_FLAG_SKIP))
					    && item->sep
					    && (nsd_append_result(*rq,
					    NS_SUCCESS, item->sep, 
					    strlen(item->sep))) != NSD_OK)
						goto _sr_err;

					/* 06/99 gomez@engr.sgi.com 
					 * The following code was 
					 * added to support regular
					 * expressions substitutions.
					 */
					

					if ( (nsd_val =
					      _do_regsub(item,
							 d,
							 vals[i],
							 value)) == NULL )
						goto _sr_err;

					if (nsd_append_result(*rq, 
							      NS_SUCCESS,
							      nsd_val,
							      strlen(nsd_val)) 
					    != NSD_OK)
						goto _sr_err;



				    }
				}

				if ((! (item->flags & ITEM_FLAG_PLUS)) &&
				    item->sep &&
				    (nsd_append_result(*rq, NS_SUCCESS,
					item->sep, strlen(item->sep))) != 
					NSD_OK) goto _sr_err;

				ldap_value_free(vals);
			} else {
				for (r = req->map->require; r; r = r->next)
					if (strcasecmp(r->key, item->name) 
						== NULL) {
						num--;
						if ((*rq)->f_used > 0) {
						  (*rq)->f_data[cb] = '\0';
						  (*rq)->f_used = cb;
						}
						goto _skip_ldap_entry;
					}
				if ((! (item->flags & ITEM_FLAG_PLUS)) &&
					item->sep &&
					(nsd_append_result(*rq, NS_SUCCESS,
					item->sep, strlen(item->sep))) 
					!= NSD_OK) goto _sr_err;
			}
			st = 0;
		}
		if ((*rq)->f_used > 0) {
			if (! req->map->prefix)
				nsd_append_result(*rq, NS_SUCCESS, &c_ret, 1);
		} 
_skip_ldap_entry:
		dummy = 0;
	}

	if (((*rq)->f_used > 0) && (req->map->prefix)) {
		if (num > 0 && fmt->end) nsd_append_result(*rq, NS_SUCCESS, 
			fmt->end, strlen(fmt->end));
	}

	if (! num) {
		req->ret_state = NSD_NEXT;
		(*rq)->f_status = NS_NOTFOUND;
	} else {
		req->ret_state = NSD_OK;
		(*rq)->f_status = NS_SUCCESS;
	}

	return 0;

_sr_err:
	if (vals) ldap_value_free(vals);
	nsd_logprintf(1, "ldap: malloc failed\n");
	req->ret_state = NSD_ERROR;
	(*rq)->f_status = NS_FATAL;

	return -1;
}

int ship_response(nsd_file_t **rq, _lrqst_t_ptr req)
{
	_lrqst_t_ptr	req_n;
	int		i, ret;
	_ldm_t_ptr	d;

	if (! req) return NSD_CONTINUE;

	nsd_logprintf(3, "shipping: %s\n", req->key);

	do {
		if (req->status != REQ_DONE) continue;

		nsd_logprintf(3, "in ship_response\n");
		dump_state(3);
		nsd_logprintf(3, "\tf_status: %d\n", req->rq->f_status);
		nsd_logprintf(3, "\tret_state: %d\n", req->ret_state);

		*rq = req->rq;
		ret = req->ret_state;
		req_n = req->next;

		nsd_timeout_remove(*rq);
		req->toc--;

		for (i = 0; i < req->toc; i++) {
			nsd_logprintf(3, "ldap: FORCED TOUT REM\n");
			nsd_timeout_remove(*rq);
		}

		if (remove_request(req, &d) == 0) my_free_request(req);

		return ret;
				
	} while (req = req->next);

	return NSD_CONTINUE;
}

int ldap_callback(nsd_file_t **rq, int fd)
{
	_lsrv_t_ptr 	sv;
	_lrqst_t_ptr	req, req_n;
	int		i, rc, ret;
	LDAPMessage	*r;
	char		*res;
	char		*ref_st;
	struct timeval	tv;
	int		resCode;
	obj_string	ref_o;

	nsd_logprintf(3, "ldap_callback\n");

	if ((sv = lookup_server(fd)) == NULL) {
		nsd_logprintf(1, "ldap_callback: bad fd: %d\n", fd);
		return NSD_CONTINUE;
	}

	sv->time = time(0);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	do {
		dump_state(3);

		rc = ldap_result(sv->ldap, LDAP_RES_ANY, 0, &tv, &r);
		nsd_logprintf(3, "ldap_callback: fd: %d result: %d\n", fd, rc);

		if (rc <= 0) {
        		sv->status = SRV_ERROR;
        		sv->time = time(0);
			nsd_callback_remove(sv->fd);
			ldap_unbind(sv->ldap);
        		sv->fd = -1;
			sv->ldap = (LDAP *) 0;
	
			break;
		}

		if ((req = lookup_request(r->lm_id)) == NULL) {
			nsd_logprintf(1, "ldap_callback: bad id: %d\n", 
				r->lm_id);
			ldap_msgfree(r);

			break;
		} 

		switch (rc) {

		case 101:
			switch (ldap_get_result_code(r)) {

			case 10:	/* referral */
				nsd_logprintf(3, "ldap: referral\n");
				nsd_timeout_remove(req->rq);
				req->toc--;
				ldap_get_referral(&ref_o, r);
				add_referrals(ref_o, r->lm_id, 
					sv->parent);
				free(ref_o.data);

				if (refer_request(req) == -1) {
					nsd_logprintf(3, 
						"refer_req failed\n");
					req->ret_state = NSD_NEXT;
					req->rq->f_status = NS_NOTFOUND;
					set_to_zero(req, 0);
				}

				break;

			default:
				nsd_timeout_remove(req->rq);
				nsd_timeout_new(req->rq, 0, 
					zero_timeout, (void *) req);
				if (req->status == REQ_SENT) {
					req->ret_state = NSD_NEXT;
					req->rq->f_status = NS_NOTFOUND;
				}
				req->status = REQ_DONE;

				break;

			}

			break;

		case 100:
			make_response(sv->ldap, req, r);
			req->status = REQ_REC;
			break;

		}

		ldap_msgfree(r);
	} while (LDAP_MORE(sv->ldap));

	return (ship_response(rq, sv->req));
}

int zero_timeout(nsd_file_t **rq, nsd_times_t *to)
{
	_lrqst_t_ptr	req, req_n;
	_lsrv_t_ptr	sv;
	int		i, ret;

	nsd_logprintf(3, "zero_timeout\n");

	nsd_timeout_remove(*rq);
	req = to->t_clientdata;

	return(ship_response(rq, req));
}

int set_to_zero(_lrqst_t_ptr req, int all)
{
	if (all) {
		for ( ; req; req = req->next) {
			nsd_timeout_new(req->rq, 0, zero_timeout, (void *) req);
			req->toc++;
			req->status = REQ_DONE;
			req->ret_state = NSD_NEXT;
			req->rq->f_status = NS_NOTFOUND;
		}
	} else {
		nsd_timeout_new(req->rq, 0, zero_timeout, (void *) req);
		req->toc++;
		req->status = REQ_DONE;
		req->ret_state = NSD_NEXT;
	}

	return 0;
}

void add_requests(_lsrv_t_ptr sv, _lrqst_t_ptr req)
{
	_lrqst_t_ptr	req_r, req_s;

	for (req_r = req; req_r != NULL; req_r = req_r->next) {
		req_r->dptr = sv->parent;	/* JCGV Need yo know domain */
		if (req->flags & REQ_FLAG_DROP) continue;
		req_r->status = REQ_WAITING;
		req_r->msgid = -1;
	}

	if (sv->req) {
		for (req_r = sv->req; req_r != NULL; req_r = req_r->next) 
			req_s = req_r;
		req_s->next = req;
	} else
		sv->req = req;
}


/*****************************************************
**
** timeout code
**
*****************************************************/

int search_timeout(nsd_file_t **rq, nsd_times_t *to)
{
	_lrqst_t_ptr	req;
	_ldm_t_ptr	dom;
	int		rc;

	req = to->t_clientdata;
	*rq = req->rq;

	nsd_logprintf(2, "search_timeout: %s\n", req->key);

	if (req->flags & REQ_FLAG_REF) {
		nsd_timeout_remove(*rq);
		nsd_logprintf(3, "remove timeout (search,to): %s\n",
			req->key);
		req->next = (_lrqst_t_ptr) 0;
		rc = refer_request(req);
		if (rc == -1) {
			if (remove_request(req, &dom) == 0)
				my_free_request(req);
			(*rq)->f_status = NS_NOTFOUND;
			return NSD_NEXT;
		} else {
			return NSD_CONTINUE;
		}
	}

	/* XXX dom may not exist! */
	if (remove_request(req, &dom) == 0) my_free_request(req);
	(*rq)->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

/*****************************************************
**
** finding a server
**
*****************************************************/

int open_timeout(nsd_file_t **rq, nsd_times_t *to)
{
	_lsrv_t_ptr	sv, sv_p;
	_lrqst_t_ptr	req;
	int		rc;

	sv = to->t_clientdata;
	nsd_logprintf(2, "open_timeout: %s\n", sv->addr);

	dump_state(3);

	*rq = sv->to_req->rq;
	sv->flags &= ~SRV_FLAG_OPENTO;
	nsd_timeout_remove(*rq);
	sv->to_req->toc--;
	nsd_logprintf(3, "remove timeout (open): %s %d\n", sv->to_req->key,
		sv->to_req->toc);

	nsd_callback_remove(sv->fd);

	sv->status = SRV_ERROR;
	sv->time = time(0);
	sv->fd = -1;

	if (sv->flags & SRV_FLAG_REF) {
		for (req = sv->req; req != NULL; req = req->next) {
			rc = refer_request(req);
			if (rc == -1) {
				req->ret_state = NSD_NEXT;
				(*rq)->f_status = NS_NOTFOUND;
				set_to_zero(req, 0);
			}
		}
	} else {
		if (sv->req) {
			*rq = sv->req->rq;
			req = sv->req;
			sv->req = (_lrqst_t_ptr) 0;
			place_requests(sv->parent, req, 1);
		}
	}

	return NSD_CONTINUE;
}

int open_callback(nsd_file_t **rq, int fd)
{
	int 		rc, i, errs, size;
	_lsrv_t_ptr 	sv;
	_lrqst_t_ptr	req;
#ifdef _SSL
	SSL		*ssl;
#endif

	nsd_callback_remove(fd);

	sv = lookup_server(fd);
	nsd_logprintf(3, "ldap: open_callback: %d st: %d\n", fd, sv->status);
	*rq = sv->to_req->rq;

	sv->flags &= ~SRV_FLAG_OPENTO;
	nsd_timeout_remove(sv->to_req->rq);
	sv->to_req->toc--;
	nsd_logprintf(3, "remove timeout (open): %s %d\n", sv->to_req->key,
		sv->to_req->toc);

#ifdef _SSL
	if (sv->status == SRV_SSL) {
		ssl = (SSL *) sv->ldap->ld_sb.ssl;

		if ((rc = ssl_open(ssl)) != 0) {
			perror("ssl_open");
			return -1;
		}

		nsd_logprintf(3, "open_callback: ssl state: %d\n", ssl->state);

		if (ssl->state != SSL_STATE_CON) {
			sv->flags |= SRV_FLAG_OPENTO;
			nsd_callback_new(sv->fd, open_callback, NSD_READ);
                        nsd_logprintf(3, "ldap: new open_callback\n");

			nsd_timeout_new(sv->to_req->rq, 
				sv->to_req->open_timeout, open_timeout, 
				(void *) sv);
			sv->to_req->toc++;
			nsd_logprintf(3, "new timeout (open): %s %d\n", 
				sv->to_req->key, sv->to_req->toc);

			return NSD_CONTINUE;
		} else {
			sv->status = SRV_BINDING;
		}
	} else 
#endif
	if (sv->status == SRV_WAITING) {
		errs = 0;
		size = sizeof(errs);
		i = getsockopt(fd, SOL_SOCKET, SO_ERROR, &errs, &size);
		if (i < 0) {
			nsd_logprintf(2, "ldap: getsockopt failed\n");
			sv->status = SRV_ERROR;
			sv->time = time(0);
			goto _open_callback_err;
		}

		if (errs) {
			nsd_logprintf(2, "ldap: connect failed\n");
			sv->status = SRV_ERROR;
			sv->time = time(0);
			goto _open_callback_err;
		}

		sv->status = SRV_BINDING;
	} else
		sv->status = SRV_CONNECTING;

	rc = open_server(sv, sv->req->error_timeout);

	switch (rc) {

	case 0:
		break;

	case -2:
		sv->flags |= SRV_FLAG_OPENTO;
		nsd_timeout_new(sv->to_req->rq, sv->to_req->open_timeout, 
			open_timeout, (void *) sv);
		sv->to_req->toc++;
		nsd_logprintf(3, "new timeout (open): %s %d\n", 
			sv->to_req->key, sv->to_req->toc);
		break;

	case -1:
_open_callback_err:
		if (sv->flags & SRV_FLAG_REF) {
			for (req = sv->req; req != NULL; req = req->next) {
				rc = refer_request(req);
				if (rc == -1) {
					req->ret_state = NSD_NEXT;
					(*rq)->f_status = NS_NOTFOUND;
					set_to_zero(req, 0);
				}
			}
		} else {
			if (sv->req) {
				*rq = sv->req->rq;
				req = sv->req;
				sv->req = (_lrqst_t_ptr) 0;
				place_requests(sv->parent, req, 1);
			}
		}
	}

	return NSD_CONTINUE;
}

int open_server(_lsrv_t_ptr sv, int eto) 
{
	int		rc, loop = 1;
	LDAP		*ld = NULL;
	LDAPMessage	*r;
	struct timeval	tv;

	nsd_logprintf(3, "open_server %s, status: %d, port: %u\n",
		      sv->addr, sv->status, sv->port);

	while (loop) {
		loop = 0;
		switch (sv->status) {

		case SRV_UNBOUND:
#ifdef _SSL
			if (_ldap_security == LDAP_SECURITY_SSL) {
				if ((ld = ldap_ssl_init(sv->addr, sv->port, 
					_ldap_cipher)) == NULL) {
					rc = -1;
					break;
				}
			} else 
#endif
				if ((ld = ldap_init(sv->addr, sv->port)) 
					== NULL) {
					rc = -1;
					break;
				}

			sv->ldap = ld;
			sv->fd = ld->ld_sb.fd;
			if (ld->ld_status == LDAP_STAT_CONNECTING) {
				nsd_logprintf(3, "ldap: new open_callback\n");
				nsd_callback_new(sv->fd, open_callback,
					NSD_WRITE);
				rc = -2;
#ifdef _SSL
				if (_ldap_security == LDAP_SECURITY_SSL)
					sv->status = SRV_SSL;
				else
#endif
					sv->status = SRV_WAITING;
			} else {
				/* XXX what if ssl does not block on connect? */
				sv->bindid = ldap_simple_bind(ld, sv->binddn, 
					sv->bindpwd, sv->version);
				sv->status = SRV_CONNECTING;
				loop = 1;
			}
			break;

		case SRV_WAITING:
			rc = -2;
			break;

		case SRV_BINDING:
			if ((sv->bindid = ldap_simple_bind(sv->ldap, 
				sv->binddn, sv->bindpwd, sv->version)) < 0) {
				rc = -1;
			} else {
				nsd_callback_new(sv->fd, open_callback,
					NSD_READ);
				nsd_logprintf(3, "ldap: new open_callback\n");
				rc = -2;
				sv->status = SRV_WAITING2;
			}
			break;

		case SRV_WAITING2:
			rc = -2;
			break;

		case SRV_CONNECTING:
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			rc = ldap_result(sv->ldap, sv->bindid, 1, &tv, &r);
			if (LDAP_MORE(sv->ldap)) {
				/* XXX is it possible that the server
				   will send more than one packet in 
				   response to a bind? */
				nsd_logprintf(2, "ldap: MORE TO READ\n");
			}
			if (rc == 97) {
				sv->status = SRV_CONNECTED;
				nsd_callback_new(sv->fd, ldap_callback, 
					NSD_READ);
				nsd_logprintf(3, 
					"open_server: new callback: %d\n", 
					sv->fd);
				nsd_logprintf(3,
					      "ldap: connected, sending rq\n");
				send_requests(sv);
				rc = 0;
			} else if (rc == -2) rc = -2;
			if (rc > 0) ldap_msgfree(r);
			break;

		case SRV_CONNECTED:
			rc = 0;
			break;

		case SRV_ERROR:
			if (time(0) - eto > sv->time) {
				sv->status = SRV_UNBOUND;
				loop = 1;
			} else {
				rc = -1;
			}
			break;
#ifdef _SSL
		case SRV_SSL:
			rc = -2;
			break;
#endif
		}
	}

	if (rc == -1) {
		/*
		** QUESTION: under what circumstances do we do an 
		** ldap_unbind here?
		*/
		sv->ldap = (LDAP *) 0;
		sv->fd = -1;
		sv->status = SRV_ERROR;
		sv->time = time(0);
	}

	return (rc);
}

_lsrv_t_ptr next_good_server(_ldm_t_ptr d, _lrqst_t_ptr req)
{
	_lsrv_t_ptr	sv;
	int		rc;

	sv = d->pos;
	do {
		rc = open_server(sv, req->error_timeout);
		if (rc == -1) sv = sv->next;
	} while (rc == -1 && sv != d->pos);

	if (rc != -1) d->pos = sv;

	if (rc == -2 && ! (sv->flags & SRV_FLAG_OPENTO)) {
		sv->flags |= SRV_FLAG_OPENTO;
		sv->to_req = req;
		nsd_timeout_new(req->rq, req->open_timeout, open_timeout,
			(void *) sv);
		req->toc++;
		nsd_logprintf(3, "new timeout (open): %s %d\n", req->key,
			req->toc);
	}
	if (rc == 0 && (sv->flags & SRV_FLAG_OPENTO)) {
		sv->flags &= ~SRV_FLAG_OPENTO;
		nsd_timeout_remove(sv->to_req->rq);
		sv->to_req->toc--;
		nsd_logprintf(3, "remove timeout (open): %s %d\n", 
			sv->to_req->key, sv->to_req->toc);
		sv->to_req = (_lrqst_t_ptr) 0;
	}

	if (rc == -1) return NULL;
	else return sv;
}

/*****************************************************
**
** placing requests
**
*****************************************************/

int send_requests(_lsrv_t_ptr sv)
{
	_lrqst_t_ptr	req;

	for (req = sv->req; req != NULL; req = req->next) {
		if (req->status != REQ_WAITING) continue;

		if ((req->num_sent > req->max_requests)
			|| ((req->msgid = ldap_search(sv->ldap, sv->base, 
			sv->scope, req->filter, req->map->format->attr, 
			0)) < 0)) {
nsd_logprintf(3, "num_sent = %d, max_requests = %d\n", req->num_sent, req->max_requests);
			req->rq->f_status = NS_NOTFOUND;
			set_to_zero(req, 0);
		} else {
			req->status = REQ_SENT;
			nsd_logprintf(3, "req: %s num_sent: %d\n", 
				req->key, req->num_sent);
			req->num_sent++;
			nsd_timeout_new(req->rq, req->search_timeout,
				search_timeout, (void *) req);
			req->toc++;
			nsd_logprintf(3, 
				"new timeout (search): %s %d\n", 
				req->key, req->toc);
		}
	}

	return 0;
}

int refer_request(_lrqst_t_ptr req)
{
	_lref_t_ptr	pos;
	_ldm_t_ptr	dom;
	int		rc = -1;

	if (req->num_sent <= req->max_requests && req->ref_list) {
		if (! req->ref_pos) {
			req->ref_pos = req->ref_list;
			pos = req->ref_pos;
		} else
			pos = req->ref_pos->next;

		while (rc == -1 && pos) {
			rc = open_server(pos->sv, req->error_timeout);
			if (rc == -1) pos = pos->next;
		}
	
		if (rc == -2 && ! (pos->sv->flags & SRV_FLAG_OPENTO)) {
			pos->sv->flags |= SRV_FLAG_OPENTO;
			pos->sv->to_req = req;
			nsd_timeout_new(req->rq, req->open_timeout, 
				open_timeout, (void *) pos->sv);
		}
		if (rc == 0 && (pos->sv->flags & SRV_FLAG_OPENTO)) {
			pos->sv->flags &= ~SRV_FLAG_OPENTO;
			nsd_timeout_remove(pos->sv->to_req->rq);
			pos->sv->to_req = (_lrqst_t_ptr) 0;
		}

		if (rc != -1) {
			req->ref_pos = pos;
			remove_request(req, &dom);
			req->flags |= REQ_FLAG_REF;
			req->next = 0;
			add_requests(pos->sv, req);
			if (pos->sv->status == SRV_CONNECTED) 
				send_requests(pos->sv);
		}
	}

	return rc;
}

int place_requests(_ldm_t_ptr d, _lrqst_t_ptr req, int all)
{
	_lsrv_t_ptr	sv;
	_lrqst_t_ptr	req_s;
	int		rc;

	nsd_logprintf(3, "place_requests\n");
	dump_state(3);

	for (req_s = req; req_s != NULL; req_s = req_s->next)
		if (req_s->status == REQ_SENT) {
			nsd_timeout_remove(req_s->rq);
			req_s->toc--;
			nsd_logprintf(3, "remove timeout (?,pr) %s %d\n", 
				req_s->key, req_s->toc);
		}

	if ((sv = next_good_server(d, req)) == NULL) {
		if (all) {
			set_to_zero(req, 1);
			return NSD_ERROR;
		}

		req->rq->f_status = NS_NOTFOUND;
		my_free_request(req);
		return NSD_NEXT;
	}
	add_requests(sv, req);
	if (sv->status == SRV_CONNECTED) send_requests(sv);
	return NSD_CONTINUE;
}

/*****************************************************
**
** nsd interface
**
*****************************************************/

int lookup(nsd_file_t *rq)
{
	char 		*d, *t, *k;
	_ldm_t_ptr 	dm;
	_lrqst_t_ptr 	r;

	d = nsd_attr_fetch_string(rq->f_attrs, "domain", NULL);
	t = nsd_attr_fetch_string(rq->f_attrs, "table", NULL);
	k = nsd_attr_fetch_string(rq->f_attrs, "key", NULL);

	nsd_logprintf(3, "entering ldap lookup: %s %s\n", k, t);

	if (! t || ! k) {
		rq->f_status = NS_BADREQ;
		return NSD_ERROR;
	}

	if ((dm = get_domain(d)) == NULL) {
		rq->f_status = NS_BADREQ;
		return NSD_ERROR;
	}

	if ((r = make_request(dm->map, t, k, 0)) == NULL) {
		rq->f_status = NS_TRYAGAIN;
		return NSD_NEXT;
	}
	r->rq = rq;

	r->error_timeout = nsd_attr_fetch_long(rq->f_attrs, 
		"error_timeout", 10, LDAP_ERROR_TIMEOUT);
	if (r->error_timeout < 0) {
		r->error_timeout = 0;
	}

	r->open_timeout = 1000 * nsd_attr_fetch_long(rq->f_attrs, 
		"open_timeout", 10, LDAP_OPEN_TIMEOUT);
	if (r->open_timeout < 0) {
		r->open_timeout = 0;
	}

	r->search_timeout = 1000 * nsd_attr_fetch_long(rq->f_attrs, 
		"search_timeout", 10, LDAP_SEARCH_TIMEOUT);
	if (r->search_timeout < 0) {
		r->search_timeout = 0;
	}

	r->max_requests = nsd_attr_fetch_long(rq->f_attrs,
		"max_requests", 10, LDAP_MAX_REQUESTS);

	return (place_requests(dm, r, 0));
}

int list(nsd_file_t *rq)
{
	char 		*t, *d;
	_ldm_t_ptr	dm;
	_lrqst_t_ptr	r;

	nsd_logprintf(3, "entering list (ldap)\n");

	if (! rq) return NSD_ERROR;

	d = nsd_attr_fetch_string(rq->f_attrs, "domain", NULL);
	t = nsd_attr_fetch_string(rq->f_attrs, "table", (char *)0);
	if (! t) {
		rq->f_status = NS_BADREQ;
		return NSD_ERROR;
	}

	if ((dm = get_domain(d)) == NULL) {
		rq->f_status = NS_BADREQ;
		return NSD_ERROR;
	}

	if ((r = make_request(dm->map, t, NULL, 1)) == NULL) {
		rq->f_status = NS_TRYAGAIN;
		return NSD_NEXT;
	}
	r->rq = rq;

	r->error_timeout = nsd_attr_fetch_long(rq->f_attrs, 
		"error_timeout", 10, LDAP_ERROR_TIMEOUT);
	if (r->error_timeout < 0) {
		r->error_timeout = 0;
	}

	r->open_timeout = 1000 * nsd_attr_fetch_long(rq->f_attrs, 
		"open_timeout", 10, LDAP_OPEN_TIMEOUT);
	if (r->open_timeout < 0) {
		r->open_timeout = 0;
	}

	r->search_timeout = 1000 * nsd_attr_fetch_long(rq->f_attrs, 
		"search_timeout", 10, LDAP_SEARCH_TIMEOUT);
	if (r->search_timeout < 0) {
		r->search_timeout = 0;
	}

	r->max_requests = nsd_attr_fetch_long(rq->f_attrs,
		"max_requests", 10, LDAP_MAX_REQUESTS);

	return (place_requests(dm, r, 0));
}

void kill_server(_lsrv_t_ptr srv, time_t t, int n)
{
	_lrqst_t_ptr	req, req_n;
	_ldm_t_ptr	dom;


	if (n == 10) {
		if (srv->status == SRV_CONNECTED) {
			ldap_unbind(srv->ldap);
			srv->ldap = (LDAP *) 0;
			nsd_callback_remove(srv->fd);
			srv->status = SRV_UNBOUND;
			srv->fd = -1;
		}
		if (srv->status != SRV_UNBOUND) {
			if (srv->flags & SRV_FLAG_OPENTO) {
				nsd_timeout_remove(srv->to_req->rq);
				srv->to_req->toc--;
				nsd_logprintf(3, 
					"remove timeout (open): %s %d\n",
					srv->to_req->key, srv->to_req->toc);
				srv->flags &= ~SRV_FLAG_OPENTO;
				srv->to_req = (_lrqst_t_ptr) 0;
			}
			srv->status = SRV_UNBOUND;
			srv->ldap = (LDAP *) 0;
		}
		req = srv->req;
		while (req) {
			req_n = req->next;
			if (req->status == REQ_SENT) {
				nsd_timeout_remove(req->rq);
				req->toc--;
				nsd_logprintf(3, 
					"remove timeout (?,ks): %s %d\n",
					req->key, req->toc);
			}
			if (remove_request(req, &dom) == 0)
				my_free_request(req);
			req = req_n;
		}
	} else if (t >= srv->time && srv->status == SRV_CONNECTED 
		&& ! srv->req) {
		nsd_logprintf(3, "shake removing: %s\n", srv->addr);
		nsd_callback_remove(srv->fd);
		srv->fd = -1;
		srv->status = SRV_UNBOUND;
		ldap_unbind(srv->ldap);
		srv->ldap = (LDAP *) 0;
	} 
}

int shake(int n)
{
	time_t 		t;
	_ldm_t_ptr	d;
	_lsrv_t_ptr	srv;

	t = time(0) - 600 + 60 * n;

	for (d = _domain_root; d != NULL; d = d->next) {
		srv = d->sls;
		if (srv) {
			do {
				kill_server(srv, t, n);
				srv = srv->next;
			} while (srv != d->sls);
		}
	}

	for (srv = _referral_servers; srv != NULL; srv = srv->next)
		kill_server(srv, t, n);
		
	return 0;
}

void dump_state(int n)
{
	_lsrv_t_ptr	srv;
	_lrqst_t_ptr	req;
	_ldm_t_ptr	d;

	nsd_logprintf(n, "-- BEGIN LDAP STATE --\n");

	for (d = _domain_root; d != NULL; d = d->next) {
		srv = d->sls;
		do {
			nsd_logprintf(n, "srv: %s st: %d fd: %d\n", srv->addr, 
				srv->status, srv->fd);
			for (req = srv->req; req != NULL; req = req->next) {
				nsd_logprintf(n, "req: %s %d %d %d\n", req->key,
					req->status, req->msgid, req->flags);
			}
			srv = srv->next;
		} while (srv != d->sls);
	}

	for (srv = _referral_servers; srv != NULL; srv = srv->next) {
		nsd_logprintf(n, "srv: %s st: %d fd: %d\n", srv->addr,
			srv->status, srv->fd);
		for (req = srv->req; req != NULL; req = req->next) {
			nsd_logprintf(n, "req: %s %d %d %d\n", req->key,
				req->status, req->msgid, req->flags);
		}
	}

	nsd_logprintf(n, "-- END LDAP STATE --\n");
}
