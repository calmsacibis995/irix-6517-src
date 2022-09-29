#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dhcp_common.h"
#include <lber.h>
#include <ldap.h>
#include "dhcp_ldap.h"
#include "ldap_api.h"
#include "ldap_dhcp.h"
#include "configdefs.h"
#define BUF256 256
#ifndef MAXCIDLEN
#define MAXCIDLEN 128
#endif
char globalsbuf[BUF256];
extern struct ether_addr *ether_aton(char *);
extern void mk_cidstr(char *cid_ptr, int cid_length, char *str);
extern void mk_cid(char *cidstr, char *cid_ptr, int *cid_len);
extern int cf0_free_config_list(dhcpConfig *delptr);


/* reservation cache */
LCache *leaseLCache = NULL;
LCache *resvnLCache = NULL;
#define RESVNCSZ 64
#define LEASECSZ 1
int ldap_no_reservations = 0;
void populateResvnCache(void);
void populateLeaseCache(void);

/********************************************************
 * configure.c
 * loc0.c
 ********************************************************
 */

/*
 * configure.c
 */
struct DHCPServiceLdap *_DHCPServiceLdap = NULL;

void updateDhcpConfigForRange(struct subnetConfigLdap* sctl_p)
{
    struct rangeConfigLdap *rcl_p;
    dhcpConfig* cfg, *cfgsubnet;
    struct in_addr ia;

    for (rcl_p = sctl_p->rangeConfTableLdap; rcl_p->rangeDn; rcl_p++) {
	rcl_p->cfg = (dhcpConfig*)0;
	if (rcl_p && rcl_p->optionvalues) {
	    if (inet_aton(rcl_p->startip, &ia) == 0)
		return;
	    cfgsubnet = cf0_get_config_ldap(ia.s_addr);
	    if (cfgsubnet == NULL)
		return;
	    
	    if ((cfg = cf0_get_subnet_config_copy(cfgsubnet)) == NULL) {
		cfg = (dhcpConfig *)malloc(sizeof(dhcpConfig));
		bzero(cfg, sizeof(dhcpConfig));
		set_default_config_values(cfg);
		cfg->p_netnum = cfgsubnet->p_netnum;
		cfg->p_netmask = cfgsubnet->p_netmask;
	    }

	    if (_DHCPServiceLdap->optionvalues)
		update_subnet_options(cfg, _DHCPServiceLdap->optionvalues);
	    
	    if (cfgsubnet->subnetCfgLdap && 
		cfgsubnet->subnetCfgLdap->optionvalues)
		update_subnet_options(cfg, 
				      cfgsubnet->subnetCfgLdap->optionvalues);
	    
	    update_subnet_options(cfg, rcl_p->optionvalues);
	    rcl_p->cfg = cfg;
	}
    }
}

static int getRangeConfig(struct rangeConfigLdap *rangeCfg)
{
    struct rangeConfigLdap *rcl_p;
    _arg_ld_t_ptr result, a_result;
    char *in_arg[10];

    if (rangeCfg ==  NULL)
	return LD_OK;

    for (rcl_p = rangeCfg; rcl_p->rangeDn; rcl_p++) {
	in_arg[0] = rcl_p->rangeDn;
	in_arg[1] = (char*)0;
	result = 0;
	ldap_op(LDAP_REQ_SEARCH, _ld_root, "RangeConfigByRangeDn", 0,
		&result, in_arg, 0, LDAP_OP_ONLINE);
	if (result) {
	    for (a_result = result; a_result; a_result = a_result->next) {
		if (strcasecmp(a_result->attr_name, STARTIPADDRESS) == 0) {
		    rcl_p->startip = strdup(a_result->argv_strvals[0]);
		    if (rcl_p->startip == NULL) {
			ldhcp_logprintf(LDHCP_LOG_CRIT,
					"getRangeConfig: no memory");
			return LD_ERROR;
		    }
		} 
		else if (strcasecmp(a_result->attr_name, ENDIPADDRESS) == 0) {
		    rcl_p->endip = strdup(a_result->argv_strvals[0]);
		    if (rcl_p->endip == NULL) {
			ldhcp_logprintf(LDHCP_LOG_CRIT, "no memory");
			return LD_ERROR;
		    }
		} 
		else if (strcasecmp(a_result->attr_name, 
				    DHCPOPTIONLIST) == 0) {
		    rcl_p->optionvalues = a_result->argv_strvals;
		    a_result->argv_strvals = 0;
		}
		else if (strcasecmp(a_result->attr_name, EXCLUSIONS) == 0) {
		    rcl_p->exclusions = a_result->argv_strvals;
		    a_result->argv_strvals = 0;
		}
	    }
	    arg_ld_free(result);
	}
	else {
	    /* there is no range */
	    ldhcp_logprintf(LDHCP_LOG_MIN, 
			    "Range Config was not found dn: %s",
			    rcl_p->rangeDn);

	}
    }
    return LD_OK;
}


