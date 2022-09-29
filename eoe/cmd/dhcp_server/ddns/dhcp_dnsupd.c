/*
 * dhcp_dnsupd.c - file containing routines to update DNS from DHCP
 */
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netdb.h>
#include <strings.h>
#include "configdefs.h"
#include "dhcp_dnsupd.h"

/* taken from nameser_compat.h */
#define DELETE          ns_uop_delete
#define ADD             ns_uop_add

extern int debug;
#if (__RES > 19960801)
static struct __res_state res;	/* used in res_init call */
#endif

/*
 * functions from the DNS libbind.a (support.c)
 */
extern int dst_s_conv_bignum_b64_to_u8(char **private_key_b64, u_char *, int);

/* some external globals */
extern int dhcp_dnsupd_ttl;
extern int dhcp_dnsupd_secure;
extern int dhcp_dnsupd_beforeACK;
extern int dhcp_dnsupd_clientARR;

struct clfqdn {
    u_char flag;
    u_char rcode1;
    u_char rcode2;
    char  domain_name[1];
};

#define DDNS_MAX_LINE 256

#define ZONE_PTR	0
#define ZONE_AA		1
#define MAXZONES	2

#define ZONE_PTR_UPD	(1 << ZONE_PTR)
#define ZONE_AA_UPD	(1 << ZONE_AA)

struct server_zone {
    struct in_addr dns_addr;	/* server address */
    char	*zonename;	/* zonename from res_nfindprimary */
};
typedef struct server_zone server_zone;
    
struct server_key {
    int		flag;		/* 0=not computed, -1=not found/err*/
    u_long	netnum;		/* network number */
    char	*b64key;	/* key in base 64 */
    server_zone sz[MAXZONES];	/* zone and server info for this key */
    ns_tsig_key tsig_key;
    struct server_key *next;
};

typedef struct server_key server_key;
static server_key *server_key_root = NULL;
static long ddnsconf_modtime = 0;

void
free_ddns_root(void)
{
    server_key *sk, *skn;
    int i;

    sk = server_key_root;
    while (sk) {
	if (sk->b64key) free(sk->b64key);
	for (i = 0 ; i < MAXZONES; i++) 
	    if (sk->sz[i].zonename) free(sk->sz[i].zonename);
	if (sk->tsig_key.data) free(sk->tsig_key.data);
	skn = sk->next;
	free(sk);
	sk = skn;
    }
    server_key_root = NULL;
}
	
/*
 * parse_ddns_dhcp_config(char *)
 * format of the file is
 * netnum|keyname|alg|secret
 */
