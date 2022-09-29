#include <sys/types.h>
#include <regex.h>	/* regcomp(), regex_t: 06/99 gomez@engr.sgi.com */

#define LDAP_ERROR_TIMEOUT	5		/* seconds */
#define LDAP_OPEN_TIMEOUT	2		/* seconds */
#define LDAP_SEARCH_TIMEOUT	2		/* seconds */

#define LDAP_MAX_REQUESTS	3

#ifdef _SSL
#define LDAP_SECURITY_NONE	0
#define LDAP_SECURITY_SSL	1
#endif

typedef struct __lsrv_t *	_lsrv_t_ptr;
typedef struct __lrqst_t *	_lrqst_t_ptr;
typedef struct __ldm_t * 	_ldm_t_ptr;
typedef struct __lref_t *	_lref_t_ptr;

typedef struct __litm_t *	_litm_t_ptr;
typedef struct __lfmt_t *	_lfmt_t_ptr;
typedef struct __lmap_t *	_lmap_t_ptr;
typedef struct __lpair_t *	_lpair_t_ptr;

typedef struct __lref_t {
	_lsrv_t_ptr	sv;
	short		num;
	_lref_t_ptr	next;
} _lref_t;

typedef struct __lrqst_t {
	char		*key;
	char		*table;
	int		toc;
	nsd_file_t	*rq;

	int		search_timeout;
	int		open_timeout;
	int		error_timeout;
	int		max_requests;

  	short		status;
#define REQ_WAITING	0x00
#define REQ_SENT	0x01
#define REQ_REC		0x02
#define REQ_DONE	0x03

	short		ret_state;

	short		flags;
#define REQ_FLAG_REF	0x01
#define REQ_FLAG_DROP	0x02

	_ldm_t_ptr	dptr;		/* Need to know which domain JCGV */
	_lref_t_ptr	ref_list;
	_lref_t_ptr	ref_pos;
	_lmap_t_ptr	map;
	int		msgid;
	int		num_sent;
	char		*filter;
  	_lrqst_t_ptr	next;
} _lrqst_t;

typedef struct __lsrv_t {
	char		*domain;
	int		version;
	char		*name;
	char		*addr;
	char		*base;
	int		scope;
	u_short		port;
	char		*binddn;
	char		*bindpwd;
	LDAP		*ldap;
	int		fd;
	int		bindid;
	_lrqst_t_ptr	to_req;

	short		flags;
#define SRV_FLAG_OPENTO	0x01
#define SRV_FLAG_REF	0x02


	int		status;
#define SRV_UNBOUND	0x00
#define SRV_WAITING	0x01
#define SRV_BINDING	0x02
#define SRV_WAITING2	0x03
#define SRV_CONNECTING	0x04
#define SRV_CONNECTED	0x05
#define SRV_ERROR	0x06
#define SRV_SSL		0x07

	time_t		time;
	_lrqst_t_ptr	req;
	_ldm_t_ptr	parent;
	_lsrv_t_ptr	next;
} _lsrv_t;

typedef struct __litm_t {
	char		*name;
	char		*sep;

	short		flags;
#define ITEM_FLAG_PLUS	0x01
#define ITEM_FLAG_DISC	0x02
#define ITEM_FLAG_SKIP	0x04

	char		*alt;
	/* 06/99 gomez@engr.sgi.com 
	 * Added match & sub fields to support regexp substitution in
	 * attribute values
	 */
	regex_t		match;	/* Pattern to match in attr value */
	char		*sub;	/* Substitution for above match */
	_litm_t_ptr	next;
} _litm_t;

typedef struct __lfmt_t {
	char		*start;
	char		*end;
	char		**attr;

	short		flags;
#define FORMAT_FLAG_SINGLE	0x01

	_litm_t_ptr	item;
} _lfmt_t;

typedef struct __lpair_t {
	char		*key;
	_lpair_t_ptr	next;
} _lpair_t;

typedef struct __lmap_t {
	char		*name;
	int		lookup_num;
	int		lookup_len;
	int		list_num;
	int		list_len;
	_lfmt_t_ptr	format;
	char		*filter_list;
	char		*filter_lookup;
	char		*def;
	_lfmt_t_ptr	prefix;
	_lpair_t_ptr	require;
	_lmap_t_ptr	next;
} _lmap_t;



typedef struct __ldm_t {
	char        	*name;

	/* 06/99 gomez@engr.sgi.com
	 * Added this to support regex substitution on a per-domain
	 * basis. The substitution rules defined at the domain level 
	 * are only valid if no individual substitution rule was 
	 * defined for the attribute inside the format rule.
	 * The next three fields support the 
	 * 'regsub	Attribute_type	Match	Substitution'
	 * syntax in the configuration file
	 */
#define MAX_NUM_REGSUB	20
	char		*att_type[MAX_NUM_REGSUB];  /* Attribute type name */
	regex_t		match[MAX_NUM_REGSUB];	    /* Precomp regexp */
	char		*sub[MAX_NUM_REGSUB];	    /* Substitutions */
	unsigned int	regsub_ndx;	/* Next Free entry in above arrays */

	_lsrv_t_ptr	pos;
	_lmap_t_ptr	map;
	_lsrv_t_ptr	sls;
	_ldm_t_ptr	next;
} _ldm_t;

_lsrv_t_ptr alloc_server(void);