static int
getSubnetConfig(struct subnetConfigLdap *subnetCfg)
{
    struct subnetConfigLdap *scl_p;
    struct rangeConfigLdap *sclr_p;
    _arg_ld_t_ptr result, a_result;
    char *in_arg[10];
    char **range;

    if (subnetCfg == NULL)
	return LD_OK;

    for (scl_p = subnetCfg; scl_p->subnetDn; scl_p++) {
	in_arg[0] = scl_p->subnetDn;
	in_arg[1] = (char*)0;
	result = 0;
	ldap_op(LDAP_REQ_SEARCH, _ld_root, "SubnetConfigBySubnetDn", 0,
		&result, in_arg, 0, LDAP_OP_ONLINE);
	if (result) {
	    for (a_result = result; a_result; a_result = a_result->next) {
		if (strcasecmp(a_result->attr_name, SUBNETIPADDRESS) == 0) {
		    scl_p->subnetIP = strdup(a_result->argv_strvals[0]);
		    if (scl_p->subnetIP == NULL) {
			ldhcp_logprintf(LDHCP_LOG_CRIT,
					"getSubnetConfig: no memory");
			return LD_ERROR;
		    }
		} else if (strcasecmp(a_result->attr_name, SUBNETMASK) == 0) {
		    scl_p->subnetMask = strdup(a_result->argv_strvals[0]);
		    if (scl_p->subnetMask == NULL) {
			ldhcp_logprintf(LDHCP_LOG_CRIT,
					"getSubnetConfig: no memory");
			return LD_ERROR;
		    }
		} else if (strcasecmp(a_result->attr_name, DHCPOPTIONLIST)
			   == 0) {
		    scl_p->optionvalues = a_result->argv_strvals;
		    a_result->argv_strvals = 0;
		} else if (strcasecmp(a_result->attr_name, DHCPRANGELIST)
			   == 0) {
		    /* find out number of ranges */
		    for (range = a_result->argv_strvals; *range; range++) 
			scl_p->numRangesLdap++;
		    scl_p->rangeConfTableLdap = (struct rangeConfigLdap*)
			malloc(sizeof (struct rangeConfigLdap) *
			       (scl_p->numRangesLdap + 1));
		    if (scl_p->rangeConfTableLdap == NULL) {
			ldhcp_logprintf(LDHCP_LOG_CRIT,
					"getSubnetConfig: no memory");
			return LD_ERROR;
		    }
		    bzero(scl_p->rangeConfTableLdap, 
			  sizeof(struct rangeConfigLdap) *
			  (scl_p->numRangesLdap + 1));
		    /* now get configurations for ranges here */
		    sclr_p = scl_p->rangeConfTableLdap;
		    for (range = a_result->argv_strvals; *range; range++) {
			sclr_p->rangeDn = strdup(get_string(*range));
			if (sclr_p->rangeDn == NULL) {
			    ldhcp_logprintf(LDHCP_LOG_CRIT,
					    "getSubnetConfig: no memory");
			    return LD_ERROR;
			}
			++sclr_p;
		    }
		}
	    }
	    arg_ld_free(result);
	}
	else {
	    /* there was no subnet available */
	    ldhcp_logprintf(LDHCP_LOG_MIN, 
			    "Subnet Config was not found dn: %s",
			    scl_p->subnetDn);
	}
	getRangeConfig(scl_p->rangeConfTableLdap);
    }
    return LD_OK;
}

int 
complong(const void *a, const void* b)
{
    u_long x, y;
    x = *(u_long*)a;
    y = *(u_long*)b;

    if (x < y)
	return -1;
    else if (x > y)
	return 1;
    else 
	return 0;
}

    
void *
getAddressRangesLdap(u_long *numbers, int totnum)
{
    char tmpbuf[256];
    char atmpbuf[512];
    int i, len;

    atmpbuf[0] = NULL;
    for (i = 0; i < totnum; i+=2) {
	sprintf(tmpbuf, "%d-%d,", numbers[i],numbers[i+1]);
	strcat(atmpbuf, tmpbuf);
    }
    len = strlen(atmpbuf);
    atmpbuf[len - 1] = NULL;
    return(getAddressRanges(atmpbuf));
}

static int
updateRanges(struct subnetConfigLdap* sctl_p, dhcpConfig* cfg)
{
    int i;
    struct rangeConfigLdap *rcl_p;
    u_long *numbers;
    int num_excl = 0, totnum;
    char **excl, *ipaddr;
    struct in_addr ia;
    u_long netnum;
    int index;
    char tmpbuf[BUF256];

    /* find the total number of exclusions  */
    for (rcl_p = sctl_p->rangeConfTableLdap; rcl_p->rangeDn; rcl_p++) {
	if (rcl_p->exclusions) {
	    for (excl = rcl_p->exclusions; *excl; excl++) 
		num_excl++;
	}
    }
    totnum = (sctl_p->numRangesLdap + num_excl) * 2;
    numbers = (u_long*)malloc(sizeof(u_long) * totnum);
    if (numbers == NULL) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, "updateRanges: no memory");
	return LD_ERROR;
    }
	
    inet_aton(sctl_p->subnetIP, &ia);
    netnum = ia.s_addr;
    index = 0;
    for (i = 0, rcl_p = sctl_p->rangeConfTableLdap; 
	 i < sctl_p->numRangesLdap; i++, rcl_p++) {
	if (rcl_p->startip && rcl_p->endip) {
	    inet_aton(rcl_p->startip, &ia);
	    numbers[index++] = ia.s_addr - netnum;
	    inet_aton(rcl_p->endip, &ia);
	    numbers[index++] = ia.s_addr - netnum;
	    for (excl = rcl_p->exclusions; excl && *excl; excl++) {
		strcpy(tmpbuf, *excl);
		ipaddr = strtok(tmpbuf, " ,\t");
		if (ipaddr) {
		    inet_aton(ipaddr, &ia);
		    numbers[index++] = ia.s_addr - netnum - 1;
		    ipaddr = strtok(0, " ,\t");
		    inet_aton(ipaddr, &ia);
		    numbers[index++] = ia.s_addr - netnum + 1;
		}
	    }
	}
    }
    if (index > totnum) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, 
			"updateRanges: incorrect range spec %s", 
			sctl_p->subnetIP);
	exit(4);
    }
    if (index) {
	/* now sort the numbers */
	qsort(numbers, totnum, sizeof(u_long), complong);
	
	/* attach this bunch of numbers to the correct subnet  */
	if (cfg == NULL) {
	    ldhcp_logprintf(LDHCP_LOG_CRIT, "updateRanges: cfg NULL ??");
	    exit(4);
	}
	if (cfg->p_addr_range)
	    free(cfg->p_addr_range);
	cfg->p_addr_range = getAddressRangesLdap(numbers, totnum);
    }
    if (numbers) free(numbers);
    return LD_OK;
}
	
	