int
parse_ddns_dhcp_config(char *filename)
{
    struct stat	st;
    FILE        *file;
    char        *line, line_b[DDNS_MAX_LINE];
    int		lineno;

    char	*nnum, *keyname, *alg, *secret;
    server_key  *svk;
    dhcpConfig	*cfgPtr;
    
    if ((file = fopen(filename, "r")) == NULL) {
	syslog(LOG_ERR, "ddns: config file not found\n");
	return -1;
    }
    fstat(fileno(file), &st);
    if (st.st_mtime == ddnsconf_modtime && st.st_nlink) {
	fclose(file);
	goto storeinconfig;
    }
    if (ddnsconf_modtime != 0) {
	free_ddns_root();
    }
    ddnsconf_modtime = st.st_mtime;
    lineno = 0;

    while (fgets(line_b, DDNS_MAX_LINE, file) && line_b) {
	
	line = line_b;
	lineno++;
	
	if (*line == '\n' || *line == '#' || *line == ';') 
	    continue;

	nnum = strtok(line, " \t");
	if (nnum)
	    keyname = strtok(NULL, " \t");
	else {
	    syslog(LOG_ERR, "ddns: parse error on line %d", lineno);
	    continue;
	}
	if (nnum) 
	    alg = strtok(NULL, " \t");
	else{
	    syslog(LOG_ERR, "ddns: parse error on line %d", lineno);
	    continue;
	}
	if (alg)
	    secret = strtok(NULL, " \t");
	else{
	    syslog(LOG_ERR, "ddns: parse error on line %d", lineno);
	    continue;
	}
	if (!secret) {
	    syslog(LOG_ERR, "ddns: parse error on line %d", lineno);
	    continue;
	}

	svk = (server_key*)malloc(sizeof(server_key));
	if (svk == NULL) {
	    syslog(LOG_ERR, "ddns: Out of memory");
	    return -2;
	}
	bzero(svk, sizeof(server_key));
	if ( (svk->netnum = inet_addr(nnum)) == INADDR_NONE) {
	    syslog(LOG_ERR, "ddns: network number error on line %d", lineno);
	    goto error_cont;
	}
	if (strcmp(alg, "hmac-md5") != 0) {
	    syslog(LOG_ERR, "ddns: error alg. only hmac-md5 supported line %d",
		   lineno);
	    goto error_cont;
	}
	svk->flag = 0;
	strcpy(svk->tsig_key.name, keyname);
	strcpy(svk->tsig_key.alg, NS_TSIG_ALG_HMAC_MD5);
	svk->b64key = strdup(secret);
	if (svk->b64key == NULL)  {
	    syslog(LOG_ERR, "ddns out of memory line %d", lineno);
	    free_ddns_root();
	    return -3;
	}
	svk->next = NULL;
	if (server_key_root) {
	    svk->next = server_key_root;
	    server_key_root = svk;
	}
	else 
	    server_key_root = svk;
	continue;
    error_cont:
	if (svk) {

	    free(svk);
	    svk = NULL;
	}
	continue;
    }
    
    fclose(file);

 storeinconfig:
    if (server_key_root) {
	/* assign to each config */
	cfgPtr = configListPtr;
	while(cfgPtr) {
#ifdef EDHCP
	    cfgPtr->ddns_conf = NULL;
#endif
	    svk = server_key_root;
	    while (svk) {
		if(cfgPtr->p_netnum == svk->netnum ) {
#ifdef EDHCP
		    cfgPtr->ddns_conf = svk;
#endif
		    break;
		}
		svk = svk->next;
	    }
	    cfgPtr = cfgPtr->p_next;
	}
    }
    return 0;
}
/*
 * convert an address in dotted decimal to inaddr.arpa notation
 */
void
addr_inaddrarpa(char *addr)
{
    int ipa[4];

    if ( (addr == (char*)0) || (*addr == '\0') )
	return;
    if (sscanf(addr, "%d.%d.%d.%d", &ipa[0], &ipa[1], &ipa[2], &ipa[3]) == 4) {
	sprintf(addr, "%d.%d.%d.%d.in-addr.arpa.", 
		ipa[3], ipa[2], ipa[1], ipa[0]);
    }
}
   
/*
 * update (add/delete) the PTR resource record
 */ 

static void 
find_primary(struct __res_state *res, ns_updrec *rrecp_start, 
	     server_key *svp, int zone)
{
    char zonename[BUFSIZ];
    int  u8keylen;
    u_char u8key[BUFSIZ];

    if (svp->flag == 0) {
	u8keylen = dst_s_conv_bignum_b64_to_u8(&svp->b64key, u8key, BUFSIZ);
	if (u8keylen > 0) {
	    svp->tsig_key.len = u8keylen;
	    svp->tsig_key.data = malloc(u8keylen);
	    if (svp->tsig_key.data == NULL) {
		syslog(LOG_ERR, "ddns: find_primary out of emmory");
		svp->flag = -1;
		return;
	    }
	    memcpy(svp->tsig_key.data, (char *)&u8key[BUFSIZ - u8keylen], 
		   u8keylen);
	}
	else {
	    svp->flag = -1;
	    return;
	}
    }
    
    bzero(zonename, BUFSIZ);
    if ( (res_nfindprimary(res, rrecp_start, NULL,
			   zonename, sizeof(zonename), 
			   &svp->sz[zone].dns_addr)) < 0) {
	syslog(LOG_ERR, "res_nfindprimary failed");
	svp->flag = -1;
	return;
    }
    else {
	svp->sz[zone].zonename = strdup(zonename);
	if (svp->sz[zone].zonename == NULL) {
	    syslog(LOG_ERR, "ddns: find_primary out of emmory");
	    return;
	}
	switch (zone) {
	case ZONE_AA:
	    svp->flag |= ZONE_AA_UPD;
	    break;
	case ZONE_PTR:
	    svp->flag |= ZONE_PTR_UPD;
	    break;
	default:
	    syslog(LOG_ERR, "Unknown zone %d", zone);
	    svp->flag = -1;
	    return;
	}
    }
    return;
}

