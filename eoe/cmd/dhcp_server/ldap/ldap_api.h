#ifndef _ldap_api_h

extern int	ldhcp_level;

/*
** Request result types.
*/
#define LD_SUCCESS      0       /* Everything is cool. */
#define LD_NOTFOUND     1       /* Key does not exist  */
#define LD_UNAVAIL      2       /* Could not talk to service */
#define LD_TRYAGAIN     3       /* Ran out of some resource. */
#define LD_BADREQ       4       /* The request was unparsable. */
#define LD_FATAL        5       /* Unrecoverable error. */

#define LD_RESULTS      6       /* Total number of result types. */


/*
** LDAP->DHCP return types.
*/
#define LD_ERROR       0       /* Something went wrong, return to client. */
#define LD_OK          1       /* We are finished, return to client. */
#define LD_CONTINUE    2       /* Go off and work on something else. */
#define LD_NEXT        4       /* Skip to next callout. */
#define LD_RETURN      8       /* Return to client with current results. */

/*
** DHCP logging levels
*/
#define LDHCP_LOG_CRIT    0       /* Critical system failures */
#define LDHCP_LOG_RESOURCE 1      /* Resource allocation failures */
#define LDHCP_LOG_OPER    2       /* Imporant operation state changes */
#define LDHCP_LOG_MIN     3       /* debugging */
#define LDHCP_LOG_LOW     4       /* debugging */
#define LDHCP_LOG_HIGH    5       /* debugging */
#define LDHCP_LOG_MAX     6       /* debugging */

extern void ldhcp_logprintf(int level, char *format, ...);

#ifndef _ldap_dhcp_h
typedef struct arg_ldap_dhcp *_arg_ld_t_ptr;
#endif

/* whats passed between the dhcp code and the ldap library  are args */
#define DHCPSUBNETLIST  "DHCPSubnetList"
#define SUBNETANDMASK	"subnetandmask"

typedef struct arg_ldap_dhcp {
    char *attr_name;		/* attribute to substitute or return */
    int entnum;			/* entry number of this attribute */
    int is_berval;		/* 1 = berval otherwise string */
    union {
	char **arg_strvals;
	struct berval **arg_bervals;
    } arg_vals;
    struct arg_ldap_dhcp *next;
} _arg_ld_t;
#define argv_strvals arg_vals.arg_strvals 
#define argv_bervals arg_vals.arg_bervals

/* the functions return result types */
extern int getSubnets(_arg_ld_t_ptr *subnetlist);
extern int getSubnetRangesBySubnetDn(_arg_ld_t_ptr *resArg, char **inargs);
extern int getSubnetConfigBySubnetDn(_arg_ld_t_ptr *result, char **subnetdn);
extern int getRangeConfigByRangeDn(_arg_ld_t_ptr *result, char **rangedn);
extern int getLeaseByCid(_arg_ld_t_ptr *result, char **cid);
extern int getLeaseByHostName(_arg_ld_t_ptr *result, char **cid);
extern int getLeaseByIpAddr(_arg_ld_t_ptr *result, char **ipaddr);
extern int addLease(_arg_ld_t_ptr *result, char **subArgs, LDAPMod **modArgs);
extern int updateLease(_arg_ld_t_ptr *result, char **subArgs, LDAPMod **modArgs);
extern int deleteLease(_arg_ld_t_ptr *result, char **subArgs);
extern int getExpiredLeases(_arg_ld_t_ptr *result, char **in);
extern int getReservationByCid(_arg_ld_t_ptr *result, char **in);
extern int getReservationByIpAddr(_arg_ld_t_ptr *result, char **in);
extern int getReservationByHostName(_arg_ld_t_ptr *result, char **in);

extern int parse_ldap_dhcp_config(char *ldap_conf_file);
extern void check_ldap_subnet_refresh(void);
extern int arg_ld_free(_arg_ld_t_ptr);
extern char *get_string(char *s);
#define _ldap_api_h
#endif /* _ldap_api.h */