static int 
updateSubnetConfigLdap(struct subnetConfigLdap* sctl_p, char **goptions,
		       int gflag)
{
    dhcpConfig *cfg, **end, *cfgPtr;
    struct in_addr ia, mask;

    if (!sctl_p)
	return LD_OK;
    if (inet_aton(sctl_p->subnetIP, &ia) == 0)
	return LD_ERROR;
    cfg = cf0_get_config_ldap(ia.s_addr);
    if (cfg == NULL) {
	/* use the default cfg to generate one */
	cfg = (dhcpConfig *)malloc(sizeof(dhcpConfig));
	bzero(cfg, sizeof(dhcpConfig));
	set_default_config_values(cfg);
	cfg->p_netnum = ia.s_addr;
	/* add it to the end of the xoconfig list */
	for (end = &configListPtr, cfgPtr = configListPtr; cfgPtr;
	     end = &cfgPtr->p_next, cfgPtr = cfgPtr->p_next);
	*end = cfg;
    }
    if (sctl_p->subnetMask && (inet_aton(sctl_p->subnetMask, &mask) == 1)) {
	cfg->p_netmask = mask.s_addr;
	cfg->p_netnum = ia.s_addr & mask.s_addr;
    }
    update_subnet_options(cfg, goptions);
    /* the ranges of address that are allowed is to be updated  */
    if (!gflag) {
	updateRanges(sctl_p, cfg);
	cfg->subnetCfgLdap = sctl_p;
	updateDhcpConfigForRange(sctl_p);
    }
    return LD_OK;
}

void cf0_free_ranges(struct rangeConfigLdap *rcl_p)
{
    struct rangeConfigLdap* rctl_p;
    for (rctl_p = rcl_p; rctl_p->rangeDn; rctl_p++) {
	if (rctl_p->rangeDn) free (rctl_p->rangeDn);
	if (rctl_p->startip) free(rctl_p->startip);
	if (rctl_p->endip) free(rctl_p->endip);
	ldap_value_free(rctl_p->exclusions);
	ldap_value_free(rctl_p->optionvalues);
    }
    free(rcl_p);
}
void cf0_free_subnets(struct subnetConfigLdap* scl_p)
{
    struct subnetConfigLdap *sctl_p;
    dhcpConfig *pDConf;
    struct in_addr ia;
    
    for (sctl_p = scl_p; sctl_p->subnetDn; sctl_p++) {
	if (sctl_p->subnetDn) free(sctl_p->subnetDn);
	if (sctl_p->subnetIP) {
	    inet_aton(sctl_p->subnetIP, &ia);
	    pDConf = cf0_get_config_ldap(ia.s_addr);
	    free(sctl_p->subnetIP);
	}
	if (sctl_p->subnetMask) free(sctl_p->subnetMask);
	if (sctl_p->optionvalues) ldap_value_free(sctl_p->optionvalues);
	cf0_free_ranges(sctl_p->rangeConfTableLdap);
    }
    free(scl_p);
    if (pDConf)
	pDConf->subnetCfgLdap = NULL;
}

void
cf0_free_config_ldap(void)
{
    if (_DHCPServiceLdap) {
	if (_DHCPServiceLdap->subnetConfTableLdap) 
	    cf0_free_subnets(_DHCPServiceLdap->subnetConfTableLdap);
	ldap_value_free(_DHCPServiceLdap->optionvalues);
	ldap_value_free(_DHCPServiceLdap->optiondefinitions);
	free(_DHCPServiceLdap);
	_DHCPServiceLdap = NULL;
    }

}

int
cf0_initialize_config_ldap(void)
{
    _arg_ld_t_ptr result, aResult;
    char **subnet;
    struct subnetConfigLdap *sctl_p;

    /* load ldap config files if on */
    if (!ldap_config_on) 
	return LD_OK;

    _DHCPServiceLdap = (struct DHCPServiceLdap*)
	malloc(sizeof(struct DHCPServiceLdap));
    if (_DHCPServiceLdap == (struct DHCPServiceLdap*)0) {
	ldhcp_logprintf(LDHCP_LOG_RESOURCE, 
			"cf0_initialize_config: Error in malloc");
	exit(4);
    }
    _DHCPServiceLdap->numSubnetsLdap = 0;
    _DHCPServiceLdap->subnetConfTableLdap = (struct subnetConfigLdap*)0;
    _DHCPServiceLdap->optiondefinitions = 0;
    _DHCPServiceLdap->optionvalues = 0;
    /* find the subnets to be configured */
    result = 0;
    ldap_op(LDAP_REQ_SEARCH, _ld_root, "SUBNETS", 0, &result, 0, 0, 
	    LDAP_OP_ONLINE);
    if (result) {
	for (aResult = result; aResult; aResult = aResult->next) {
	    if (strcasecmp(aResult->attr_name, DHCPSUBNETLIST) == 0) {
		for (subnet = aResult->argv_strvals; *subnet; subnet++) 
		    _DHCPServiceLdap->numSubnetsLdap++;
	    }
	}
    }
    else {
	ldhcp_logprintf(LDHCP_LOG_CRIT, 
			"Could not get subnet configuration from ldap");
	ldhcp_logprintf(LDHCP_LOG_CRIT,
			"Fallback to files only ... Continuing");
	_ld_root->leases_store = 0;
	return LD_ERROR;
    }
    if (!_DHCPServiceLdap->numSubnetsLdap)
	return LD_OK;
    _DHCPServiceLdap->subnetConfTableLdap = 
	malloc((sizeof(struct subnetConfigLdap)) * 
	       (_DHCPServiceLdap->numSubnetsLdap + 1));
    if (_DHCPServiceLdap->subnetConfTableLdap == NULL) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, 
			"cf0_initialize_config_ldap: no memory");
	return LD_ERROR;
    }
    bzero(_DHCPServiceLdap->subnetConfTableLdap, 
	  ((sizeof (struct subnetConfigLdap)) * 
	   (_DHCPServiceLdap->numSubnetsLdap + 1)));
    sctl_p = _DHCPServiceLdap->subnetConfTableLdap;
    
    /* save the dn of the subnets */
    for (aResult = result; aResult; aResult = aResult->next) {
	if (strcasecmp(aResult->attr_name, DHCPSUBNETLIST) == 0) {
	    for (subnet = aResult->argv_strvals; *subnet; subnet++) {
		sctl_p->subnetDn = strdup(get_string(*subnet));
		if (sctl_p->subnetDn == NULL) {
		    ldhcp_logprintf(LDHCP_LOG_CRIT,
				    "cf0_initialize_config_ldap: no memory");
		    return LD_ERROR;
		}
		++sctl_p;
	    }
	}
	else if (strcasecmp(aResult->attr_name, DHCPOPTIONDEFINITIONS) == 0) {
	    if (_DHCPServiceLdap->optiondefinitions) {
		ldhcp_logprintf(LDHCP_LOG_OPER, 
				"cf0_initialize_config_ldap: dup in optiondefinitions");
		ldap_value_free(_DHCPServiceLdap->optiondefinitions);
	    }
	    _DHCPServiceLdap->optiondefinitions = aResult->argv_strvals;
	    aResult->argv_strvals = 0;
	}
	else if (strcasecmp(aResult->attr_name, DHCPOPTIONLIST) == 0) {
	    if (_DHCPServiceLdap->optionvalues) {
		ldhcp_logprintf(LDHCP_LOG_OPER, 
				"cf0_initialize_config_ldap: dup in optionsvalues");
		ldap_value_free(_DHCPServiceLdap->optionvalues);
	    }
	    _DHCPServiceLdap->optionvalues = aResult->argv_strvals;
	    aResult->argv_strvals = 0;
	}
    }
    arg_ld_free(result);
    
    /* now that we have the subnets get the ranges and options 
     * for each subnet and range related values */
    getSubnetConfig(_DHCPServiceLdap->subnetConfTableLdap);

    /* now that we have the configuration options
     * we can update the dhcpConfig structures
     */
    for (sctl_p = _DHCPServiceLdap->subnetConfTableLdap; 
	 sctl_p->subnetDn; sctl_p++) {
	/* update global options  */
	updateSubnetConfigLdap(sctl_p, _DHCPServiceLdap->optionvalues, 1);
	/* update subnet options */
	if (sctl_p->subnetIP && sctl_p->optionvalues) 
	    updateSubnetConfigLdap(sctl_p, sctl_p->optionvalues, 0);
    }
    /*    populateLeaseCache();*/
    populateResvnCache();
    return LD_OK;			/* ??????? */
}

