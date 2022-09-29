#ifndef _dhcp_ldap_h
#define _dhcp_ldap_h

#include "configdefs.h"

extern int	ldap_config_on;	/* should we use ldap configs */
extern int	ldap_update_on;	/* should we update leases in ldap */

struct rangeConfigLdap {
    char *rangeDn;
    char *startip;
    char *endip;
    char **exclusions;
    char **optionvalues;
    dhcpConfig *cfg;
};
    
struct subnetConfigLdap {
    char *subnetDn;
    char *subnetIP;
    char *subnetMask;
    int numRangesLdap;
    struct rangeConfigLdap *rangeConfTableLdap; /* array of rangeConfigLdap */
    char **optionvalues;
};

struct DHCPServiceLdap {
    int numSubnetsLdap;
    struct subnetConfigLdap *subnetConfTableLdap;
    char **optiondefinitions;
    char **optionvalues;
};

extern struct DHCPServiceLdap *_DHCPServiceLdap;    

extern int update_subnet_options(dhcpConfig *dhptr, char **goptions);
extern void cf0_free_config_ldap(void);
extern dhcpConfig * cf0_get_config_ldap(u_long addr);
extern void *getAddressRangesLdap(u_long *numbers, int totnum);
extern dhcpConfig* cf0_get_subnet_config_copy(dhcpConfig* cfgsubnet);
extern dhcpConfig* cf0_get_subnet_config_copy(dhcpConfig* cfgsubnet);

#define DHCPOPTIONDEFINITIONS "dhcpoptiondefinitions"
#define DHCPOPTIONLIST  "dhcpoptionList"
#define SUBNETIPADDRESS "subnetipaddress"
#define SUBNETMASK "subnetmask"
#define DHCPRANGELIST "dhcprangelist"
#define STARTIPADDRESS "startipaddress"
#define ENDIPADDRESS "endipaddress"
#define EXCLUSIONS "exclusions"
#define IPADDRESS "ipaddress"
#define MACADDRESS "macaddress"
#define LEASEEXPIRATION "leaseexpiration"
#define CLIENTNAME "clientname"
#define UNIQUEID "uniqueid"
#endif
