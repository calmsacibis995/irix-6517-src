#ifndef _ldap_dhcp_h
#include <sys/time.h>
#include "configdefs.h"
/*
**  LDHCP return types.
*/
#define LDHCP_ERROR       0       /* Something went wrong, return to client. */
#define LDHCP_OK          1       /* We are finished, return to client. */
#define LDHCP_CONTINUE    2       /* Go off and work on something else. */
#define LDHCP_NEXT        4       /* Skip to next callout. */
#define LDHCP_RETURN      8       /* Return to client with current results. */

/*
** callback flags.
*/
#define LDHCP_READ        1
#define LDHCP_WRITE       2
#define LDHCP_EXCEPT      4


typedef struct ldap_dhcp *	_ld_t_ptr;
typedef struct __lsrv_t *	_lsrv_t_ptr;
typedef struct __lfmt_t *	_lfmt_t_ptr;
typedef struct __litm_t *	_litm_t_ptr;
typedef struct __lpair_t *	_lpair_t_ptr;
typedef struct __lent_t *	_lent_t_ptr;
typedef struct __lrqst_t *	_lrqst_t_ptr;
typedef struct __lref_t *	_lref_t_ptr;
#ifndef _ldap_api_h
typedef struct arg_ldap_dhcp *	_arg_ld_t_ptr;
#endif
typedef struct __lref_t {
    _lsrv_t_ptr		sv;
    short		num;
    _lref_t_ptr		next;
} _lref_t;

typedef struct __lrqst_t {
    int			op;
    char		*key;
    char		*table;
    int			toc;
    _ld_t_ptr		rq;
    
    int			access_timeout;
    int			open_timeout;
    int			error_timeout;
    int			max_requests;

    short		status;
#define REQ_WAITING	0x00
#define REQ_SENT	0x01
    
    short		flags;
#define REQ_FLAG_REF	0x01
#define REQ_FLAG_DROP	0x02
    
    _lref_t_ptr		ref_list;
    _lref_t_ptr		ref_pos;
    _lent_t_ptr		ent;
    int			msgid;
    int			num_sent;
    char		*base;
    char		*filter;
    LDAPMod		**mods;
    /*    LDAPMessage		*result; */
    _lrqst_t_ptr	next;
} _lrqst_t;

typedef struct __lsrv_t {
    char		*schema;
    char		*name;
    char		*addr;
    char		*base;
    int			scope;
    u_short		port;
    char		*binddn;
    char		*bindpwd;
    LDAP		*ldap;
    int			fd;
    int			bindid;
    _lrqst_t_ptr	to_req;	 
    
    short		flags;
#define SRV_FLAG_OPENTO	0x01
#define SRV_FLAG_REF	0x02
    
    
    int			status;
#define SRV_UNBOUND	0x00
#define SRV_WAITING	0x01
#define SRV_BINDING	0x02
#define SRV_WAITING2	0x03
#define SRV_CONNECTING	0x04
#define SRV_CONNECTED	0x05
#define SRV_ERROR	0x06
    
    time_t		time;
    _lrqst_t_ptr	req; 
    _ld_t_ptr		parent; 
    _lsrv_t_ptr		next;
} _lsrv_t;

typedef struct __litm_t {
    char		*name;
    int			type;
#define ITM_BERVAL	1
    _litm_t_ptr		next;
} _litm_t;

typedef struct __lfmt_t {
    char		*line;
    char		**attr;
    _litm_t_ptr		item;
} _lfmt_t;

typedef struct __lpair_t {
    char		*key;
    _lpair_t_ptr	next;
} _lpair_t;

typedef struct __lent_t {
    int			op;
    char		*name;
    int			lookup_num;
    int			lookup_len;
    int			list_num;
    int			list_len;
    int			base_num;
    int			base_len;
    _lfmt_t_ptr		format;
    char		*filter_list;
    char		*filter_lookup;
    char		*base;
    int			scope;
    _lpair_t_ptr	require;
    _lent_t_ptr		next;
} _lent_t;