struct rangeConfigLdap*
getRangeConfigLdap(struct rangeConfigLdap *rclp_p, u_long ipa)
{
    struct rangeConfigLdap* rcl_p;
    struct in_addr st, en;

    for (rcl_p = rclp_p; rcl_p->rangeDn; rcl_p++) {
	if (inet_aton(rcl_p->startip, &st) == 0)
	    continue;
	if (inet_aton(rcl_p->endip, &en) == 0)
	    continue;
	if ((ipa  >= st.s_addr) && ( ipa <= en.s_addr))
	    return rcl_p;
    }
    return NULL;
}

dhcpConfig* 
get_dhcpConfig_ldap(dhcpConfig* cfg, u_long addr)
{
    dhcpConfig *cfgptr;
    struct rangeConfigLdap *rcl_p;

    if ((cfg == (dhcpConfig*)0) || (addr == 0))
	return NULL;
    if (cfgptr = getReservationDhcpConfig(resvnLCache, addr))
	return cfgptr;
    if (cfg->subnetCfgLdap && cfg->subnetCfgLdap->rangeConfTableLdap) {
	rcl_p = getRangeConfigLdap(cfg->subnetCfgLdap->rangeConfTableLdap, 
				   addr);
	if (rcl_p)
	    return (rcl_p->cfg);
    }
    return NULL;
}
	
/*
 * dbm.c
 */

char *makeLeaseRecordWithCid(_arg_ld_t_ptr *result, char *sbuf, 
			     char *cid, int *cid_len)
{
    _arg_ld_t_ptr a_result;
    char *ipaddr, *lease, *hostname, *mac;
    int nument = -1;

    for (a_result = *result; a_result; a_result = a_result->next) {
	if (nument == -1)
	    nument = a_result->entnum;
	else if (nument != a_result->entnum) 
	    break;

	if (strcasecmp(a_result->attr_name, MACADDRESS) == 0) 
	    mac = a_result->argv_strvals[0];
	else if (strcasecmp(a_result->attr_name, IPADDRESS) == 0) 
	    ipaddr = a_result->argv_strvals[0];
	else if (strcasecmp(a_result->attr_name, CLIENTNAME) == 0)
	    hostname = a_result->argv_strvals[0];
	else if (strcasecmp(a_result->attr_name, LEASEEXPIRATION) == 0)
	    lease = a_result->argv_strvals[0];
	else if (strcasecmp(a_result->attr_name, UNIQUEID) == 0) {
	    mk_cid(a_result->argv_strvals[0], cid, cid_len);
	}
    }
    
    if (!sbuf)
	sbuf = globalsbuf;
    sprintf(sbuf, "%s\t%s\t%s\t%s", mac, ipaddr, hostname, lease);
    *result = a_result;
    return sbuf;
}

char *makeLeaseRecord(_arg_ld_t_ptr result, char *sbuf, char *key)
{
    _arg_ld_t_ptr a_result;
    char *ipaddr, *lease, *hostname, *mac;
    int nument = -1;

    for (a_result = result; a_result; a_result = a_result->next) {
	/* there should be only one entry */
	if (nument == -1)
	    nument = a_result->entnum;
	else if (nument != a_result->entnum) {
	    ldhcp_logprintf(LDHCP_LOG_CRIT, 
			    "More than one entry for key %s", key);
	    continue;
	}
	if (strcasecmp(a_result->attr_name, MACADDRESS) == 0) 
	    mac = a_result->argv_strvals[0];
	else if (strcasecmp(a_result->attr_name, IPADDRESS) == 0) 
	    ipaddr = a_result->argv_strvals[0];
	else if (strcasecmp(a_result->attr_name, CLIENTNAME) == 0)
	    hostname = a_result->argv_strvals[0];
	else if (strcasecmp(a_result->attr_name, LEASEEXPIRATION) == 0)
	    lease = a_result->argv_strvals[0];
    }
    if (!sbuf)
	sbuf = globalsbuf;
    sprintf(sbuf, "%s\t%s\t%s\t%s", mac, ipaddr, hostname, lease);
    return sbuf;
}

char *
getRecByCidLdap(char *sbuf, char *cid_ptr, int cid_length)
{
    char *in_arg[10];
    char str[256];
    _arg_ld_t_ptr result;
    char *s, *sl;
    int rc;

    mk_cidstr(cid_ptr, cid_length, str);
    in_arg[0] = str;
    in_arg[1] = 0;
    result = 0;
    sl = getLCacheEntry(leaseLCache, DHCPLEASE, CID, str, sbuf, &rc);
    if (rc == -1) {
	ldap_op(LDAP_REQ_SEARCH, _ld_root, "LeaseByCid", 0, 
		&result, in_arg, 0, LDAP_OP_ONLINE);
	if (result) {
	    s = makeLeaseRecord(result, sbuf, str);
	    insertLCacheEntry(leaseLCache, DHCPLEASE, result, s);
	    arg_ld_free(result);
	    return s;
	}
	else
	    insertLCacheEntryNULL(leaseLCache, DHCPLEASE, CID, str);
    }
    else
	return sl;
    return NULL;
}

