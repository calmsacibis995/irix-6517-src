#ifndef _configdefs_h
#define _configdefs_h

#define ERR_ILLEGAL_NETWORK_NUMBER	101
#define ERR_CANNOT_OPEN_CONFIG_DIR	102
#define ERR_CANNOT_OPEN_CONFIG_FILE	103

struct addr_range {
    int			r_low;
    int			r_high;
};

struct addr_pair {
    u_long		r_dest;
    u_long		r_router;
};

struct dhcp_config {
    int			serve_me;
    int			p_addrcnt;
    int			p_hstpfxcnt;
    u_long		p_netnum;
    u_long		p_netmask;
    u_long		p_broadcast;
    u_long		p_propel_addr;
    u_long		p_lease;
    int			p_choose_name;
    int			p_mtu;
    int			p_allnetsloc;
    int			p_domaskdisc;
    int			p_respmaskreq;
    u_long 		*p_router_addr;
    u_long		*p_tmsserver_addr;
    u_long		*p_dnsserver_addr;
    u_long		*p_nisserver_addr;
    char		*p_hostpfx;
    char		*p_dnsdomain;
    char		*p_nisdomain;
    struct addr_range	*p_addr_range;
    struct addr_pair	*p_routes;

    /* new options added */
    u_long		*p_logserver_addr;
    u_long		*p_cookieserver_addr;
    u_long		*p_LPRserver_addr;
    u_long		*p_resourceserver_addr;
    int			p_bootfile_size;
    u_long		p_swapserver_addr;
    int			p_IPforwarding;
    int			p_source_routing;
    struct addr_pair    *p_policy_filter;
    int			p_max_reassy_size;
    int			p_IP_ttl;
    u_long		p_pathmtu_timeout;
    int                 *p_pathmtu_table;
    int			p_router_disc;
    u_long		p_router_solicit_addr;
    int			p_trailer_encaps;
    u_long		p_arpcache_timeout;
    int			p_ether_encaps;
    int			p_TCP_ttl;
    u_long		p_TCP_keepalive_intrvl;
    int			p_TCP_keepalive_garbage;
    u_long		*p_NetBIOS_nameserver_addr;
    u_long		*p_NetBIOS_distrserver_addr;
    int			p_NetBIOS_nodetype;
    char		*p_NetBIOS_scope;
    u_long		*p_X_fontserver_addr;
    u_long		*p_X_displaymgr_addr;
    char		*p_nisplusdomain;
    u_long		*p_nisplusserver_addr;
    u_long		*p_mobileIP_homeagent_addr;
    u_long		*p_SMTPserver_addr;
    u_long		*p_POP3server_addr;
    u_long		*p_NNTPserver_addr;
    u_long		*p_WWWserver_addr;
    u_long		*p_fingerserver_addr;
    u_long		*p_IRCserver_addr;
    u_long		*p_StreetTalkserver_addr;
    u_long		*p_STDAserver_addr;

    int			p_time_offset;
    u_long		*p_nameserver116_addr;
    u_long		*p_impressserver_addr;
    char		*p_merit_dump_pathname;
    char		*p_root_disk_pathname;
    char		*p_extensions_pathname;
    u_long		*p_NTPserver_addr;
    char		*p_tftp_server_name;
    char		*p_bootfile_name;
    struct vendor_options *vop_options;
#ifdef EDHCP_LDAP
    struct subnetConfigLdap *subnetCfgLdap; /* configuration from LDAP */
#endif
#ifdef EDHCP
    void		*ddns_conf;
#endif
    struct dhcp_config	*p_next;
};

typedef struct dhcp_config dhcpConfig;

struct rfc1533_options {
    u_long		p_netmask;
    int			p_time_offset;
    u_long 		*p_router_addr;
    u_long		*p_tmsserver_addr;
    u_long		*p_nameserver116_addr;
    u_long		*p_dnsserver_addr;
    u_long		*p_logserver_addr;
    u_long		*p_cookieserver_addr;
    u_long		*p_LPRserver_addr;
    u_long		*p_impressserver_addr;
    u_long		*p_resourceserver_addr;
    int			p_bootfile_size;
    char		*p_merit_dump_pathname;
    char		*p_dnsdomain;
    u_long		p_swapserver_addr;
    char		*p_root_disk_pathname;
    char		*p_extensions_pathname;
    int			p_IPforwarding;
    int			p_source_routing;
    struct addr_pair    *p_policy_filter;
    int			p_max_reassy_size;
    int			p_IP_ttl;
    u_long		p_pathmtu_timeout;
    int                 *p_pathmtu_table;
    int			p_mtu;
    int			p_allnetsloc;
    u_long		p_broadcast;
    int			p_domaskdisc;
    int			p_respmaskreq;
    int			p_router_disc;
    u_long		p_router_solicit_addr;
    struct addr_pair	*p_routes;
    int			p_trailer_encaps;
    u_long		p_arpcache_timeout;
    int			p_ether_encaps;
    int			p_TCP_ttl;
    u_long		p_TCP_keepalive_intrvl;
    int			p_TCP_keepalive_garbage;
    char		*p_nisdomain;
    u_long		*p_nisserver_addr;
    u_long		*p_NTPserver_addr;
    u_long		*p_NetBIOS_nameserver_addr;
    u_long		*p_NetBIOS_distrserver_addr;
    int			p_NetBIOS_nodetype;
    char		*p_NetBIOS_scope;
    u_long		*p_X_fontserver_addr;
    u_long		*p_X_displaymgr_addr;
    char		*p_nisplusdomain;
    u_long		*p_nisplusserver_addr;
    u_long		*p_mobileIP_homeagent_addr;
    u_long		*p_SMTPserver_addr;
    u_long		*p_POP3server_addr;
    u_long		*p_NNTPserver_addr;
    u_long		*p_WWWserver_addr;
    u_long		*p_fingerserver_addr;
    u_long		*p_IRCserver_addr;
    u_long		*p_StreetTalkserver_addr;
    u_long		*p_STDAserver_addr;
};

typedef struct rfc1533_options rfc1533opts;

extern dhcpConfig *cf0_get_config(u_long, int flag);
extern void set_default_config_values(dhcpConfig *dhptr);

#ifdef EDHCP_LDAP
extern int cf0_initialize_config_ldap(void);
extern dhcpConfig* get_dhcpConfig_ldap(dhcpConfig*, u_long);
struct addr_range * getAddressRanges(char *str);
#endif

#ifdef EDHCP
extern int cf0_initialize_vendor_class(void);
#endif
extern dhcpConfig *configListPtr;

#endif /* _configdefs_h */