static int dhcp_dnsupd_PTR_RR(int opcode, u_long ipaddr, char *fqdn, 
			      u_long lease, server_key *svk)
{
    ns_updrec *rrecp_start = NULL, *rrecp, *tmprrecp;
    int n = 0;
    int r_section, r_size, r_class, r_type, r_ttl;
    struct in_addr in;
    char inaddr_dname[BUFSIZ], *r_dname;

    r_section = S_UPDATE;
    if (ipaddr == 0) {
	syslog(LOG_ERR, "dhcp_dnsupd_PTR_RR: IP address should be not NULL");
	return DHCP_DNSUPD_ERR;
    }
    in.s_addr = ipaddr;
    r_dname = inet_ntoa(in);
    strcpy(inaddr_dname, r_dname);
    addr_inaddrarpa(inaddr_dname);
    r_dname = inaddr_dname;
    if (r_dname == (char*)0) {
	syslog(LOG_ERR, "dhcp_dnsupd_PTR_RR: IP address should be not NULL");
	return DHCP_DNSUPD_ERR;
    }

    r_class = C_IN;
    r_type = T_PTR;
    
    if ( (dhcp_dnsupd_ttl > 0) && (dhcp_dnsupd_ttl < lease) )
	r_ttl = dhcp_dnsupd_ttl;
    else
	r_ttl = (int)lease;

    r_size = strlen(fqdn);
    
    if (opcode == ADD) {	/* do we need this */
	if ( !(rrecp = res_mkupdrec(r_section, r_dname, r_class,
				    r_type, 0)) ||
	     (r_size > 0 && !(rrecp->r_data = (u_char *)malloc(r_size))) ) {
	    if (rrecp)
		res_freeupdrec(rrecp);
	    syslog(LOG_ERR, "dhcp_dnsupd_PTR_RR: Not enough memory");
	    return DHCP_DNSUPD_ERR;
	}
	rrecp->r_opcode = DELETE;
	rrecp->r_size = r_size;
	(void) strncpy((char *)rrecp->r_data, fqdn, r_size);
	rrecp_start = rrecp;
    }
    if ( !(rrecp = res_mkupdrec(r_section, r_dname, r_class,
				r_type, r_ttl)) ||
	 (r_size > 0 && !(rrecp->r_data = (u_char *)malloc(r_size))) ) {
	if (rrecp)
	    res_freeupdrec(rrecp);
	syslog(LOG_ERR, "dhcp_dnsupd_PTR_RR: Not enough memory");
	return DHCP_DNSUPD_ERR;
    }
    rrecp->r_opcode = opcode;
    rrecp->r_size = r_size;
    (void) strncpy((char *)rrecp->r_data, fqdn, r_size);
    if (rrecp_start == NULL)
	rrecp_start = rrecp;
    else
	rrecp_start->r_next = rrecp;

    if (rrecp_start) {
#if (__RES > 19960801)
	bzero(&res, sizeof(struct __res_state));
	(void) res_ninit(&res);
#ifdef DEBUG
	/*	res.options |= RES_DEBUG; */
#endif
	if ( svk && ((svk->flag & ZONE_PTR_UPD) == 0) ) {
	    find_primary(&res, rrecp_start, svk, ZONE_PTR);
	}
	if ( svk && dhcp_dnsupd_secure && (svk->flag != -1) ) {
	    if ( (n = res_nsendupdate(&res, rrecp_start, &svk->tsig_key,
				      svk->sz[ZONE_PTR].zonename, 
				      svk->sz[ZONE_PTR].dns_addr))  < 0)
		syslog(LOG_ERR, "res_nsendupdate failed");
	}		
	else {
	    if ((n = res_nupdate(&res, rrecp_start)) < 0)
		syslog(LOG_ERR, "dhcp_dnsupd_PTR_RR: failed update packet");
	}
#else
	res_init();
	_res.options &= (~RES_DEBUG); 
	if ((n = res_update(rrecp_start)) < 0)
	    syslog(LOG_ERR, "dhcp_dnsupd_PTR_RR: failed update packet");
#endif
	
	/* free malloc'ed memory */
	while(rrecp_start) {
	    tmprrecp = rrecp_start;
	    rrecp_start = rrecp_start->r_next;
	    if (tmprrecp->r_size)
		free((char *)tmprrecp->r_data);
	    res_freeupdrec(tmprrecp);
	}
    }
    return (n);
}