char *
getRecByHostNameLdap(char *sbuf, char *hostname)
{
    char *in_arg[10];
    _arg_ld_t_ptr result;
    char *s, *sl;
    int rc;

    in_arg[0] = hostname;
    in_arg[1] = 0;
    
    result = 0;
    sl = getLCacheEntry(leaseLCache, DHCPLEASE, HNM, hostname, sbuf, &rc);
    if (rc == -1) {
	ldap_op(LDAP_REQ_SEARCH, _ld_root, "LeaseByHostName", 0, 
		&result, in_arg, 0, LDAP_OP_ONLINE);
	if (result) {
	    s = makeLeaseRecord(result, sbuf, hostname);
	    insertLCacheEntry(leaseLCache, DHCPLEASE, result, s);
	    arg_ld_free(result); 
	    return s;
	}
	else
	    insertLCacheEntryNULL(leaseLCache, DHCPLEASE, HNM, hostname);
    }
    else
	return sl;
    return NULL;
}  

char *
getRecByIpAddrLdap(char *sbuf, u_long *ipa)
{
    char *in_arg[10];
    _arg_ld_t_ptr result;
    struct in_addr ia;
    char *ipc, buf[64];
    char *s, *sl;
    int rc; 

    ia.s_addr = *ipa;
    ipc = inet_ntoa(ia);
    strcpy(buf, ipc);

    in_arg[0] = buf;
    in_arg[1] = 0;
    
    result = 0;
    sl = getLCacheEntry(leaseLCache, DHCPLEASE, IPA, ipa, sbuf, &rc);
    if (rc == -1) {
	ldap_op(LDAP_REQ_SEARCH, _ld_root, "LeaseByIpAddr", 0, 
		&result, in_arg, 0, LDAP_OP_ONLINE);
	if (result) {
	    s = makeLeaseRecord(result, sbuf, buf);
	    insertLCacheEntry(leaseLCache, DHCPLEASE, result, s);
	    arg_ld_free(result);
	    return s;
	}
	else
	    insertLCacheEntryNULL(leaseLCache, DHCPLEASE, IPA, ipa);
    }
    else
	return sl;
    return NULL;
}  

void *
ldhcp_malloc( size_t size)
{
    void *mem;
    if ( (mem = malloc(size)) == NULL ) {
	ldhcp_logprintf(LDHCP_LOG_CRIT,
			"ldhcp_malloc: Non enough memory");
	exit(4);
    }
    return (mem);
}

void
addLeaseLdap(char *cid_ptr, int cid_len, char* frmted_data)
{
    char sbuf[256];
    char *eh, *ipc, *hna, *hlease;
    dhcpConfig * dhptr;
    struct in_addr ia;
    _arg_ld_t_ptr result;
    char *inArg[3];
    LDAPMod *cidMod, *ipaddMod, *leaseEMod, 
	*leaseSMod, *macMod, *hostNMod, *leaseTMod, *ocMod;
    char **cidVals, **ipaddVals, **leaseEvals, **leaseSvals,
	**macVals, **hostNVals, **leaseTVals, **ocVals;
    LDAPMod **modArgs;
    char str[64];
    int online;
    int i;

#define NUMADDLEASEMODS 9

    if ( !(_ld_root && _ld_root->leases_store) )
	return;
    strcpy(sbuf, frmted_data);
    if(parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &hlease)) 
	return ; /*error*/

    inet_aton(ipc, &ia);
    dhptr = cf0_get_config_ldap(ia.s_addr);
    if (dhptr == NULL) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, 
			"addLeaseLdap: Can't find subnet for addr %s", ipc);
	exit(4);
    }
    if (!dhptr->subnetCfgLdap)
	return;
    
    inArg[0] = ipc;
    inArg[1] = dhptr->subnetCfgLdap->subnetDn;
    inArg[2] = 0;
    
    modArgs = (LDAPMod **)ldhcp_malloc(sizeof(LDAPMod *) * NUMADDLEASEMODS );
    for (i = 0; i < (NUMADDLEASEMODS - 1); i++) 
	modArgs[i] = (LDAPMod *)ldhcp_malloc(sizeof(LDAPMod));
	    
    cidMod = modArgs[0];
    mk_cidstr(cid_ptr, cid_len, str);
    cidVals = (char **)ldhcp_malloc(sizeof(char *) * 2);
    cidVals[0] = strdup(str);
    cidVals[1] = NULL;
    cidMod->mod_op = LDAP_MOD_REPLACE;
    cidMod->mod_values = cidVals;

    ipaddMod = modArgs[1] ;
    ipaddVals = (char **)ldhcp_malloc(sizeof(char *) * 2);
    ipaddVals[0] = strdup(ipc);
    ipaddVals[1] = NULL;
    ipaddMod->mod_op = LDAP_MOD_REPLACE;
    ipaddMod->mod_values = ipaddVals;

    leaseEMod = modArgs[2];
    leaseEvals = (char **)ldhcp_malloc(sizeof(char *) * 2);
    leaseEvals[0] = strdup(hlease);
    leaseEvals[1] = NULL;
    leaseEMod->mod_op = LDAP_MOD_REPLACE;
    leaseEMod->mod_values = leaseEvals;

    leaseSMod = modArgs[3];
    leaseSvals = (char **)ldhcp_malloc(sizeof(char *) * 2);
    leaseSvals[0] = strdup("1");
    leaseSvals[1] = NULL;
    leaseSMod->mod_op = LDAP_MOD_REPLACE;
    leaseSMod->mod_values = leaseSvals;

    macMod = modArgs[4];
    macVals = (char **)ldhcp_malloc(sizeof(char *) * 2);
    macVals[0] = strdup(eh);
    macVals[1] = NULL;
    macMod->mod_op = LDAP_MOD_REPLACE;
    macMod->mod_values = macVals;

    hostNMod = modArgs[5];
    hostNVals = (char **)ldhcp_malloc(sizeof(char *) * 2);
    hostNVals[0] = strdup(hna);
    hostNVals[1] = NULL;
    hostNMod->mod_op = LDAP_MOD_REPLACE;
    hostNMod->mod_values = hostNVals;

    leaseTMod = modArgs[6];
    leaseTVals = (char **)ldhcp_malloc(sizeof(char *) * 2);
    leaseTVals[0] = strdup("1");
    leaseTVals[1] = NULL;
    leaseTMod->mod_op = LDAP_MOD_REPLACE;
    leaseTMod->mod_values = leaseTVals;

    ocMod = modArgs[7];
    ocVals = (char **)ldhcp_malloc(sizeof(char *) * 3);
    ocVals[0] = strdup("top");
    ocVals[1] = strdup("DHCPLease");
    ocVals[2] = NULL;
    ocMod->mod_op = LDAP_MOD_REPLACE;
    ocMod->mod_values = ocVals;

    modArgs[8] = NULL;
    result = 0;
    online = _ld_root->leases_online;
    if (_ld_root->ldiflog)
	online |= LDAP_OP_OFFLINE;
    ldap_op(LDAP_REQ_ADD, _ld_root, "LeaseAdd", 0, &result, inArg, modArgs,
	    online);
    if (result) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, "addLeaseLdap: result should be null");
	arg_ld_free(result);
    }
}


