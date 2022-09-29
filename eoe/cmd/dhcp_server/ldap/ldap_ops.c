/*
 * ldap operations using teh configuration file
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <strings.h>
#include <ctype.h>
#include <alloca.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <lber.h>
#include <ldap.h>
#include <ldif.h>

#include "ldap_api.h"
#include "ldap_dhcp.h"

int ldhcp_level = LDHCP_LOG_RESOURCE;
int ldhcp_silent = 1;
_lsrv_t_ptr     _referral_servers = (_lsrv_t_ptr) 0;
_lrqst_t_ptr    _orphan_list = (_lrqst_t_ptr) 0;
int             _ldap_referral_flag = 0;
char            *_ldap_referral;
fd_set ldhcp_readset, ldhcp_exceptset, ldhcp_sendset;
int ldhcp_maxfd = 0;

int drop_timeout(_ld_t_ptr *rq, ldhcp_times_t *to);
void add_requests(_lsrv_t_ptr sv, _lrqst_t_ptr req);
void add_orphans(_lrqst_t_ptr req);
void remove_orphan(_lrqst_t_ptr req);
void dump_state(int n);
extern _lsrv_t_ptr alloc_server(void );
int refer_request(_lrqst_t_ptr req);
int place_requests(_ld_t_ptr d, _lrqst_t_ptr req, int all);
int open_callback(_ld_t_ptr *rq, int fd);
int send_requests(_lsrv_t_ptr sv);


/*
 * This routine will append results to out args
 */
int
ldhcp_append_result(_ld_t_ptr *rq, int status, int entnum, void *data, int type,
		    char *ent_name)
{
    _arg_ld_t_ptr res_item, res;
    
    if (! rq || ! data) {
	return LDHCP_ERROR;
    }

    if ((res_item = (_arg_ld_t_ptr)malloc(sizeof(struct arg_ldap_dhcp)))
	== NULL) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, 
			"ldap: arg_ldap_dhcp, malloc failed\n");
	return LDHCP_ERROR;
    }

    res_item->next = (_arg_ld_t_ptr)0;

    if ((res_item->attr_name = strdup(ent_name)) == NULL) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, 
			"ldap: attr_name, malloc failed\n");
	goto _at_res_error;
    }
    res_item->entnum = entnum;

    if (type == ITM_BERVAL) {
	res_item->is_berval = ITM_BERVAL;
	res_item->argv_bervals = data;
    }
    else {
	res_item->is_berval = 0;
	res_item->argv_strvals = data;
    }
    if (!((*rq)->f_result))
	(*rq)->f_result = res_item;
    else {
	for (res = (*rq)->f_result; res->next; res = res->next) 
	    ;
	res->next = res_item;
    }
    (*rq)->f_status = status;
    
    return LDHCP_OK;
 _at_res_error:
    if (res_item->attr_name) free(res_item->attr_name);
    if (res_item) free(res_item);
    
    return LDHCP_ERROR;

}



_lrqst_t_ptr lookup_request(int msgid)
{
    _lsrv_t_ptr	s;
    _lrqst_t_ptr	req;
    _ld_t_ptr	d;
    
    s = _ld_root->sls;
    d = _ld_root;
    s = d->sls;
    do {
	for (req = s->req; req != NULL; req = req->next)
	    if (req->msgid == msgid) return req;
	s = s->next;
    } while (s != d->sls);
    
    
    for (s = _referral_servers; s != NULL; s = s->next)
	for (req = s->req; req != NULL; req = req->next)
	    if (req->msgid == msgid) return req;
    
    return NULL;
}

void my_free_request(_lrqst_t_ptr req)
{
    _lref_t_ptr	p, pn;
    
    if (req->key) free(req->key);
    if (req->base) free(req->base);
    free(req->table);
    free(req->filter);
    
    p = req->ref_list;
    while (p) {
	pn = p->next;
	free(p);
	p = pn;
    }
    
    if (req->mods)
	ldap_mods_free(req->mods, 1);

    free(req);
}

_ld_t_ptr remove_request(_lrqst_t_ptr req)
{
    _lsrv_t_ptr	s;
    _lrqst_t_ptr	r;
    _ld_t_ptr	d;

    if (req->flags & REQ_FLAG_REF) {
	for (s = _referral_servers; s != NULL; s = s->next) {
	    if (s->req == req) { 
		s->req = req->next;
		return NULL;
	    } else {
		for (r = s->req; r != NULL; r = r->next) {
		    if (r->next == req) {
			r->next = req->next;
			return NULL;
		    }
		}
	    }
	}
	
	return NULL;
    }
    
    d = _ld_root;
    s = d->sls;
    do {
	if (! s->req) {
	    s = s->next;	
	    continue;
	}
	
	if (s->req == req) {
	    s->req = s->req->next;
	    return _ld_root;
	}
	
	for (r = s->req; r->next != NULL; r = r->next) {
	    if (r->next == req) {
		r->next = r->next->next;
		return d;
	    }
	}
	s = s->next;
    } while (s != d->sls);
    return NULL;
}

/*****************************************************
**
** referral helper routines
**
*****************************************************/