typedef struct ldap_dhcp {
    char	*schema;	/* what schema is it DEN/NOVELL */
    int		config_ldap;	/* load from files(0), ldap(1) */
#define FILES_ONLY	0
#define FILES_AND_LDAP	1	
    int		subnet_refresh;	/* refreshes every n hours on the hour */
    int		range_refresh;	/* 0 = not used, 1 = used */
    int		leases_store;	/* 0 = use ndbm, 1 = use ldap */
    FILE	*ldiflog;	/* ldif log for offline storage */
    int		leases_online;	/* should we update online or not */
    int		open_timeout;	/* bind or connect wait time */
    int		access_timeout;	/* timeout for search, update, etc. */
    int		error_timeout;	/* server taken off round robin */
    int		max_requests;	/* max number of referrals */
    int		f_status;	/* status of request */
    int		f_tries;	/* number of tries */
    void	(*f_free)(void *); /* data free routine */
    _arg_ld_t_ptr f_result;	/* results in arg format */
    _lsrv_t_ptr	pos;		/* next server */
    _lsrv_t_ptr sls;		/* ldap server information */
    _lent_t_ptr ent;		/* ent information */
} _ld_t;

extern _ld_t_ptr _ld_root;	/* root to store config information */

/* timeout.c */

/*
** The time_list structure is a node in the timeout queue.  It contains
** an absolute time for when the timer will go off, and information
** about the callback routine to call.
*/
typedef struct ldhcp_times {
    void		    *t_clientdata;
    _ld_t_ptr		   t_file;
    int			    (*t_proc)(_ld_t_ptr *, struct ldhcp_times *);
    struct timeval	    t_timeout;
    struct ldhcp_times	      *t_next;
} ldhcp_times_t;

typedef int (ldhcp_timeout_proc)(_ld_t_ptr *, ldhcp_times_t *);

typedef struct lCacheEnt {
    int valid;			/* entry is valid ? */
#define DHCPLEASE		1
#define DHCPRESERVATION		2
    int class;			/* objectclass code */
    int type;			/* non zero if this is a NULL record
				 * indicating that there is no record */
#define CID 1
#define IPA 2
#define HNM 3
#define MAXCIDLEN 128
    char cid[MAXCIDLEN];
    u_long ipa;
    char hostname[128];
    char etherToIPLine[256];
} LCacheEnt;

typedef struct lCache {
    int sz;
    int nextInsert;
    LCacheEnt *lCacheEntp;
} LCache;

/*
 * ldap_op - online or offline or both
 */
#define LDAP_OP_ONLINE 1
#define LDAP_OP_OFFLINE 2

extern struct timeval *ldhcp_timeout_set(void);
extern ldhcp_times_t *ldhcp_timeout_next(void);
extern ldhcp_times_t *ldhcp_timeout_new(_ld_t_ptr rq, long, ldhcp_timeout_proc *, void *);
extern int ldhcp_timeout_remove(_ld_t_ptr);

/* callback.c */
typedef int (ldhcp_callback_proc)(_ld_t_ptr *, int);

ldhcp_callback_proc *ldhcp_callback_new(int, ldhcp_callback_proc *, unsigned);
int ldhcp_callback_remove(int);
ldhcp_callback_proc *ldhcp_callback_get(int);

/* ldap_ops.c */
extern int ldap_op(int op_type, _ld_t_ptr ldp, char *ent_name, int list,
		   _arg_ld_t_ptr *outArgs, char **inArgs, LDAPMod **modArgs,
		   int online);
extern int ldhcp_arg_ld_free(_arg_ld_t_ptr restmp);

/* ldap_cach.c */
extern LCache *makeLCache(int size);
extern int insertLCacheEntry(LCache*, int class, _arg_ld_t_ptr, char *s );
extern int insertLCacheEntryNULL(LCache*, int class, int type, void *s);
extern char* getLCacheEntry(LCache*, int class, int ktype, void *k, char *s,
			    int *rc);
extern void invalidateLCache(LCache *c);
extern int result2keys(_arg_ld_t_ptr result, char *cid, u_long *ipa, char *hnm);
extern char **result2optionvalues(_arg_ld_t_ptr result);
extern int insertLCacheEntries(LCache*, int, _arg_ld_t_ptr);

extern char *makeLeaseRecord(_arg_ld_t_ptr, char *, char*);
extern char *makeLeaseRecordWithCid(_arg_ld_t_ptr *, char *, char*, int*);
extern char *makeReservationRecord(_arg_ld_t_ptr , char *, char*);
dhcpConfig* getDHCPConfigForAddr(u_long, char **);
dhcpConfig* getReservationDhcpConfig(LCache* , u_long);
#define _ldap_dhcp_h
#endif /* _ldap_dhcp_h */