void
deleteLeaseLdap(char *cid_ptr, int cid_len, u_long ipa)
{
    struct in_addr ia;
    dhcpConfig * dhptr;
    char *inArg[3];
    _arg_ld_t_ptr result;
    char ipc[64], *ips;
    int online;
    
    if (! (_ld_root && _ld_root->leases_store) )
	return;
    ia.s_addr = ipa;
    ips = inet_ntoa(ia);
    if (ips != NULL)
	strcpy(ipc, ips);
    else {
	ldhcp_logprintf(LDHCP_LOG_CRIT, "deleteLeaseLdap: unknown ipa %d", 
			ipa);
	exit(4);
    }
    dhptr = cf0_get_config_ldap(ipa);
    if (dhptr == NULL) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, 
			"deleteLeaseLdap: Can't find subnet for addr %s", ipc);
	exit(4);
    }
    if (!dhptr->subnetCfgLdap)
	return;
    inArg[0] = ipc;
    inArg[1] = dhptr->subnetCfgLdap->subnetDn;
    inArg[2] = 0;

    result = 0;
    online = _ld_root->leases_online;
    if (_ld_root->ldiflog)
	online |= LDAP_OP_OFFLINE;
    ldap_op(LDAP_REQ_DELETE, _ld_root, "LeaseDelete", 0,
	    &result, inArg, 0, online);
    if (result) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, "deleteLeaseLdap: result should be null");
	arg_ld_free(result);
    }
}

void
updateLeaseLdap(char *cid_ptr, int cid_len, char* ipc, long lease)
{
    struct in_addr ia;
    dhcpConfig * dhptr;
    char *inArg[3];
    _arg_ld_t_ptr result;
    LDAPMod *leaseEMod;
    char **leaseEvals;
    LDAPMod **modArgs;
    char str[32];
    int online;
    int i;

    if ( !( _ld_root && _ld_root->leases_store) )
	return;
    if (inet_aton(ipc, &ia) == 0) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, "deleteLeaseLdap: unknown ipa %s", 
			ipc);
	return;
    }

    dhptr = cf0_get_config_ldap(ia.s_addr);
    if (dhptr == NULL) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, 
			"deleteLeaseLdap: Can't find subnet for addr %s", ipc);
	exit(4);
    }
    if (!dhptr->subnetCfgLdap)
	return;
    inArg[0] = ipc;
    inArg[1] = dhptr->subnetCfgLdap->subnetDn;
    inArg[2] = 0;

    modArgs = (LDAPMod **)ldhcp_malloc(sizeof(LDAPMod *) * 2 );
    for (i = 0; i < 1; i++) 
	modArgs[i] = (LDAPMod *)ldhcp_malloc(sizeof(LDAPMod));

    sprintf(str, "%d", lease);
    leaseEMod = modArgs[0];
    leaseEvals = (char **)ldhcp_malloc(sizeof(char *) * 2);
    leaseEvals[0] = strdup(str);
    leaseEvals[1] = NULL;
    leaseEMod->mod_op = LDAP_MOD_REPLACE;
    leaseEMod->mod_values = leaseEvals;
    modArgs[1] = NULL;

    result = 0;
    online = _ld_root->leases_online;
    if (_ld_root->ldiflog)
	online |= LDAP_OP_OFFLINE;
    ldap_op(LDAP_REQ_MODIFY, _ld_root, "LeaseModify", 0,
	    &result, inArg, modArgs, online);
    if (result) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, "updateLeaseLdap: result should be null");
	arg_ld_free(result);
    }
}


void
check_ldap_range_refresh(void)
{
    static int range_ref_time = 0;
    struct in_addr ia;
    struct subnetConfigLdap * sctl_p;
    dhcpConfig* cfg;

    range_ref_time++;
    if (_ld_root && (_ld_root->range_refresh != 0)) {
	/* 0 means never refresh  */
	if (range_ref_time % _ld_root->range_refresh == 0) {
	    for (sctl_p = _DHCPServiceLdap->subnetConfTableLdap;
		 sctl_p->subnetDn; sctl_p++) {
		cf0_free_ranges(sctl_p->rangeConfTableLdap);
		if (inet_aton(sctl_p->subnetIP, &ia) == 0)
		    return;
		cfg = cf0_get_config_ldap(ia.s_addr);
		updateRanges(sctl_p, cfg);
	    }
	}
    }
}

void
check_ldap_subnet_refresh(void)
{
    int secs;

    if (_ld_root) {
	cf0_free_config_ldap();
	if (cf0_initialize_config_ldap() == LD_ERROR) {
	    ldhcp_logprintf(LDHCP_LOG_CRIT, "Reloading Subnet Config Error");
	    return;
	}
	else
	    ldhcp_logprintf(LDHCP_LOG_CRIT, "Reloading Subnet Config Ok");
	secs = _ld_root->subnet_refresh * 3600;
	alarm(secs);
    }
}

