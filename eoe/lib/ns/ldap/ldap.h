#define LDAP_OP_BIND_REQUEST	0x00
#define LDAP_OP_BIND_RESPONSE	0x01
#define LDAP_OP_UNBIND		0x02
#define LDAP_OP_SEARCH		0x03
#define LDAP_OP_SEARCH_ENTRY	0x04
#define LDAP_OP_SEARCH_DONE	0x05
#define LDAP_OP_SEARCH_REF	0x06
#define LDAP_OP_MDFY_REQ	0x07
#define LDAP_OP_MDFY_RES	0x08
#define LDAP_OP_ADD_REQ		0x09
#define LDAP_OP_ADD_RES		0x10

#define LDAP_PORT		389

#define LDAP_SCOPE_BASE		0x00
#define LDAP_SCOPE_ONELEVEL	0x01
#define LDAP_SCOPE_SUBTREE	0x02

#define LDAP_MAX_INT		2147483647

#define LDAP_TYPE_SEARCH	0x10

#define	LDAP_FILTER_AND		0x00
#define LDAP_FILTER_OR		0x01
#define LDAP_FILTER_EQU		0x03

#define LDAP_AUTH_SIMPLE	0x00
#define LDAP_AUTH_SASL		0x03

#define LDAP_RES_ANY		-1

#define LDAP_OK			0

#define LDAP_STAT_OK		0
#define LDAP_STAT_CONNECTING	1

typedef struct _LDAPMessage {
	struct ber		*lm_ber;
	int			lm_id;
	int			lm_tag;
	struct _LDAPMessage	*next;
} LDAPMessage;

struct ldap_request {
	int			id;
	struct _LDAPMessage	*mes;
	struct _LDAPMessage	*ptr;
	struct ldap_request	*next;
} ;

typedef struct _LDAP {
	sockbuf			ld_sb;
	LDAPMessage		*ld_mes;
	int			ld_id;
	int			ld_status;
	struct ldap_request	*ld_req;
} LDAP;

LDAP *ldap_init(char *addr, short port);
LDAP *ldap_ssl_init(char *addr, short port, int);
LDAPMessage *ldap_first_entry(LDAP *ld, LDAPMessage *lm);
LDAPMessage *ldap_next_entry(LDAP *ld, LDAPMessage *lm, LDAPMessage *pe);
char *ldap_get_dn(LDAP *ld, LDAPMessage *e);
char **ldap_get_values(LDAP *ld, LDAPMessage *e, char *key);