char *parse_referral(char *url, char **b)
{
    char *p;
    
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

void add_referrals(char *ref_s, int id, _ld_t_ptr d)
{
    _lrqst_t_ptr	req;
    _lref_t_ptr	ref, rfp;
    _lsrv_t_ptr	sv, sv_p;
    char		*colon, *name, *base;
    struct hostent	*he;
    short		port;
    
    if ((req = lookup_request(id)) == NULL) {
	ldhcp_logprintf(LDHCP_LOG_OPER, 
			"ldap: unknown request id: %d\n", id);
	return;
    }
    
    if ((name = parse_referral(ref_s, &base)) == NULL) {
	ldhcp_logprintf(LDHCP_LOG_OPER, "ldap: bad referral: %s\n", ref_s);
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
	
	if ((sv->name = strdup(name)) == NULL) {
	    free(sv);
	    return;
	}
	
	if (isdigit(*name)) {
	    if ((sv->addr = strdup(name)) == NULL) {
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
	/* 	sv->domain = (char *) 0; ???? */
	sv->binddn = (char *) 0;
	sv->bindpwd = (char *) 0;
	sv->flags = SRV_FLAG_REF;
	sv->next = (_lsrv_t_ptr) 0;
	
	if (_referral_servers) sv_p->next = sv;
	else _referral_servers = sv;
    }
    
    if ((ref = (_lref_t_ptr) malloc(sizeof(_lref_t))) == NULL) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, "ldap: malloc failed\n");
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

_lsrv_t_ptr lookup_server(int fd)
{
    _lsrv_t_ptr     srv;
    _ld_t_ptr      d;
    
    d = _ld_root; 
    srv = d->sls;
    do {
	if (srv->fd == fd) return srv;
	srv = srv->next;
    } while (srv != d->sls);
    
    for (srv = _referral_servers; srv != NULL; srv = srv->next)
	if (srv->fd == fd) return srv;
    
    return NULL;
}

/* delete everything on the chain */
int arg_ld_free(_arg_ld_t_ptr res)
{
    _arg_ld_t_ptr ressave, restmp;
    int rc = 0;
    
    for (restmp = res; restmp; restmp = ressave) {
	ressave = restmp->next;
	rc = ldhcp_arg_ld_free(restmp);
    }
    return rc;
}

/* delete one entry on th result chain */
int ldhcp_arg_ld_free(_arg_ld_t_ptr restmp)
{
    if (!restmp)
	return 0;
    if (restmp->attr_name) free(restmp->attr_name);
    if (restmp->is_berval == ITM_BERVAL) {
	if (restmp->argv_bervals) 
	    ldap_value_free_len(restmp->argv_bervals);
    }
    else {
	if (restmp->argv_strvals) 
	    ldap_value_free(restmp->argv_strvals);
    }
    free(restmp);
    restmp = (_arg_ld_t_ptr)0;
    return 0;
}

/* remove all the entries that have subs_loc == num */
int ldhcp_skip_entry(_ld_t_ptr rq, int num)
{
    _arg_ld_t_ptr res, resprev;

    if (rq->f_result == NULL)
	return LDHCP_OK;

    res = rq->f_result;
    while (res && (res->entnum == num)) {
	rq->f_result = res->next;
	ldhcp_arg_ld_free(res);
	res = rq->f_result;
    }
    resprev = res;
    if (res) {
	for (res = res->next; res; res = res->next) {
	    if (res->entnum == num) {
		resprev->next = res->next;
		ldhcp_arg_ld_free(res);
		res = resprev;
		if (!res)
		    break;
	    }
	    else
		resprev = res;
	}
    }
    return LDHCP_OK;
}

int collect_results(_ld_t_ptr *rq, LDAP *ld, LDAPMessage *result)
{
    LDAPMessage	*e;
    char		**vals;
    struct berval	**bervals;
    int			num=0, skip_entry, rc;
    _lrqst_t_ptr	req;
    _litm_t_ptr		item;
    _lpair_t_ptr	r;
    _lfmt_t_ptr		fmt;
    
    if ((req = lookup_request(result->lm_msgid)) == NULL) {
	ldhcp_logprintf(LDHCP_LOG_HIGH, 
		      "ldap: collect_results: no request\n");
	ldap_msgfree(result);
	return LDHCP_ERROR;
    }
    
    *rq = req->rq;
    ldhcp_timeout_remove(*rq);
    req->toc--;
    ldhcp_logprintf(LDHCP_LOG_HIGH, 
		    "remove timeout (op,key,fnd): %d %s %d\n", 
		    req->op, req->key, req->toc);

    if (req->flags & REQ_FLAG_DROP) { 
	ldhcp_timeout_remove(*rq);
	req->toc--;
	ldhcp_logprintf(LDHCP_LOG_HIGH, 
			"remove timeout (drop): %s %d\n", 
			req->key, req->toc);
    }
    

    switch(req->op) {
    case LDAP_REQ_SEARCH:
	fmt = req->ent->format;
	
	for (e = ldap_first_entry(ld, result); e != NULL;
	     e = ldap_next_entry(ld, e)) {
	    
	    num++;
	    
	    for (item = fmt->item; item != NULL; 
		 item = item->next) {
		skip_entry = 1;
		if (item->type == ITM_BERVAL) {
		    bervals = ldap_get_values_len(ld, e, item->name);
		    if (bervals && bervals[0]->bv_val) {
			if ((ldhcp_append_result(rq, LD_SUCCESS, num, bervals, 1,
						 item->name))
			    != LDHCP_OK) goto _sr_err;
			skip_entry = 0;
		    }
		}
		else {
		    vals = ldap_get_values(ld, e, item->name);
		    if (vals && vals[0]) {
			
			if ((ldhcp_append_result(rq, LD_SUCCESS, num, vals, 0,
						 item->name)) 
			    != LDHCP_OK) goto _sr_err;
			skip_entry = 0;
		    }
		} 
		if (skip_entry) {
		    /* we cannot include this entry if this was a required
		     * item and it is not available */
		    skip_entry = 0;
		    for (r = req->ent->require; r; r = r->next)
			if (strcasecmp(r->key, item->name) 
			    == NULL) {
			    ldhcp_skip_entry(*rq, num);
			    skip_entry = 1;
			    num--;
			    break;
			}
		    if (skip_entry)
			break;	/* out of item loop */
		}
	    }
	}
	break;
    case LDAP_REQ_ADD:
    case LDAP_REQ_MODIFY:
    case LDAP_REQ_DELETE:
	rc = ldap_result2error(ld, result, 0);
	if (rc == LDAP_SUCCESS) {
	    num = 1;
	    (*rq)->f_status = LD_SUCCESS;
	}
	else {
	    (*rq)->f_status = LD_ERROR;
	    if ( (req->op == LDAP_REQ_DELETE) && (rc == LDAP_NO_SUCH_OBJECT) )
		;
	    else
		ldhcp_logprintf(LDHCP_LOG_CRIT, "collect_results (%s): %s", 
				req->table, ldap_err2string(rc));
	}
	break;
    default:
	ldhcp_logprintf(LDHCP_LOG_CRIT, "collect_results: unknown op %d",
			req->op);
	rc = ldap_result2error(ld, result, 0);
	if (rc == LDAP_SUCCESS) {
	    num = 1;
	    (*rq)->f_status = LD_SUCCESS;
	}
	else {
	    (*rq)->f_status = LD_ERROR;
	    ldhcp_logprintf(LDHCP_LOG_CRIT, "collect_results: %s", 
			    ldap_err2string(rc));
	}
	break;
    }    
    ldap_msgfree(result);
    
    remove_request(req);
    req->next = (_lrqst_t_ptr) 0;
    
    if (! num) {
	rc = refer_request(req);
	if (rc == -1) {
	    my_free_request(req);
	    (*rq)->f_status = LD_NOTFOUND;
	    return LDHCP_NEXT;
	} else
	    return LDHCP_CONTINUE;	
    }
    
    my_free_request(req);
    
    return LDHCP_OK;
    
 _sr_err:
    ldap_msgfree(result);
    if (vals) ldap_value_free(vals);
    if (bervals) ldap_value_free_len(bervals);
    ldhcp_logprintf(LDHCP_LOG_RESOURCE, "ldap: malloc failed\n");
    return LDHCP_ERROR;
}


int ldap_callback(_ld_t_ptr *rq, int fd)
{
    _lsrv_t_ptr 	sv;
    _lrqst_t_ptr	req;
    int		rc;
    LDAPMessage	*r;
    struct timeval	tv;
    
    dump_state(LDHCP_LOG_MAX);
    
    if ((sv = lookup_server(fd)) == NULL) {
	ldhcp_logprintf(LDHCP_LOG_MIN, 
			"ldap: unknown callback fd: %d\n", fd);
	return LDHCP_CONTINUE;
    }
    
    sv->time = time(0);
    
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    
    rc = ldap_result(sv->ldap, LDAP_RES_ANY, 1, &tv, &r);
    ldhcp_logprintf(LDHCP_LOG_HIGH, "ldap_callback: fd: %d result: %d\n", 
		    fd, rc);
    
    if (_ldap_referral_flag) {
	ldhcp_logprintf(LDHCP_LOG_LOW, 
			"received referral: %s\n", _ldap_referral);
	add_referrals(_ldap_referral, r->lm_msgid, sv->parent);
	_ldap_referral_flag = 0;
	free(_ldap_referral);
    }
    
    switch (rc) {
	
    case -1:
	/* assume an ldap server crash */
	ldhcp_callback_remove(fd);
	sv->status = SRV_ERROR;
	sv->time = time(0);
	sv->fd = -1;
	ldap_unbind(sv->ldap);
	req = sv->req;
	sv->req = (_lrqst_t_ptr) 0;
	sv->ldap = (LDAP *) 0;
	if (req) {
	    if (sv->flags && SRV_FLAG_REF) {
		for ( ; req != NULL; req = req->next) {
		    remove_request(req);
		    req->next = (_lrqst_t_ptr) 0;
		    rc = refer_request(req);
		    if (rc == -1) {
			add_orphans(req);
		    }
		}
	    } else
		place_requests(sv->parent, req, 1);
	    
	    return LDHCP_CONTINUE;
	}
	return LDHCP_ERROR;
	
    case 0:
	return LDHCP_CONTINUE;
	
    case 100:
	return LDHCP_ERROR;
	
    case LDAP_RES_SEARCH_RESULT: /* search */
    case LDAP_RES_ADD:		/* add */
    case LDAP_RES_MODIFY:	/* modify */
    case LDAP_RES_DELETE:	/* delete */
	return(collect_results(rq, sv->ldap, r));
    }
    
    return 0;
}

/*****************************************************
**
** timeout code
**
*****************************************************/

int drop_timeout(_ld_t_ptr *rq, ldhcp_times_t *to)
{
    _lrqst_t_ptr	req;
    
    req = to->t_clientdata;
    *rq = req->rq;
    
    ldhcp_timeout_remove(*rq);
    req->toc--;
    ldhcp_logprintf(LDHCP_LOG_HIGH, "remove timeout (drop): %s %d\n", 
		    req->key, req->toc);

    (*rq)->f_status = LD_NOTFOUND;
    ldhcp_logprintf(LDHCP_LOG_HIGH, "removing orphan: %s\n", req->key);
    remove_orphan(req);
    
    return LDHCP_NEXT;
}

int access_timeout(_ld_t_ptr *rq, ldhcp_times_t *to)
{
    _lrqst_t_ptr	req;
    _ld_t_ptr	dom;
    int		rc;
    
    req = to->t_clientdata;
    *rq = req->rq;
    
    ldhcp_logprintf(LDHCP_LOG_HIGH, "search_timeout: %s\n", req->key);

    dom = remove_request(req);

    if (req->flags & REQ_FLAG_REF) {
	ldhcp_timeout_remove(*rq);
	ldhcp_logprintf(LDHCP_LOG_HIGH, "remove timeout (search,to): %s\n",
			req->key);
	req->next = (_lrqst_t_ptr) 0;
	rc = refer_request(req);
	if (rc == -1) {
	    my_free_request(req);
	    (*rq)->f_status = LD_NOTFOUND;
	    return LDHCP_NEXT;
	} else {
	    return LDHCP_CONTINUE;
	}
    }
    
    dom->pos = dom->pos->next;
    req->next = (_lrqst_t_ptr) 0;
    return (place_requests(dom, req, 0));
}

/*****************************************************
**
** finding a server
**
*****************************************************/

int open_timeout(_ld_t_ptr *rq, ldhcp_times_t *to)
{
    _lsrv_t_ptr	sv;
    _lrqst_t_ptr	req;
    int		rc;
    
    sv = to->t_clientdata;
    ldhcp_logprintf(LDHCP_LOG_MIN, "open_timeout: %s\n", sv->addr);
    
    dump_state(LDHCP_LOG_MAX);
    
    *rq = sv->to_req->rq;
    sv->flags &= ~SRV_FLAG_OPENTO;
    ldhcp_timeout_remove(*rq);
    sv->to_req->toc--;
    ldhcp_logprintf(LDHCP_LOG_HIGH, "remove timeout (open): %s %d\n", 
		    sv->to_req->key, sv->to_req->toc);
    
    ldhcp_callback_remove(sv->fd);
    
    sv->status = SRV_ERROR;
    sv->time = time(0);
    sv->fd = -1;
    
    if (sv->flags & SRV_FLAG_REF) {
	for (req = sv->req; req != NULL; req = req->next) {
	    remove_request(req);
	    req->next = (_lrqst_t_ptr) 0;
	    rc = refer_request(req);
	    if (rc == -1) {
		add_orphans(req);
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
    
    return LDHCP_CONTINUE;
}

int open_server(_lsrv_t_ptr sv, int eto) 
{
    int		rc, loop = 1;
    LDAP		*ld = NULL;
    LDAPMessage	*r;
    struct timeval	tv;
    
    ldhcp_logprintf(LDHCP_LOG_HIGH, "open_server %s, status: %d\n", 
		    sv->addr, sv->status);
    
    while (loop) {
	loop = 0;
	switch (sv->status) {
	    
	case SRV_UNBOUND:
	    if ((ld = ldap_open(sv->addr, sv->port)) == NULL) {
		rc = -1;
		break;
	    }
	    sv->ldap = ld;
	    sv->ldap->ld_sb.sb_options 
		|= LBER_NO_READ_AHEAD;
	    sv->fd = ld->ld_sb.sb_sd;
	    if (ld->ld_defconn->lconn_status 
		== LDAP_CONNST_CONNECTING) {
		ldhcp_logprintf(LDHCP_LOG_LOW, 
			      "ldap: new open_callback\n");
		ldhcp_callback_new(sv->fd, open_callback,
				 LDHCP_WRITE);
		rc = -2;
		sv->status = SRV_WAITING;
	    } else {
		sv->bindid = ldap_simple_bind(ld, sv->binddn, 
					      sv->bindpwd);
		sv->status = SRV_CONNECTING;
		loop = 1;
	    }
	    break;
	    
	case SRV_WAITING:
	    rc = -2;
	    break;
	    
	case SRV_BINDING:
	    if ((sv->bindid = ldap_simple_bind(sv->ldap, 
					       sv->binddn, sv->bindpwd)) < 0) {
		rc = -1;
	    } else {
		ldhcp_callback_new(sv->fd, open_callback,
				   LDHCP_READ);
		ldhcp_logprintf(LDHCP_LOG_LOW, 
				"ldap: new open_callback\n");
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
	    if (rc == 97) {
		sv->status = SRV_CONNECTED;
		ldhcp_callback_new(sv->fd, ldap_callback, 
				   LDHCP_READ);
		ldhcp_logprintf(LDHCP_LOG_LOW, 
				"open_server: new callback: %d\n", 
				sv->fd);
		send_requests(sv);
		rc = 0;
	    } else if (rc == 0) {
		ldhcp_callback_new(sv->fd, open_callback,
				 LDHCP_READ);
		rc = -2;
	    }
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

_lsrv_t_ptr next_good_server(_ld_t_ptr d, _lrqst_t_ptr req)
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
	ldhcp_timeout_new(req->rq, req->open_timeout, open_timeout,
			  (void *) sv);
	req->toc++;
	ldhcp_logprintf(LDHCP_LOG_LOW, "new timeout (open): %s %d\n", 
			req->key, req->toc);
    }
    if (rc == 0 && (sv->flags & SRV_FLAG_OPENTO)) {
	sv->flags &= ~SRV_FLAG_OPENTO;
	ldhcp_timeout_remove(sv->to_req->rq);
	sv->to_req->toc--;
	ldhcp_logprintf(LDHCP_LOG_LOW, "remove timeout (open): %s %d\n", 
			sv->to_req->key, sv->to_req->toc);
	sv->to_req = (_lrqst_t_ptr) 0;
    }
    
    if (rc == -1) return NULL;
    else return sv;
}



int open_callback(_ld_t_ptr *rq, int fd)
{
	int 		rc, i, errs, size, status;
	_lsrv_t_ptr 	sv;
	_lrqst_t_ptr	req;

	ldhcp_callback_remove(fd);

	sv = lookup_server(fd);
	ldhcp_logprintf(LDHCP_LOG_HIGH, "ldap: open_callback: %d st: %d\n", 
		      fd, sv->status);
	*rq = sv->to_req->rq;

	sv->flags &= ~SRV_FLAG_OPENTO;
	ldhcp_timeout_remove(sv->to_req->rq);
	sv->to_req->toc--;
	ldhcp_logprintf(LDHCP_LOG_HIGH, "remove timeout (open): %s %d\n", 
		      sv->to_req->key, sv->to_req->toc);

	if (sv->status == SRV_WAITING) {
		errs = 0;
		size = sizeof(errs);
		i = getsockopt(fd, SOL_SOCKET, SO_ERROR, &errs, &size);
		if (i < 0) {
			ldhcp_logprintf(LDHCP_LOG_RESOURCE, 
				      "ldap: getsockopt failed\n");
			sv->status = SRV_ERROR;
			sv->time = time(0);
			goto _open_callback_err;
		}

		status = 0;
		if (errs || ioctl(fd, FIONBIO, (caddr_t)&status) == -1) {
			ldhcp_logprintf(LDHCP_LOG_RESOURCE, 
				      "ldap: connect failed\n");
			sv->status = SRV_ERROR;
			sv->time = time(0);
			goto _open_callback_err;
		}

		sv->status = SRV_BINDING;
		sv->ldap->ld_defconn->lconn_status = LDAP_CONNST_CONNECTED;
	} else
		sv->status = SRV_CONNECTING;

	rc = open_server(sv, sv->req->error_timeout);

	switch (rc) {

	case 0:
		break;

	case -2:
		sv->flags |= SRV_FLAG_OPENTO;
		ldhcp_timeout_new(sv->to_req->rq, sv->to_req->open_timeout, 
			open_timeout, (void *) sv);
		sv->to_req->toc++;
		ldhcp_logprintf(LDHCP_LOG_LOW, "new timeout (open): %s %d\n", 
			      sv->to_req->key, sv->to_req->toc);
		break;

	case -1:
_open_callback_err:
		if (sv->flags & SRV_FLAG_REF) {
			for (req = sv->req; req != NULL; req = req->next) {
				remove_request(req);
				req->next = (_lrqst_t_ptr) 0;
				rc = refer_request(req);
				if (rc == -1) {
					add_orphans(req);
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

	return LDHCP_CONTINUE;
}



/*
 * put requests in a log file that can be read by ldapmodify
 */

int
write_mod_ldif_value(FILE* fp, LDAPMod *mods)
{
    char	*buf, *bufp;
    unsigned long len, blen;
    int		i;
    char	*bval;
    
    for ( i = 0; mods->mod_bvalues != NULL &&
	      mods->mod_bvalues[i] != NULL; i++ ) {
	len = strlen( mods->mod_type );
	
	if (mods->mod_op & LDAP_MOD_BVALUES) {
	    blen = mods->mod_bvalues[i]->bv_len;
	    bval = mods->mod_bvalues[i]->bv_val;
	}
	else {
	    blen = strlen(mods->mod_values[i]);
	    bval = mods->mod_values[i];
	}
	len = LDIF_SIZE_NEEDED( len, blen) + 1;

	buf = malloc( len );
	if (buf == NULL) {
	    syslog(LOG_ERR, "Out of memory in ldap_ldif_log");
	    return -1;
	}
	
	bufp = buf;
	put_type_and_value( &bufp, mods->mod_type, bval, (int)blen);
	
	*bufp = '\0';
    
	fputs( buf, fp );
	
	free( buf );
    }
    return 0;
}


void
ldap_ldif_log(FILE *fp, int optype, char *dn, void* change, int flag)
{
    char    *newrdn;
    LDAPMod** mods;
    int i;

    if (fp == NULL)
	return;
    fprintf( fp, "dn: %s\n", dn );
    switch ( optype ) {
    case LDAP_REQ_MODIFY:
	mods = change;
	fprintf( fp, "changetype: modify\n" );
	for (i = 0; mods[i] != NULL; i++ ) {
	    switch ( mods[i]->mod_op & ~LDAP_MOD_BVALUES ) {
	    case LDAP_MOD_ADD:
		fprintf( fp, "add: %s\n", mods[i]->mod_type );
		break;
		
	    case LDAP_MOD_DELETE:
		fprintf( fp, "delete: %s\n", mods[i]->mod_type );
		break;
		
	    case LDAP_MOD_REPLACE:
		fprintf( fp, "replace: %s\n", mods[i]->mod_type );
		break;
	    }
	    
	    if (write_mod_ldif_value(fp, mods[i]) < 0)
		return;

	}
	fprintf( fp, "-\n" );
	break;
	
    case LDAP_REQ_ADD:
	mods = change;
	fprintf( fp, "changetype: add\n" );
	for (i = 0 ; mods[i] != NULL; i++ ) {
	    if (write_mod_ldif_value(fp, mods[i]) < 0)
		return;
	}
	break;
	
    case LDAP_REQ_DELETE:
	fprintf( fp, "changetype: delete\n" );
	break;
	
    case LDAP_REQ_MODRDN:
	newrdn = change;
	fprintf( fp, "changetype: modrdn\n" );
	fprintf( fp, "newrdn: %s\n", newrdn );
	fprintf( fp, "deleteoldrdn: %d\n", flag ? 1 : 0 );
    }
    fprintf( fp, "\n" );
    fflush(fp);
}    


/*****************************************************
**
** placing requests
**
*****************************************************/

int send_requests(_lsrv_t_ptr sv)
{
    _lrqst_t_ptr	req;
    char *base;
    int scope;

    for (req = sv->req; req != NULL; req = req->next) {
	base = (req->base) ? req->base: sv->base;
	scope = (req->ent->scope != -1) ? req->ent->scope: sv->scope;
	if (req->status == REQ_WAITING) {
	    if (req->num_sent > req->max_requests) {
		req->flags |= REQ_FLAG_DROP;
		remove_request(req);
		add_orphans(req);
	    } else {
		switch (req->op) {
		case LDAP_REQ_SEARCH:
		    req->msgid = ldap_search(sv->ldap, base, scope, 
					     req->filter, 
					     req->ent->format->attr, 0);
		    break;
		case LDAP_REQ_ADD:
		    req->msgid = ldap_add(sv->ldap, base, req->mods);
		    break;
		case LDAP_REQ_MODIFY:
		    req->msgid = ldap_modify(sv->ldap, base, req->mods);
		    break;
		case LDAP_REQ_DELETE:
		    req->msgid = ldap_delete(sv->ldap, base);
		    break;
		default:
		    ldhcp_logprintf(LDHCP_LOG_CRIT, 
				    "send_requests: Unknown op %d", req->op);
		}
		if (req->msgid < 0) {
		    req->status = REQ_SENT;
		    req->flags |= REQ_FLAG_DROP;
		    ldhcp_timeout_new(req->rq, 0, drop_timeout, 
				      (void *) req);
		    req->toc++;
		    ldhcp_logprintf(LDHCP_LOG_LOW, 
				    "new timeout (drop,ls): %s %d\n",
				    req->key, req->toc);
		} else {
		    req->status = REQ_SENT;
		    ldhcp_logprintf(LDHCP_LOG_LOW, 
				    "op: %d req: %s num_sent: %d\n", 
				    req->op, req->key, req->num_sent);
		    req->num_sent++;
		    ldhcp_timeout_new(req->rq, req->access_timeout,
				      access_timeout, (void *) req);
		    req->toc++;
		    ldhcp_logprintf(LDHCP_LOG_HIGH, 
				    "new timeout (search): %s %d\n", 
				    req->key, req->toc);
		}
	    }
	}
    }
    
    return 0;
}

int refer_request(_lrqst_t_ptr req)
{
    _lref_t_ptr	pos;
    int		rc = -1;
    
    req->flags |= REQ_FLAG_REF;
    
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
			ldhcp_timeout_new(req->rq, req->open_timeout, 
					open_timeout, (void *) pos->sv);
	}
	if (rc == 0 && (pos->sv->flags & SRV_FLAG_OPENTO)) {
	    pos->sv->flags &= ~SRV_FLAG_OPENTO;
	    ldhcp_timeout_remove(pos->sv->to_req->rq);
	    pos->sv->to_req = (_lrqst_t_ptr) 0;
	}
	
	if (rc != -1) {
	    req->ref_pos = pos;
	    add_requests(pos->sv, req);
	    if (pos->sv->status == SRV_CONNECTED) 
		send_requests(pos->sv);
	}
    }
    
    return rc;
}

int place_requests(_ld_t_ptr d, _lrqst_t_ptr req, int all)
{
    _lsrv_t_ptr	sv;
    _lrqst_t_ptr	req_s;
    
    ldhcp_logprintf(LDHCP_LOG_HIGH, "place_requests\n");
    
    for (req_s = req; req_s != NULL; req_s = req_s->next)
	if (req_s->status == REQ_SENT) {
	    ldhcp_timeout_remove(req_s->rq);
	    req_s->toc--;
	    ldhcp_logprintf(LDHCP_LOG_HIGH, 
			   "remove timeout (?,pr) %s %d\n", 
			   req_s->key, req_s->toc);
	}
    
    if ((sv = next_good_server(d, req)) == NULL) {
	if (all) {
	    add_orphans(req);
	    return LDHCP_ERROR;
	}
	req->rq->f_status = LD_NOTFOUND;
	add_orphans(req->next);
	my_free_request(req);
	return LDHCP_NEXT;
    }
    add_requests(sv, req);
    if (sv->status == SRV_CONNECTED) send_requests(sv);
    return LDHCP_CONTINUE;
}


static char*
substitute_strings(char *format, int num, int len, char **in_args, int *pos)
{
    char *f, *fp, *p;
    int total_len, i;
 
    if (!format)
	return (char*)0;

    if (num == 0) {
	f = strdup(format);
	if (f == NULL) {
	    syslog(LDHCP_LOG_RESOURCE, "substitute_strings: error in malloc");
	    return 0;
	}
    }
    else {
	/* compute the total length of the result */
	total_len = len;
	for (i = *pos; i < (*pos + num); i++) 
	    if (in_args[i] && *(in_args[i]))
		total_len += (strlen(in_args[i]) - 2);
	    else
		total_len -= 1;/* '*' (1-2) */

	f = malloc(total_len+1);
	if (f == NULL) {
	    syslog(LDHCP_LOG_RESOURCE, "substitute_strings: error in malloc");
	    return 0;
	}
	bzero(f, total_len+1);
	for (fp = f, p = format; *p; p++) {
	    if (*p == '%') {
		/* if we don't have additional substitutions we just add 
		 * a '*' 
		 * substitution and don't bump the position 'pos' */
		if (in_args[*pos] && *(in_args[*pos])) {
		    strcpy(fp, in_args[*pos]);
		    fp += strlen(in_args[*pos]);
		    (*pos)++;
		}
		else {
		    strcpy(fp, "*");
		    fp += 1;
		}
		p++;
	    }
	    else {
		*fp = *p;
		fp++;
	    }
	}
	
    }
    return f;
}


LDAPMod **insert_attr_names(_lfmt_t_ptr fmt, LDAPMod **modArgs)
{
    _litm_t_ptr		item;
    int i;

    for (item = fmt->item, i = 0; item != NULL; 
	 item = item->next, i++) {
	if (modArgs[i] == NULL)
	    goto _mo_err;
	modArgs[i]->mod_type = item->name;
    }

    return modArgs;
 _mo_err:
    return NULL;
}

_lrqst_t_ptr make_request(_ld_t_ptr ldp, char *table, int op,
			  char **in_args, LDAPMod **modArgs, int l)
{
    _lrqst_t_ptr 	r;
    _lent_t_ptr	ent;
    char		*f;
    int		num, len, pos_subs;
    
    if ((r = (_lrqst_t_ptr)malloc(sizeof(_lrqst_t))) == NULL) {
	ldhcp_logprintf(LDHCP_LOG_RESOURCE, 
		     "ldap: make_request: malloc failed\n");
	return NULL;
    }
    
    r->table 	= strdup(table);
    r->msgid 	= -1;
    r->toc	= 0;
    r->num_sent = 0;
    r->status 	= REQ_WAITING;
    r->filter	= (char *) 0;
    r->ref_list = (_lref_t_ptr) 0;
    r->ref_pos 	= (_lref_t_ptr) 0;
    r->flags 	= 0;
    r->base	= (char*)0;
    r->ent 	= (_lent_t_ptr) 0;
    r->mods	= (LDAPMod **)0;
    if (in_args && in_args[0])
	r->key = strdup(in_args[0]); /* for printing only - debug */
    else
	r->key = (char *)0;
    r->next 	= (_lrqst_t_ptr) 0;
    
    for (ent = ldp->ent; ent != NULL; ent = ent->next) {
	if (strcasecmp(ent->name, r->table) == 0) {
	    if (ent->op != op)
		continue;
	    r->op = op;

	    if (l) {
		f = ent->filter_list;
		num = ent->list_num;
		len = ent->list_len;
	    } else {
		f = ent->filter_lookup;
		num = ent->lookup_num;
		len = ent->lookup_len;
	    }
	    

	    if ((op == LDAP_REQ_SEARCH) && (! f)) continue;

	    r->ent = ent;
	    
	    /* substitue the in args first in the base if it exists and then
	     * in the filter 
	     */
	    pos_subs = 0;
	    r->base = substitute_strings(ent->base, ent->base_num, 
					 ent->base_len, in_args, 
					 &pos_subs);
	    r->filter = substitute_strings(f, num, len, in_args, 
					   &pos_subs);
	    if (modArgs) {
		r->mods = insert_attr_names(ent->format, modArgs);
		if (r->mods == NULL)
		    goto _mr_err;
	    }
	    goto _mr_jump;
	}
    }
    
 _mr_jump:		
    if (! r->ent) goto _mr_err;
    if (! r->table || (! l && ! r->ent) || 
	((op == LDAP_REQ_SEARCH) && (! r->filter))) goto _mr_err;
    
    return r;
    
 _mr_err:
    if (r->table)	free(r->table);
    if (r->key)		free(r->key);
    if (r->filter) 	free(r->filter);
    if (r->base)	free(ent->base);
    free(r);
    
    return NULL;
}

/*
** This routine is called from within the central select loop in
** main when a timeout has occurred.  In here we pop the timeout
** of the top of the list, and call the callback associated with
** it.  Then we spin down the callout list until we have an answer.
*/
static int
ldhcp_timeout_loop(void)
{
    ldhcp_times_t *tc;
    _ld_t_ptr rq = 0;
    int status;
    
    /*
    ** The select timed out, so one of the callouts must
    ** have put something on the timeout queue.  We call
    ** the timeout handler to deal with it.
    */
    tc = ldhcp_timeout_next();
    if (tc && tc->t_proc) {
	status = (*tc->t_proc)(&rq, tc);
	free(tc);
	if (! rq) {
	    return status;
	}
    }	
    
    return LDHCP_OK;
}

static int
ldhcp_callback_loop(int count, fd_set *readset, fd_set *exceptset,
		  fd_set *sendset)
{
    _ld_t_ptr rq;
    int i, status;
    ldhcp_callback_proc *bp;
    char buf[BUFSIZ];
    
    /*
    ** We have work to do.  We just call the callbacks
    ** associated with the set file descriptors.
    */
    for (i = 0; (i < ldhcp_maxfd) && (count > 0); i++) {
	if (FD_ISSET(i, readset) || FD_ISSET(i, exceptset) ||
	    FD_ISSET(i, sendset)) {
	    bp = ldhcp_callback_get(i);
	    if (!bp) {
		/*
		** We have no callback on this file
		** descriptor so we just read off the
		** waiting data and continue.
		*/
		read(i, buf, sizeof(buf));
		continue;
	    }
	    rq = (_ld_t_ptr)0;
	    count--;
	    ldhcp_logprintf(LDHCP_LOG_HIGH,
			  "incoming packet on %d\n", i);
	    status = (*bp)(&rq, i);
	    if (! rq) {
		continue;
	    }
	}    
    }
    return status;
}


int
ldhcp_reply(void)
{
    int count, status;
    struct timeval *timeout = 0;
    struct timeval def_timeout = {1, 0};
    fd_set readset, sendset, expset;
    status = LDHCP_OK;
    

    for (;;) {
	readset = ldhcp_readset;
	expset = ldhcp_exceptset;
	sendset = ldhcp_sendset;

#ifdef DEBUG
	if (timeout) {
	    ldhcp_logprintf(LDHCP_LOG_MAX, "maxfd: %d, tv_sec: %d, tv_usec: %d\n",
			    ldhcp_maxfd, timeout->tv_sec, timeout->tv_usec);
	} else {
	    ldhcp_logprintf(LDHCP_LOG_MAX, "maxfd: %d, timeout: %x\n", ldhcp_maxfd,
			    timeout);
	}
#endif
	if (timeout == (struct timeval *)0) {
	    timeout = &def_timeout;
	}
	    
	count = select(ldhcp_maxfd, &readset, 0, 0, timeout);
	switch (count) {
	case -1:
	    /*
	    ** We got an error from select.  If this was from
	    ** a signal we just drop back into the select.  If
	    ** not then we exit.
	    */
	    if (errno != EINTR) {
		ldhcp_logprintf(LDHCP_LOG_RESOURCE, "error in select: %s\n",
				strerror(errno));
		exit(4);
	    }
	    break;

	case 0:
	    status = ldhcp_timeout_loop();
	    break;
	    
	default:
	    status = ldhcp_callback_loop(count, &readset, &expset, &sendset);
	    break;
	}
	/*
	** We set the timeout to the value of the first element in
	** the timeout list.
	*/
	timeout = ldhcp_timeout_set();
	if ((status == LDHCP_OK) && (_ld_root->f_status == LD_SUCCESS))
	    break;
	if ((status == LDHCP_OK) && (_ld_root->f_status == LD_NOTFOUND))
	    break;
    }
    return status;
}



int
ldap_op(int op_type, _ld_t_ptr ldp, char *ent_name, int list,
	_arg_ld_t_ptr *result, char **inArgs, LDAPMod **modArgs, int online)
{
    _lrqst_t_ptr r;
    int rc = LD_OK;

    if ((ldp == NULL) || (ent_name == NULL)) {
	ldp->f_status = LD_BADREQ;
	return LD_ERROR;
    }

    if ((r = make_request(ldp, ent_name, op_type, 
			  inArgs, modArgs, list)) == NULL) {
	ldp->f_status = LD_TRYAGAIN; /*  memory?  */
	if (modArgs)
	    ldap_mods_free(modArgs, 1);
	return LD_NEXT;
    }
    r->rq = ldp;
    r->error_timeout = ldp->error_timeout;
    r->open_timeout = 1000 * ldp->open_timeout;
    r->access_timeout = 1000 * ldp->access_timeout;
    r->max_requests = ldp->max_requests;
    if (online & LDAP_OP_ONLINE) {
	if (place_requests(ldp, r, 0) == LDHCP_ERROR) {
	    ldhcp_logprintf(LDHCP_LOG_HIGH, "ldap_op: Place requests error");
	    return LD_ERROR;
	}
	rc = ldhcp_reply();
	if (ldp->f_status == LD_SUCCESS) {
	    *result = ldp->f_result;
	    ldp->f_result = (_arg_ld_t_ptr)0;
	}
	else
	    rc = LD_ERROR;
    }
    if (online & LDAP_OP_OFFLINE) {
	ldap_ldif_log(ldp->ldiflog, op_type, r->base, r->mods, 0);
    }
    return  rc;
}

/*
 * this function will write the request to a file which can then be used to
 * update the LDAP database for DHCP offline
 */
/*
int
ldap_op_offline(struct POffLdap *pOffLdap, int op_type,  
		char *ent_name, int list,
		char **inArgs, LDAPMod **modArgs)
{
    int i;
    char *in_arg;
    char buf[256];
    char *newbuf;
    char *pc;

    if ((pOffLdap == NULL) || (pOffLdap->fd == -1))
	return LD_ERROR;
    sprintf(pOffLdap->buf, "%s|%d|%d|", ent_name, op_type, list);
    len = strlen(pOffLdap->buf);
    pc = pOffLdap->buf[len];
    
    for (i = 0; inArg[i]; i++) {
	sprintf(buf, "%s|", inArg[i]);
	len += strlen(buf);
	if (len > pOffLdap->size)
	    pOffLdap->buf = resizeOffLdapBuf(pOffLdap);
	strcpy(
    }
*/
/*
** This routine just prints a formated message to either the log or
** the screen.
*/
/*VARARGS2*/
void
ldhcp_logprintf(int level, char *format, ...)
{
    struct timeval t;
    va_list ap;
    int priority ;
    
    if (ldhcp_level >= level) {
	if (ldhcp_silent) {
	    switch (ldhcp_level) {
	    case LDHCP_LOG_CRIT:
		priority=LOG_CRIT;
		break;
	    case LDHCP_LOG_RESOURCE:
		priority=LOG_ERR;
		break;
	    case LDHCP_LOG_OPER:
		priority=LOG_NOTICE;
		break;
	    case LDHCP_LOG_MIN:
	    case LDHCP_LOG_LOW:
	    case LDHCP_LOG_HIGH:
	    case LDHCP_LOG_MAX:
	    default:
		priority=LOG_DEBUG;
	    }
	    va_start(ap, format);
	    vsyslog(priority, format, ap);
	    va_end(ap);
	} else {
	    (void) gettimeofday(&t, NULL);
	    (void) fprintf(stderr, "%19.19s: ", ctime(&t.tv_sec));
	    va_start(ap, format);
	    (void) vfprintf(stderr, format, ap);
	    va_start(ap, format);
	    (void) vfprintf(stderr, format, ap);
	    va_end(ap);
	    fflush(stderr);
	}
    }
}


void dump_state(int n)
{
    _lsrv_t_ptr	srv;
    _lrqst_t_ptr	req;
    _ld_t_ptr	d;
    
    d = _ld_root;
    srv = d->sls;
    do {
	ldhcp_logprintf(n, "srv: %s st: %d fd: %d\n", srv->addr, 
			srv->status, srv->fd);
	for (req = srv->req; req != NULL; req = req->next) {
	    ldhcp_logprintf(n, "req: %s %d %d\n", req->key, 
			    req->status, req->msgid);
	}
	srv = srv->next;
    } while (srv != d->sls);


    for (srv = _referral_servers; srv != NULL; srv = srv->next) {
	ldhcp_logprintf(n, "srv: %s st: %d fd: %d\n", srv->addr,
			srv->status, srv->fd);
	for (req = srv->req; req != NULL; req = req->next) {
	    ldhcp_logprintf(n, "req: %s %d %d\n", req->key,
			    req->status, req->msgid);
	}
    }
    
    if (_orphan_list) {
	ldhcp_logprintf(n, "orphans:\n");
	for (req = _orphan_list; req != NULL; req = req->next) {
	    ldhcp_logprintf(n, "req: %s %d %d\n", req->key,
			    req->status, req->msgid);
	}
    }
}

void add_requests(_lsrv_t_ptr sv, _lrqst_t_ptr req)
{
    _lrqst_t_ptr	req_r, req_s;
    
    for (req_r = req; req_r != NULL; req_r = req_r->next) {
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
** orphan specific code
**
*****************************************************/

void add_orphans(_lrqst_t_ptr req)
{
    _lrqst_t_ptr	req_s;
    
    if (req_s = _orphan_list) {
	while (req_s->next) req_s = req_s->next;
	req_s->next = req;
    } else
	_orphan_list = req;
    
    for (req_s = req; req_s != NULL; req_s = req_s->next) {
	req_s->flags |= REQ_FLAG_DROP;
	ldhcp_timeout_new(req_s->rq, 0, drop_timeout, (void *) req_s);
	req_s->toc++;
	ldhcp_logprintf(LDHCP_LOG_HIGH, "new timeout (drop,or): %s %d\n", 
		      req_s->key, req_s->toc);
	ldhcp_logprintf(LDHCP_LOG_LOW, "adding orphan: %s\n", req_s->key);
    }
}

void remove_orphan(_lrqst_t_ptr req)
{
    _lrqst_t_ptr	req_s;
    
    if (_orphan_list == req) 
	_orphan_list = req->next;
    else {
	req_s = _orphan_list;
	for (req_s = _orphan_list; req_s->next != NULL; 
	     req_s = req_s->next) {
	    if (req_s->next == req) {
		req_s->next = req->next;
		break;
	    }
	}
    }
    
    my_free_request(req);
}