_arg_ld_t_ptr
getExpiredLeasesLdap(time_t curr_time, dhcpConfig* dhptr)
{
    char *in_arg[3];
    _arg_ld_t_ptr result;
    char strtime[32];
    
    if (!dhptr->subnetCfgLdap)
	return NULL;
    sprintf(strtime, "%d", curr_time);
    in_arg[0] = dhptr->subnetCfgLdap->subnetDn;
    in_arg[1] = strtime;
    in_arg[2] = 0;
    
    result = 0;
    ldap_op(LDAP_REQ_SEARCH, _ld_root, "ExpiredLeases", 1, &result, in_arg, 0,
	    _ld_root->leases_online);
    
    return result;
}

_arg_ld_t_ptr
dhcp_nextlease(_arg_ld_t_ptr result, char *sbuf, char *cid, int *cid_len)
{
    if (result) {
	makeLeaseRecordWithCid(&result, sbuf, cid, cid_len);
    }
    return result;
	
}
/* find oldest expired ethernet & ipaddress pair
   returns 0 if found, 1 if error, or 2 if not found
   */
   
int
loc0_scan_find_lru_entry_ldap(EtherAddr *eth, u_long *ipa, char **hname, 
			      char *lru_cid, int *lru_cid_len, 
			      dhcpConfig* dhptr)
{
    char *eh, *ipc, *hna, *hlease;
    time_t lease_end;
    EtherAddr *ehs;
    time_t curr_time;
    time_t oldest_lease = LONG_MAX;
    char sbuf[256];
    _arg_ld_t_ptr leases, ldm;
    char tmpcid[MAXCIDLEN];
    int tmpcid_len;

    if (time(&curr_time) == (time_t)-1)
	return 1;		/* error */
    
    leases = getExpiredLeasesLdap(curr_time, dhptr);
    ldm = leases;  
    while (ldm = dhcp_nextlease(ldm, sbuf, tmpcid, &tmpcid_len)) {	
	
	if(parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &hlease)) {
	    return 1; /*error*/
	}
	lease_end = atol(hlease);
	if ((lease_end <= curr_time) && (lease_end > 0)) { /* expired entry */
	    /* -1 is static lease, -2 is stolen address  -3 is static */
	    if (lease_end < oldest_lease) {
		ehs = ether_aton(eh);
		if(!ehs)
		    return 1;
		bcopy(ehs, eth, sizeof(EtherAddr));
		*ipa = inet_addr(ipc);
		if(*ipa == INADDR_NONE)
		    return 1;
		if (*hname)
		    free(*hname);
		*hname = strdup(hna);
		*lru_cid_len = tmpcid_len;
		if (*lru_cid_len > (MAXCIDLEN-1)) {
		    ldhcp_logprintf(LDHCP_LOG_CRIT, 
				    "Cid too long len = %d", *lru_cid_len);
		    return 1;
		}
		bcopy(tmpcid, lru_cid, *lru_cid_len);
		oldest_lease = lease_end;
	    }
	}
    }
    arg_ld_free(leases);
    if (oldest_lease == LONG_MAX)
	return 2;
    else
	return 0;
}


char *makeReservationRecord(_arg_ld_t_ptr result, char *sbuf, char *key)
{
    _arg_ld_t_ptr a_result;
    char *ipaddr, *hostname;
    int nument = -1;
    char *mac;

    if (!result)
	return NULL;
    for (a_result = result; a_result; a_result = a_result->next) {
	/* there could be more than one entry */
	if (nument == -1)
	    nument = a_result->entnum;
	else if (nument != a_result->entnum) {
	    break;
	}
	if (strcasecmp(a_result->attr_name, MACADDRESS) == 0) 
	    mac = a_result->argv_strvals[0];
	else if (strcasecmp(a_result->attr_name, IPADDRESS) == 0) 
	    ipaddr = a_result->argv_strvals[0];
	else if (strcasecmp(a_result->attr_name, CLIENTNAME) == 0)
	    hostname = a_result->argv_strvals[0];
    }
    if (!sbuf)
	sbuf = globalsbuf;
    sprintf(sbuf, "%s\t%s\t%s\t%d", mac, ipaddr, hostname, STATIC_LEASE);
    return sbuf;
}


char *
getReservationByCidLdap(char *sbuf, char *cid_ptr, int cid_length)
{
    char str[256];
    char *sl;
    int rc;

    mk_cidstr(cid_ptr, cid_length, str);
    if (ldap_no_reservations)
	return NULL;
    sl = getLCacheEntry(resvnLCache, DHCPRESERVATION, CID, str, sbuf, &rc);
    return sl;
}

char *
getReservationByIpAddrLdap(char *sbuf, u_long *ipa)
{
    struct in_addr ia;
    char *ipc, buf[64];
    char *sl;
    int rc;

    if (ldap_no_reservations)
	return NULL;
    ia.s_addr = *ipa;
    ipc = inet_ntoa(ia);
    strcpy(buf, ipc);

    sl = getLCacheEntry(resvnLCache, DHCPRESERVATION, IPA, ipa, sbuf, &rc);

    return sl;
}  


char *
getReservationByHostNameLdap(char *sbuf, char *hostname)
{
    char *sl;
    int rc;

    if (ldap_no_reservations)
	return NULL;
    sl = getLCacheEntry(resvnLCache, DHCPRESERVATION, HNM, hostname, sbuf, &rc);
    return sl;
}  

void
invalidateLeaseCache(void)
{
    invalidateLCache(leaseLCache);
}

void
populateResvnCache(void)
{
    char *in_arg[10];
    _arg_ld_t_ptr result, a_result;
    int numentries;

    /* the reservation cache is built only once */
    
    in_arg[0] = 0;
    
    result = 0;
    ldap_op(LDAP_REQ_SEARCH, _ld_root, "Reservations", 1, 
	    &result, in_arg, 0, LDAP_OP_ONLINE);
    if (result) {
	for (a_result = result; a_result->next; a_result = a_result->next);
	numentries = a_result->entnum + 1;
	resvnLCache = makeLCache(numentries);
	insertLCacheEntries(resvnLCache, DHCPRESERVATION, result);
	arg_ld_free(result); 
    }
    else
	ldap_no_reservations = 1;
}

