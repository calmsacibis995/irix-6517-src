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

struct obj *ldap_make_filter(char *f, char **n)
{
	obj_string	*t_s0, *t_s1;
	struct obj	*m, *t0, *t1;
	char		*p, *ns;
	int		num, ts;

	if (*f++ != '(') return NULL;
	switch (*f) {
	
	case '&':
	case '|':
		t0 = (struct obj *) 0;
		ts = *f == '&' ? LDAP_FILTER_AND : LDAP_FILTER_OR;
		m = (struct obj *) malloc(sizeof(struct obj));
		ber_make_obj(m, LBER_CONTEXT | LBER_CONST | ts,
			LBER_CONST | LBER_TYPE_SEQ, t0, 0);

		num = 1;
		t1 = (struct obj *) 0;
		while (*(p = ++f) && num < 2) {
			if (*p == '(') num--;
			else if (*p == ')') num++;
			if (num == 0) {
				num = 1;
				t0 = ldap_make_filter(f, &ns);
				f = ns - 1;
				if (t1) t1->next = t0;
				else 	m->data = t0;
				t1 = t0;
			}
		}
		
		*n = p;
		break;

	default:
		t0 = (struct obj *) malloc(sizeof(struct obj));
		t1 = (struct obj *) malloc(sizeof(struct obj));

		t_s0 = (obj_string *) malloc(sizeof(obj_string));
		t_s1 = (obj_string *) malloc(sizeof(obj_string));

		p = strchr(f, '=');
		*p = (char) 0;
		t_s0->data = f;
		t_s0->len = strlen(t_s0->data);

		f = p + 1;
		p = strchr(f, ')');
		*p = (char) 0;
		t_s1->data = f;
		t_s1->len = strlen(t_s1->data);

		ber_make_obj(t0, LBER_TYPE_OCTET, 0, t_s0, t1);
		ber_make_obj(t1, LBER_TYPE_OCTET, 0, t_s1, 0);

		m = (struct obj *) malloc(sizeof(struct obj));
		ber_make_obj(m, LBER_CONTEXT | LBER_CONST | LDAP_FILTER_EQU,
			LBER_CONST | LBER_TYPE_SEQ, t0, 0);

		*n = p + 1;
	}
	
	return m;
}

int ldap_free_filter(struct obj *start, struct obj *end)
{
	struct obj *sv;

	while (start && start != end) {
		sv = start->next;
		switch (start->type) {

		case LBER_TYPE_OCTET:
			free(start->data);
			free(start);
			break;

		default:
			ldap_free_filter(start->data, NULL);
			free(start);
			break;

		}

		start = sv;
	}

	return 0;
}

int ldap_make_attrs(struct obj *a_o, char **a)
{
	obj_string	*t, *ts;
	struct obj	*t_o, *ts_o, *t_top;
	int		cnt = 0;

	ber_make_obj(a_o, LBER_CONST | LBER_TYPE_SEQ, 0, 0, 0);

	if (! a || ! *a || ! **a) return 0;

	ts = (obj_string *) malloc(sizeof(obj_string));
	ts_o = (struct obj *) malloc(sizeof(struct obj));
	t_top = ts_o;

	ts->len = strlen(a[cnt]);
	ts->data = a[cnt];
	ber_make_obj(ts_o, LBER_TYPE_OCTET, 0, ts, 0);
	while (a[++cnt]) {
		t = (obj_string *) malloc(sizeof(obj_string));
		t_o = (struct obj *) malloc(sizeof(struct obj));

		t->len = strlen(a[cnt]);
		t->data = a[cnt];
		ber_make_obj(t_o, LBER_TYPE_OCTET, 0, t, 0);

		ts_o->next = (struct obj *) t_o;
		ts_o = t_o;
	}

	a_o->data = (void *) t_top;

	return 0;
}

int
ldap_make_search(LDAP *ld, int id, char *b, int s, int da, 
	int sl, int tl, int to, char *f, char **at)
{
	struct obj	mes_o, pop_o, id_o, base_o;
	struct obj	scope_o, da_o, sl_o, tl_o, to_o;
	struct obj	*f_o, at_o;

	obj_string	base_s;

	char		*n, *f_s;
	int		rc;

	base_s.len = strlen(b);
	base_s.data = b;

	f_s = (char *) alloca(strlen(f) + 1);
	strcpy(f_s, f);

	f_o = ldap_make_filter(f_s, &n);
	ldap_make_attrs(&at_o, at);

	ber_make_obj(&mes_o, LBER_CONST | LBER_TYPE_SEQ, 0, &id_o, 0);
	ber_make_obj(&id_o, LBER_TYPE_INT, 0, &id, &pop_o);
	ber_make_obj(&pop_o, LBER_APP | LBER_CONST | LDAP_OP_SEARCH, 
		LBER_CONST | LBER_TYPE_SEQ, &base_o, 0);

	ber_make_obj(&base_o, LBER_TYPE_OCTET, 0, &base_s, &scope_o);
	ber_make_obj(&scope_o, LBER_TYPE_ENUM, 0, &s, &da_o);
	ber_make_obj(&da_o, LBER_TYPE_ENUM, 0, &da, &sl_o);
	ber_make_obj(&sl_o, LBER_TYPE_INT, 0, &sl, &tl_o);
	ber_make_obj(&tl_o, LBER_TYPE_INT, 0, &tl, &to_o);
	ber_make_obj(&to_o, LBER_TYPE_BOOL, 0, &to, &f_o);

	to_o.next = f_o;
	f_o->next = &at_o;
	at_o.next = (struct obj *) 0;

	rc = ber_write(ld->ld_sb, &mes_o);

	ldap_free_filter(f_o, &at_o);
	ldap_free_filter(at_o.data, NULL);
		
	return rc;
}

int 
ldap_search(LDAP *ld, char *b, int s, char *f, char **a, int a_only)
{
	long		id;

	id = ld->ld_id;
	ld->ld_id = ld->ld_id == LDAP_MAX_INT ? 0 : ld->ld_id + 1;

	if (ldap_make_search(ld, id, b, s, 0, 0, 0, 0, f, a) < 0) {
		perror("ldap_search");
		return -1;
	}

	return id;
}	