/*
 * update (add/delete) the A resource record
 */ 

static int dhcp_dnsupd_A_RR(int opcode, u_long ipaddr, char *fqdn, 
			    u_long lease, server_key *svk)
{
    ns_updrec *rrecp_start = NULL, *rrecp, *tmprrecp;
    int n = 0;
    int r_section, r_size, r_class, r_type, r_ttl;
    struct in_addr in;
    char *r_dname, *r_data;
    char dname[BUFSIZ];

    r_section = S_UPDATE;
    if (ipaddr == 0) {
	syslog(LOG_ERR, "dhcp_dnsupd_A_RR: IP address should be not NULL");
	return DHCP_DNSUPD_ERR;
    }
    in.s_addr = ipaddr;
    r_dname = fqdn;
    if (r_dname == (char*)0) {
	syslog(LOG_ERR, "dhcp_dnsupd_A_RR: IP address should be not NULL");
	return DHCP_DNSUPD_ERR;
    }

    r_class = C_IN;
    r_type = T_A;
    r_ttl = 0;
    r_size = 0;
    
    if ( !(rrecp = res_mkupdrec(r_section, r_dname, r_class,
				r_type, r_ttl)) ||
	 (r_size > 0 && !(rrecp->r_data = (u_char *)malloc(r_size))) ) {
	if (rrecp)
	    res_freeupdrec(rrecp);
	syslog(LOG_ERR, "dhcp_dnsupd_A_RR: Not enough memory");
	return DHCP_DNSUPD_ERR;
    }

    rrecp->r_opcode = DELETE;
    rrecp->r_data = (u_char*)0;
    rrecp->r_size = r_size;
    rrecp_start = rrecp;

    if (opcode == ADD) {
	/* add the new fqdn */
	r_data = inet_ntoa(in);
	r_size = strlen(r_data);
	strncpy(dname, r_data, r_size);
	if ( (dhcp_dnsupd_ttl > 0) && (dhcp_dnsupd_ttl < lease) )
	    r_ttl = dhcp_dnsupd_ttl;
	else
	    r_ttl = (int) lease;
	if ( !(rrecp = res_mkupdrec(r_section, r_dname, r_class,
				    r_type, r_ttl)) ||
	     (r_size > 0 && !(rrecp->r_data = (u_char *)malloc(r_size))) ) {
	    if (rrecp)
		res_freeupdrec(rrecp);
	    syslog(LOG_ERR, "dhcp_dnsupd_A_RR: Not enough memory");
	    return DHCP_DNSUPD_ERR;
	}
	rrecp->r_opcode = ADD;
	rrecp->r_size = r_size;
	(void) strncpy((char *)rrecp->r_data, dname, r_size);
	rrecp_start->r_next = rrecp;
    }
    if (rrecp_start) {
#if (__RES > 19960801)
	bzero(&res, sizeof(struct __res_state));
	(void) res_ninit(&res);
#ifdef DEBUG
	/*	res.options |= RES_DEBUG; */
#endif
	if ( svk && ((svk->flag & ZONE_AA_UPD) == 0) ) {
	    find_primary(&res, rrecp_start, svk, ZONE_AA);
	}
	if ( svk && dhcp_dnsupd_secure && (svk->flag != -1) ) {
	    if ( (n = res_nsendupdate(&res, rrecp_start, &svk->tsig_key,
				      svk->sz[ZONE_AA].zonename, 
				      svk->sz[ZONE_AA].dns_addr))  < 0)
		syslog(LOG_ERR, "res_nsendupdate failed");
	}		
	else {
	    if ((n = res_nupdate(&res, rrecp_start)) < 0)
		syslog(LOG_ERR, "dhcp_dnsupd_A_RR: failed update packet");
	}
#else
	res_init();
	_res.options &= (~RES_DEBUG); 
	if ((n = res_update(rrecp_start)) < 0)
	    syslog(LOG_ERR, "dhcp_dnsupd_A_RR: failed update packet");
#endif
	/* free malloc'ed memory */
	while(rrecp_start) {
	    tmprrecp = rrecp_start;
	    rrecp_start = rrecp_start->r_next;
	    if (tmprrecp->r_size)
		free((char *)tmprrecp->r_data);
	    res_freeupdrec(tmprrecp);
	}
    }
    return n;
}