void
populateLeaseCache(void)
{
    char *in_arg[10];
    _arg_ld_t_ptr result;
    struct subnetConfigLdap* sctl_p;

    for (sctl_p = _DHCPServiceLdap->subnetConfTableLdap;
	 sctl_p->subnetDn; sctl_p++) {
	in_arg[0] = sctl_p->subnetDn;
	in_arg[1] = 0;
	
	result = 0;
	ldap_op(LDAP_REQ_SEARCH, _ld_root, "SubnetLeases", 1, 
		&result, in_arg, 0, _ld_root->leases_online);
	if (result) {
	    leaseLCache = makeLCache(LEASECSZ);
	    insertLCacheEntries(leaseLCache, DHCPLEASE, result);
	    arg_ld_free(result); 
	}
    }
}
    

/*
 * ldap_cach.c
 */
int result2keys(_arg_ld_t_ptr result, char *cid, u_long *ipa, char *hnm)
{
    _arg_ld_t_ptr a_result;
    int nument = -1;
    struct in_addr ia;
    int rc = 0;

    for (a_result = result; a_result; a_result = a_result->next) {
	/* there should be only one entry */
	if (nument == -1)
	    nument = a_result->entnum;
	else if (nument != a_result->entnum) {
	    break;
	}
	if (strcasecmp(a_result->attr_name, IPADDRESS) == 0) {
	    if (inet_aton(a_result->argv_strvals[0], &ia)) {
		*ipa = ia.s_addr;
		rc++;
	    }
	}
	else if (strcasecmp(a_result->attr_name, CLIENTNAME) == 0) {
	    strcpy(hnm, a_result->argv_strvals[0]);
	    rc++;
	}
	else if (strcasecmp(a_result->attr_name, UNIQUEID) == 0) {
	    strcpy(cid,a_result->argv_strvals[0]);
	    rc++;
	}
    }
    
    if (rc == 3)
	return LD_OK;
    else
	return LD_ERROR;
}

char**
result2optionvalues(_arg_ld_t_ptr result)
{
    _arg_ld_t_ptr a_result;
    int nument = -1;
    char **ov;

    for (a_result = result; a_result; a_result = a_result->next) {
	/* there should be only one entry */
	if (nument == -1)
	    nument = a_result->entnum;
	else if (nument != a_result->entnum) {
	    break;
	}
	if (strcasecmp(a_result->attr_name, DHCPOPTIONLIST) == 0) {
	    ov = a_result->argv_strvals;
	    a_result->argv_strvals = NULL;
	    return ov;
	}
    }
    
    return NULL;
}

 
dhcpConfig*
getDHCPConfigForAddr(u_long ipa, char **op)
{
    dhcpConfig* cfg, *cfgsubnet;
    struct rangeConfigLdap* rcl_p;

    if (op == (char **)0)
	return NULL;
    cfgsubnet = cf0_get_config_ldap(ipa);
    if (cfgsubnet == NULL)
	return NULL;

    if ((cfg = cf0_get_subnet_config_copy(cfgsubnet)) == NULL) {
	cfg = (dhcpConfig *)malloc(sizeof(dhcpConfig));
	bzero(cfg, sizeof(dhcpConfig));
	set_default_config_values(cfg);
    }
    cfg->p_netnum = ipa;
    cfg->p_netmask = 0xffffffff;

    if (_DHCPServiceLdap->optionvalues)
	update_subnet_options(cfg, _DHCPServiceLdap->optionvalues);

    if (cfgsubnet->subnetCfgLdap && cfgsubnet->subnetCfgLdap->optionvalues)
	update_subnet_options(cfg, cfgsubnet->subnetCfgLdap->optionvalues);

    rcl_p = getRangeConfigLdap(cfgsubnet->subnetCfgLdap->rangeConfTableLdap,
			       ipa);
    if (rcl_p && rcl_p->optionvalues)
	update_subnet_options(cfg, rcl_p->optionvalues);

    update_subnet_options(cfg, op);
    return cfg;
}

void
cleanup_caches()
{
    if (leaseLCache != NULL) {
	if (leaseLCache->lCacheEntp)
	    free(leaseLCache->lCacheEntp);
	free(leaseLCache);
    }
    if (resvnLCache != NULL) {
	if (resvnLCache->lCacheEntp)
	    free(resvnLCache->lCacheEntp);
	free(resvnLCache);
    }
}

void
cleanup_rangeConf(struct rangeConfigLdap* rcl_p)
{
    if (rcl_p->rangeDn) free (rcl_p->rangeDn);
    if (rcl_p->startip) free (rcl_p->startip);
    if (rcl_p->endip) free (rcl_p->endip);
    if (rcl_p->exclusions) ldap_value_free(rcl_p->exclusions);
    if (rcl_p->optionvalues) ldap_value_free(rcl_p->optionvalues);
    if (rcl_p->cfg) cf0_free_config_list(rcl_p->cfg);
	
}
void
cleanup_subnetConf(struct subnetConfigLdap* scl_p)
{
    struct rangeConfigLdap *rcl_p;
    if (scl_p->subnetDn) free(scl_p->subnetDn);
    if (scl_p->subnetIP) free(scl_p->subnetIP);
    if (scl_p->optionvalues) ldap_value_free(scl_p->optionvalues);
    for (rcl_p = scl_p->rangeConfTableLdap; rcl_p->rangeDn; rcl_p++)
	cleanup_rangeConf(rcl_p);
}
void
cleanup_DHCPServiceLdap()
{
    struct subnetConfigLdap *scl_p;
    if (!_DHCPServiceLdap)
	return;
    if (_DHCPServiceLdap->optiondefinitions)
	ldap_value_free(_DHCPServiceLdap->optiondefinitions);
    if (_DHCPServiceLdap->optionvalues)
	ldap_value_free(_DHCPServiceLdap->optionvalues);
    for (scl_p = _DHCPServiceLdap->subnetConfTableLdap; 
	 scl_p->subnetDn; scl_p++) 
	cleanup_subnetConf(scl_p);
    if (_DHCPServiceLdap->subnetConfTableLdap)
	free(_DHCPServiceLdap->subnetConfTableLdap);
    free(_DHCPServiceLdap);
}
