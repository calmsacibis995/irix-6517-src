#include <stdio.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <alloca.h>
#include "lber.h"
#ifdef _SSL
#include <bsafe.h>
#include <ssl.h>
#endif
#include "ldap.h"

long _count = 0;

int
ldap_msgfree(LDAPMessage *m)
{
	LDAPMessage *mn;	

	while (m) {
		mn = m->next;
	
		free(m->lm_ber->data);
		free(m->lm_ber);
		free(m);

		m = mn;
	}

	return 0;
}

char *
ldap_get_dn(LDAP *ld, LDAPMessage *e)
{
	struct ber	ber;
	obj_string	dn;
	char		*ps;

	ps = e->lm_ber->ptr;
	ber_get_sequence(&ber, e->lm_ber);
	ber_get_string(&dn, &ber, 0x04);
	e->lm_ber->ptr = ps;

	return dn.data;
}

int
ldap_get_result_code(LDAPMessage *e)
{
	char		*ps;
	int		ret;
	struct ber	ber;

	ps = e->lm_ber->ptr;
	ber_get_sequence(&ber, e->lm_ber);
	ber_get_int(&ret, &ber);
	e->lm_ber->ptr = ps;

	return ret;
}

int
ldap_get_referral(obj_string *ref, LDAPMessage *e)
{
	struct ber	ber, ber2;	
	
	ber_get_sequence(&ber, e->lm_ber);
	ber_skip_obj(&ber);	/* resultCode */
	ber_skip_obj(&ber);	/* matchedDN */
	ber_skip_obj(&ber);	/* errorMessage */
	ber_get_sequence(&ber2, &ber);
	ber_get_string(ref, &ber2, 0x04);

	return 0;
}

char **
ldap_get_values(LDAP *ld, LDAPMessage *e, char *key)
{
	struct ber	ber_t, ber, ber_p, ber_a;
	obj_string	k, a;
	char		*ps, **ret;
	int		n = 1, cnt = 0, i;

	ps = e->lm_ber->ptr;
	ber_get_sequence(&ber_t, e->lm_ber);
	ber_skip_obj(&ber_t);
	ber_get_sequence(&ber, &ber_t);

	while (ber_get_sequence(&ber_p, &ber) == 0) {
		if (ber_get_string(&k, &ber_p, 0x04) != 0) {
			nsd_logprintf(2, "ldap_get_values: parse error\n");
			goto _lgv_err;
		}
		if (strncasecmp(k.data, key, strlen(key)) == 0) {
			ret = (char **) malloc(5 * n * sizeof(char *));
			for (i = 5 * (n - 1); i < 5 * n; i++) 
				ret[i] = (char *) 0;

			ber_get_sequence(&ber_a, &ber_p);
			while (ber_get_string(&a, &ber_a, 0x04) == 0) {
				if (cnt == 5 * n - 1) {
					n++;
					ret = (char **) realloc(ret,
						5 * n * sizeof(char *));
					for (i = 5 * (n - 1); i < 5 * n; i++)
						ret[i] = (char *) 0;
				}
				ret[cnt++] = strdup(a.data);
				free(a.data);
			}
			e->lm_ber->ptr = ps;
			free(k.data);
			return ret;
		}
		free(k.data);
	}

_lgv_err:
	e->lm_ber->ptr = ps;
	return (char **) 0;
}

int ldap_value_free(char **v)
{
	int	i = 0;

	while (v[i]) free(v[i++]);
	free(v);

	return 0;
}

LDAPMessage *
ldap_first_entry(LDAP *ld, LDAPMessage *lm)
{
	if (lm->lm_tag == 0x65) lm = lm->next;
	if (! lm) return 0;
	return lm->lm_tag == 0x64 ? lm : 0;
}

LDAPMessage *
ldap_next_entry(LDAP *ld, LDAPMessage *lm, LDAPMessage *pe)
{
	if (! pe || ! pe->next) return 0;
	return pe->next->lm_tag == 0x64 ? pe->next : 0;
}