/* 
 * delete DNS records on a DHCPRELEASE
 */
int dhcp_dnsdel(u_long ipa, void *svp)
{
    struct hostent *he;
    char hsname[MAXDNAME];
    int n;
    server_key *svk = svp;

    he = gethostbyaddr((char*)&ipa, sizeof(ipa), AF_INET);
    if (he) {
        strcpy(hsname, he->h_name);
	n = dhcp_dnsupd_A_RR(DELETE, ipa, hsname, 0, svk);
	if (n < 0) 
	  return -1;
    }
    return (dhcp_dnsupd_PTR_RR(DELETE, ipa, hsname, 0, svk));
}

/*
 * update DNS records as needed and set the client_fqdn option
 */
int dhcp_dnsadd(int when, u_long ipaddr, char *hostname, char *domain,
		u_long lease, char *client_fqdn, void *svp)
{
    char cf[MAXDNAME+3];
    struct clfqdn *clfqdn_p;
    char *fqhnm;
    server_key *svk = svp;

    clfqdn_p  = (struct clfqdn*)client_fqdn;
    sprintf(cf, "%s.%s", hostname, domain);
    if (clfqdn_p->domain_name[0] != '\0') {
	if (strcmp(cf, clfqdn_p->domain_name) != 0) {
	    if (debug >= 2)
		syslog(LOG_DEBUG, "Name specified in CLIENT_FQDN (%s) option"
		       "mismatch; using %s", clfqdn_p->domain_name, cf);
	}
	strcpy(clfqdn_p->domain_name, cf);
    }
    fqhnm = cf;
    clfqdn_p->rcode1 = (u_char)0;
    clfqdn_p->rcode2 = (u_char)0;
    if ( when != dhcp_dnsupd_beforeACK) { /* do updates to DNS */
	/* called before (0) sending DHCPACK and update beforeACK = 1
	 * or when called after(1) sending DHCPACK and beforeACK = 0
	 */
	clfqdn_p->rcode1 = 
	    (u_char)dhcp_dnsupd_PTR_RR(ADD, ipaddr, fqhnm, lease, svk);
	if ( (clfqdn_p->flag == 1) || (dhcp_dnsupd_clientARR == 1) ) {
	    clfqdn_p->rcode2 =
		(u_char)dhcp_dnsupd_A_RR(ADD, ipaddr, fqhnm, lease, svk);
	    strcpy(clfqdn_p->domain_name, fqhnm);
	}
    }
    else if (when == 0) {	/* update AfterACK but called before the ACK */
	clfqdn_p->rcode1 = (u_char)255;
	if ( (clfqdn_p->flag == 1) || (dhcp_dnsupd_clientARR == 1) ) {
	    clfqdn_p->rcode2 = (u_char) 255;
	    strcpy(clfqdn_p->domain_name, fqhnm);
	}
    }
    if ( (when == 0) &&
	 (clfqdn_p->flag == 0) && (dhcp_dnsupd_clientARR == 1) ) 
	clfqdn_p->flag = (u_char) 3; /* forced update at server */
    return 0;
}
	