int
ldap_save_message(LDAP *ld, LDAPMessage *lm, int id)
{
	struct ldap_request *r, *rn;

	if (! ld->ld_req) {
		ld->ld_req = (struct ldap_request *) 
			malloc(sizeof(struct ldap_request));
		ld->ld_req->next = (struct ldap_request *) 0;
		ld->ld_req->id = id;
		ld->ld_req->mes = lm;
		ld->ld_req->ptr = lm;
		
		return 0;
	} 

	r = ld->ld_req;
	while (r) {
		rn = r;
		if (r->id == id) {
			r->ptr->next = lm;
			r->ptr = lm;

			return 0;
		}
		r = r->next;
	}

	rn->next = (struct ldap_request *) malloc(sizeof(struct ldap_request));
	rn = rn->next;
	rn->next = (struct ldap_request *) 0;
	rn->id = id;
	rn->mes = lm;
	rn->ptr = lm;

	return 0;
}

LDAPMessage *
ldap_get_messages(LDAP *ld, LDAPMessage *lm, int id)
{
	struct ldap_request	*req, *reqn;
	LDAPMessage		*ret;

	if (! ld->ld_req) return lm;

	if (ld->ld_req->id == id) {
		lm->next = ld->ld_req->mes;
		reqn = ld->ld_req->next;
		free(ld->ld_req);
		ld->ld_req = reqn;

		return lm;
	}

	req = ld->ld_req;
	while (req->next) {
		if (req->next->id == id) {
			lm->next = req->next->mes;
			reqn = req->next->next;
			free(req->next);
			req->next = reqn;
		
			return lm;
		}

		req = req->next;
	}

	return lm;
}

/*
** ldap_result
**
** returns:	-1	error
**		-2	would block
**		0	close
**		> 0	O.K., message type
**
** note: we break API in that we ignore timeval.  for now this is
** purely a non-blocking implimentation.  all = 1 requires saving
** state of request.
*/
int
ldap_result(LDAP *ld, int id, int all, struct timeval *tv, LDAPMessage **ret)
{
	LDAPMessage	*lm, *lms;
	struct ber	*ber;
	long		id_r;
	int		rc;

	nsd_logprintf(3, "ldap_result\n");

	lms = NULL;
	for (lm = ld->ld_mes; lm != 0; lms = lm, lm = lm->next) {
		nsd_logprintf(3, "ldap_result: saved: %d\n", lm->lm_id);
		if (id == LDAP_RES_ANY || (lm->lm_id == id)) {
			if (lms) lms->next  = lm->next;
			else 	 ld->ld_mes = lm->next;

			if (all && lm->lm_tag == 0x64) {
				nsd_logprintf(2, "LDAP: %d\n", _count++);
				ldap_save_message(ld, lm, lm->lm_id);
				*ret = (LDAPMessage *) 0;
				return lm->lm_tag;
			}

			*ret = ldap_get_messages(ld, lm, lm->lm_id);
			return lm->lm_tag;
		}
	}

	while (1) {
		ber = (struct ber *) malloc(sizeof(struct ber));

		rc = ber_read(&(ld->ld_sb), ber);
		nsd_logprintf(3, "ldap_result: ber_read: %d\n", rc);
		if (rc <= 0) return rc;

		lm = (LDAPMessage *) malloc(sizeof(LDAPMessage));
		ber_get_int(&id_r, ber);
		lm->lm_id = id_r;
		lm->lm_tag = ber_peek_tag(ber);
		lm->lm_id = id_r;
		lm->next = (LDAPMessage *) 0;
		lm->lm_ber = ber;	

		if (id == LDAP_RES_ANY || (lm->lm_id == id)) {
			if (all && lm->lm_tag == 0x64) {
				nsd_logprintf(2, "LDAP: %d\n", _count++);
				ldap_save_message(ld, lm, lm->lm_id);
				*ret = (LDAPMessage *) 0;
				return lm->lm_tag;
			}

			*ret = ldap_get_messages(ld, lm, lm->lm_id);
			return lm->lm_tag;
		}

		if (lms) lms->next  = *ret;
		else     ld->ld_mes = *ret;
		
		lms = *ret;
	}
}
