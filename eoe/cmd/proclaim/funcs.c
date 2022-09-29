#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dhcp.h"
#include "dhcp_common.h"
#include <strings.h>
#include <sys/sysmacros.h>
#include <ctype.h>
#include <dirent.h>
#include <syslog.h>

/*extern int dh0_encode_dhcp_client_packet(u_char *dhbuf, int, int, u_long,
					 u_int, char *, int, u_char, u_long,
					 char *, char *, int, char*);
*/
extern int client_fqdn_flag;
extern char *client_fqdn;
extern int client_fqdn_option;

char	*RealHostFile =    "/etc/hosts";
char	*tmpHostFile =     "/etc/hosts.tmp";
char	*RealEtherFile =   "/etc/ethers";
char	*tmpEtherFile =    "/etc/ethers.tmp";

#define LINE_SIZE 128

#define CONFIG_DIR  "/etc/config/"

int
dh0_get_dhcp_msg_type(dhbuf)
    u_char dhbuf[];
{
    u_char *p, *p_end;
    int len;
    int msg_type = -1;

    p = dhbuf;
    p_end = p + DHCP_OPTS_SIZE;

    while( (p < p_end) && (*p != END_TAG) ) {
	switch(*p++) {
	    case PAD_TAG:
		continue;
	    case DHCP_MSG_TAG:
		len = *p++;
		if(len == 1)
		    msg_type = *p++;
		return msg_type;
	    case SGI_VENDOR_TAG:
		len = *p++;
		continue;
	    default:
		len = *p++;
		p += len;
	}
    }
    return msg_type;
}

int
dh0_decode_dhcp_server_packet(dhbuf, lease, msg, server, hsname,
				nisdm, dnsdm, smask, sdServer,
				lease_t1, lease_t2, offered_hsname)
    u_char dhbuf[];
    u_int *lease, *lease_t1, *lease_t2;
    char **msg, **hsname , **nisdm, **dnsdm;
    u_long *server, *smask, *sdServer;
    char **offered_hsname;
{
    char nbuf[MAXHOSTNAMELEN+1];
    u_char *p, *p_end;
    int len, tlen;
    int msg_type = BOOTP1533;

    *lease = *lease_t1 = *lease_t2 = 0;
    *msg = *hsname = *nisdm = *dnsdm = *offered_hsname = (char *)0;
    *server = *smask = *sdServer = 0;
    p = dhbuf;
    p_end = p + DHCP_OPTS_SIZE;

    while( (p < p_end) && (*p != END_TAG) ) {
	switch(*p++) {
	    case PAD_TAG:
		continue;
	    case DHCP_MSG_TAG:
		len = *p++;
		if(len == 1)
		    msg_type = *p++;
		else
		    p += len;
		if( (msg_type < DHCPDISCOVER) || (msg_type > DHCPRELEASE) )
		    msg_type = -1;
		continue;
	    case IP_LEASE_TIME_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, lease, 4);
		    p += len;
		}
		continue;
	    case RENEW_LEASE_TIME_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, lease_t1, 4);
		    p += len;
		}
		continue;
	    case REBIND_LEASE_TIME_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, lease_t2, 4);
		    p += len;
		}
		continue;
	    case SUBNETMASK_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, smask, 4);
		    p += len;
		}
		continue;
	    case HOSTNAME_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		*hsname = strdup(nbuf);
		p += len;
		continue;
	    case NISDOMAIN_NAME_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		*nisdm = strdup(nbuf);
		p += len;
		continue;
	    case DHCP_SERVER_MSG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		*msg = strdup(nbuf);
		p += len;
		continue;
	    case DNSDOMAIN_NAME_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		*dnsdm = strdup(nbuf);
		p += len;
		continue;
	    case DHCP_SERVER_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, server, 4);
		    p += len;
		}
		continue;
	    case SGI_VENDOR_TAG:
		len = *p++;
		continue;
	    case RESOLVE_HOSTNAME_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		*offered_hsname = strdup(nbuf);
		p += len;
		continue;
	    case SDIST_SERVER_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, sdServer, 4);
		    p += len;
		}
		continue;
	    default:
		len = *p++;
		p += len;
	}
    }
    return msg_type;
}

int
dh0_encode_dhcp_client_packet(dhbuf, msgtype, reqParams, ipaddr, lease,
			      clientId, clientId_len, hw_type, server,
			      req_hsname, addl_options_to_get,
			      addl_options_len, reqHostName, vendor_class_str)
    u_char dhbuf[];
    int msgtype;
    int reqParams;
    u_int lease;
    char *clientId;
    int clientId_len;
    u_char hw_type;
    u_long server, ipaddr;
    char *req_hsname;
    char* addl_options_to_get;
    int addl_options_len;
    char *reqHostName;
    char *vendor_class_str;
{
    int len, qt;
    char *p, *q;

    p = (char *)dhbuf;

    if( (msgtype >= DHCPDISCOVER) && (msgtype <= DHCPPOLL) ) {
	/* DHCP Message Type : 4 */
	*p++ = DHCP_MSG_TAG;
	*p++ = 1;
	*p++ = msgtype;
    }
    /* else it is an extended BOOTP 1533 packet and we ignore the value */

    /* IP Address : 8 */
    if(ipaddr) {
	*p++ = IPADDR_TAG;
	*p++ = 4;
	bcopy(&ipaddr, p, 4);
	p += 4;
    }

    /* Server addr : 8 */
    if(server) {
	*p++ = DHCP_SERVER_TAG;
	*p++ = 4;
	bcopy(&server, p, 4);
	p += 4;
    }

    /* Lease Time : 8 */
    if(lease) {
	*p++ = IP_LEASE_TIME_TAG;
	*p++ = 4;
	bcopy(&lease, p, 4);
	p += 4;
    }

    /* clientId : 3+len */
    if(clientId && clientId_len) {
       	*p++ = DHCP_CLIENT_ID_TAG;
	*p++ = clientId_len + 1;
	*p++ = hw_type;
	bcopy(clientId, p, clientId_len);
	p += clientId_len;
    }

    if (reqHostName && *reqHostName) {
	*p++ = HOSTNAME_TAG;
	len = strlen(reqHostName);
	*p++ = len;
	bcopy(reqHostName, p, len);
	p += len;
    }

    if(req_hsname && *req_hsname) {
	len = strlen(req_hsname);
	*p++ = SGI_VENDOR_TAG;
	*p++ = len+2;
	*p++ = RESOLVE_HOSTNAME_TAG;
	*p++ = len;
	bcopy(req_hsname, p, len);
	p += len;
    }
    
    if ( (msgtype == DHCPREQUEST) && (client_fqdn_option) ) {
	len = strlen(client_fqdn);
	*p++ = CLIENT_FQDN_TAG;
	*p++ = len + 3;
	*p++ = client_fqdn_flag;
	*p++ = 0;
	*p++ = 0;
	strncpy(p, client_fqdn, len);
	p += len;
    }

    /* vendor class */
    if (vendor_class_str && *vendor_class_str) {
	len = strlen(vendor_class_str);
	*p++ = DHCP_CLASS_TAG;
	*p++ = len;
	strncpy(p, vendor_class_str, len);
	p += len;
    }
    /* Requested Parameters */
    if((reqParams == 0) && (addl_options_len <= 0)) {
	*p = END_TAG;
	return 0;
    }
    len = 0;
    *p++ = DHCP_PARAM_REQ_TAG;
    q = p++;		/* Save the pointer to the length field for later */
    if(reqParams & GET_IPLEASE_TIME) {
	*p++ = IP_LEASE_TIME_TAG;
	len++;
    }
    if(reqParams & GET_SUBNETMASK) {
	*p++ = SUBNETMASK_TAG;
	len++;
    }
    if(reqParams & GET_HOSTNAME) {
	*p++ = HOSTNAME_TAG;
	len++;
    }
    if(reqParams & GET_NISDOMAIN) {
	*p++ = NISDOMAIN_NAME_TAG;
	len++;
    }
    if(reqParams & GET_DNSDOMAIN) {
	*p++ = DNSDOMAIN_NAME_TAG;
	len++;
    }
    if(reqParams & GET_DHCP_SERVER) {
	*p++ = DHCP_SERVER_TAG;
	len++;
    }
    if(reqParams & GET_SDIST_SERVER) {
	*p++ = SDIST_SERVER_TAG;
	len++;
    }
    if(reqParams & GET_IPADDRESS) {
	*p++ = IPADDR_TAG;
	len++;
    }
    if(reqParams & RESOLVE_HOSTNAME) {
	*p++ = RESOLVE_HOSTNAME_TAG;
	len++;
    }
    if (addl_options_len) {
	bcopy(addl_options_to_get, p, addl_options_len);
	len += addl_options_len;
	p += addl_options_len;
    }
    *q = len;

    *p = END_TAG;
    return 0;
}

int
sys0_addToHostsFile(u_long ipa, char *hname, char *nisdm)
{
    FILE *fp;
    struct in_addr ipn;
    char ibuf[128];
    char *ipchar;
    int rval;

    if(rval = sys0_removeFromHostsFile(ipa))
	return(rval);

    fp = fopen(RealHostFile, "a");
    if(!fp)
	return(1);
    ipn.s_addr = ipa;
    ipchar = inet_ntoa(ipn);
    if (nisdm && (strchr(hname, '.') == (char*)0)) /*write fully qualified
						     name also */
	fprintf(fp, "%s\t%s.%s\t%s\n", ipchar, hname, nisdm, hname);
    else
	fprintf(fp, "%s\t%s\n", ipchar, hname);
    fclose(fp);
    return(0);
}

int
sys0_addToEthersFile(struct ether_addr *eth, char *hname)
{
    FILE *fp;
    char *ebuf;

    sys0_removeFromEthersFile(eth);
    fp = fopen(RealEtherFile, "a");
    if(!fp)
	return(1);
    ebuf = ether_ntoa(eth);
    fprintf(fp, "%s\t%s\n", ebuf, hname);
    fclose(fp);
    return(0);
}

int
sys0_removeFromHostsFile(u_long ipaddr)
{
    FILE *fp, *fp1;
    char buf[256], zbuf[256], dbuf[256];
    u_long curip;
    char *p;

    fp = fopen(RealHostFile, "r");
    if(!fp)
	return(1);
    fp1 = fopen(tmpHostFile, "w");
    if(!fp1) {
	fclose(fp);
	return(2);
    }

    /* Instead of deleteing the old entry, comment it out */

    dbuf[0] = '#'; dbuf[1] = ' '; dbuf[2] = '\0';

    while(fgets(buf, 256, fp) != NULL) {
	if( (buf[0] == '\n') || (buf[0] == '#') ) {
	    fputs(buf, fp1);
	    continue;
	}
	if(isdigit(buf[0])) {
	    strcpy(zbuf, buf);
	    p = buf;
	    while( (*p != ' ') && (*p != '\t') )
		p++;
	    *p = '\0';
	    curip = inet_addr(buf);
	    if(curip == ipaddr) {
		strcat(dbuf, zbuf);
		fputs(dbuf, fp1);
		continue;
	    }
	}
	fputs(zbuf, fp1);
    }
    fclose(fp);
    fclose(fp1);
    rename(tmpHostFile, RealHostFile);
    return(0);
}

int
sys0_removeFromEthersFile(struct ether_addr *eth)
{
    FILE *fp, *fp1;
    char buf[256], hsname[64];
    struct  ether_addr ehs;
    int len;
    int cr_found;

    fp = fopen(RealEtherFile, "r");
    if(!fp)
	return(1);
    fp1 = fopen(tmpEtherFile, "w");
    if(!fp1) {
	fclose(fp);
	return(2);
    }

    while(fgets(buf, 256, fp) != NULL) {
	if( (buf[0] == '\n') || (buf[0] == '#') ) {
	    fputs(buf, fp1);
	    continue;
	}
	if(isxdigit(buf[0])) {
	    len = strlen(buf);
	    cr_found = 0;
	    if(buf[len-1] == '\n') {
		buf[len-1] = '\0';
		cr_found = 1;
	    }
	    if(ether_line(buf, &ehs, hsname) == 0) {
		if(bcmp(ehs.ea_addr, eth->ea_addr, 6) == 0) {
		    cr_found = 0;
		    continue;
		}
	    }
	}
	if(cr_found) {
	   buf[len-1] = '\n';
	   cr_found = 0;
	}
	fputs(buf, fp1);
    }
    fclose(fp);
    fclose(fp1);
    rename(tmpEtherFile, RealEtherFile);
    return(0);
}

void
print_ipaddr_array(FILE* fd, char *p, int len, char *msg)
{
    int i = 0;
    u_long addr;
    struct in_addr ipz;
    
    if (len <= 0)
	return;
    fprintf(fd, "%s ", msg);
    while (i < len) {
	bcopy(p+i, &addr, 4);
	ipz.s_addr = addr;
	fprintf(fd, "%s ", inet_ntoa(ipz));
	i += 4;
    }
    fprintf(fd, "\n");
}

void
print_ipaddr_pair(FILE* fd, char *p, int len, char *msg)
{
    int i = 0;
    u_long addr1, addr2;
    struct in_addr ipz1, ipz2;
    
    if (len <= 0)
	return;
    fprintf(fd, "%s ", msg);
    while (i < len) {
	bcopy(p+i, &addr1, 4);
	bcopy(p+i+4, &addr2, 4);
	ipz1.s_addr = addr1;
	ipz2.s_addr = addr2;
	fprintf(fd, "%s - ", inet_ntoa(ipz1)); /* note: static return value */
	fprintf(fd, "%s ", inet_ntoa(ipz2));
	i += 8;
    }
    fprintf(fd, "\n");
}

void
print_short_array(FILE* fd, char* p, int len, char* msg)
{
    int i = 0;
    short shorttmp;

    if (len <= 0)
	return;
    fprintf(fd, "%s ", msg);
    while (i < len) {
	bcopy((p+i), &shorttmp, len);
	fprintf(fd, "%d", shorttmp);
	i += 2;
    }
}


/*
 * NOTE:  special care required for changing the format of the
 *        CLIENT_CACHE_ASCII file.  If you must change the format,
 *        change attribute names, etc. do not also change the format,
 *        attribute names, etc. of the CLIENT_DATA file, since it may
 *        break compatibility with applications that depend on that file.
 *        This concerns only the existing parameters below.  If you are
 *        simply adding new parameters, there are no restrictions.
 */
int
dh0_decode_and_write_ascii_cache(FILE* fd, time_t lease_start,
				 struct bootp* rxp)
{
				
    u_int lease, lease_t1, lease_t2;
    u_long server, smask, sdServer;
    struct in_addr ipz;

    char nbuf[MAXHOSTNAMELEN+1];
    u_char *p, *p_end;
    int len, tlen;
    int msg_type = BOOTP1533;
    short shorttmp;

    lease = lease_t1 = lease_t2 = 0;
    server = smask = sdServer = 0;
    p = (u_char*)rxp->dh_opts;
    p_end = p + DHCP_OPTS_SIZE;

    /*    fprintf(fd, "LeaseStartTime: %d\n", lease_start); */
    ipz.s_addr = rxp->bp_yiaddr.s_addr;
    fprintf(fd, "HostIPaddress: %s\n", inet_ntoa(ipz));
    fprintf(fd, "DHCPserverName: %s\n", (char*)rxp->bp_sname);
    while( (p < p_end) && (*p != END_TAG) ) {
	switch(*p++) {
	    case PAD_TAG:
		continue;
	    case DHCP_MSG_TAG:
		len = *p++;
		if(len == 1)
		    msg_type = *p++;
		else
		    p += len;
		if( (msg_type < DHCPDISCOVER) || (msg_type > DHCPPRPL) )
		    msg_type = -1;
		fprintf(fd,"MessageType: %d\n", msg_type);
		continue;
	    case IP_LEASE_TIME_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &lease, 4);
		    fprintf(fd,"LeaseTimeInSecs: %d\n", lease);
		}
		p += len;
		continue;
	    case RENEW_LEASE_TIME_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &lease_t1, 4);
		    fprintf(fd,"RenewLeaseTimeInSecs: %d\n", lease_t1);
		}
		p += len;
		continue;
	    case REBIND_LEASE_TIME_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &lease_t1, 4);
		    fprintf(fd,"RenewLeaseTimeInSecs: %d\n", lease_t1);
		}
		p += len;
		continue;
	    case SUBNETMASK_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &smask, 4);
		    fprintf(fd, "NetworkMask: 0x%x\n", smask);
		}
		p += len;
		continue;
	    case HOSTNAME_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		fprintf(fd, "HostName: %s\n", nbuf);
		p += len;
		continue;
	    case NISDOMAIN_NAME_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		fprintf(fd, "NISdomainName: %s\n", nbuf);
		p += len;
		continue;
	    case DHCP_SERVER_MSG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		fprintf(fd, "ServerMessage: %s\n", nbuf);
		p += len;
		continue;
	    case DNSDOMAIN_NAME_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		fprintf(fd, "DNSdomainName: %s\n", nbuf);
		p += len;
		continue;
	    case DHCP_SERVER_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &server, 4);
		    ipz.s_addr = server;
		    fprintf(fd, "ServerIPaddress: %s\n", inet_ntoa(ipz));
		}
		p += len;
		continue;
	    case SGI_VENDOR_TAG:
		len = *p++;
		continue;
	    case RESOLVE_HOSTNAME_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		fprintf(fd, "OfferedHostname: %s\n", nbuf);
		p += len;
		continue;
	    case SDIST_SERVER_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &sdServer, 4);
		    ipz.s_addr = sdServer;
		    fprintf(fd, "PropelServerIPaddress: %s\n",
			    inet_ntoa(ipz));
		}
		p += len;
		continue;
	      case ROUTER_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "Routers:");
		p+=len;
		continue;
	      case TIMESERVERS_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "TimeServers:");
		p+=len;
		continue;
	      case DNSSERVERS_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "DNSservers:");
		p+=len;
		continue;
	      case NIS_SERVERS_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "NISservers:");
		p+=len;
		continue;
	      case IF_MTU_TAG:
		len = *p++;
		if(len == 2) {
		    bcopy(p, &shorttmp, len);
		    fprintf(fd, "MTU: %d\n", shorttmp);
		}
		p += len;
		continue;
	      case ALL_SUBNETS_LOCAL_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "AllSubnetsLocal: %d\n", (int)(*p));
		p += len;
		continue;
	      case BROADCAST_ADDRESS_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &smask, 4);
		    ipz.s_addr = smask;
		    fprintf(fd, "BroadCastAddress: %s\n",
			    inet_ntoa(ipz));
		}
		p += len;
		continue;
	      case CLIENT_MASK_DISC_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "ClientMaskDiscovery: %d\n", (int)(*p));
		p += len;
		continue;
	      case MASK_SUPPLIER_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "MaskSupplier: %d\n", (int)(*p));
		p += len;
		continue;
	      case STATIC_ROUTE_TAG:
		len = *p++;
		print_ipaddr_pair(fd, (char *)p, len, "StaticRoutes:");
		p += len;
		continue;
	      case LOGSERVERS_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "LogServers:");
		p+=len;
		continue;
	      case COOKIESERVERS_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "CookieServers:");
		p+=len;
		continue;
	      case LPRSERVERS_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "LprServers:");
		p+=len;
		continue;
	      case RESOURCESERVERS_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "ResourceServers:");
		p+=len;
		continue;
	      case BOOTFILE_SIZE_TAG:
		len = *p++;
		if(len == 2) {
		    bcopy(p, &shorttmp, len);
		    fprintf(fd, "BootFileSize: %d\n", shorttmp);
		}
		p += len;
		continue;
	      case SWAPSERVER_ADDRESS_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &server, 4);
		    ipz.s_addr = server;
		    fprintf(fd, "SwapServerIPaddress: %s\n",
			    inet_ntoa(ipz));
		}
		p += len;
		continue;
	      case IP_FORWARDING_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "IPforwarding: %d\n", (int)(*p));
		p += len;
		continue;
	      case NON_LOCAL_SRC_ROUTE_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "NonLocalSourceRouting: %d\n", (int)(*p));
		p += len;
		continue;
	      case POLICY_FILTER_TAG:
		len = *p++;
		print_ipaddr_pair(fd, (char *)p, len, "PolicyFilters:");
		p += len;
		continue;
	      case MAX_DGRAM_REASSEMBL_TAG:
		len = *p++;
		if(len == 2) {
		    bcopy(p, &shorttmp, len);
		    fprintf(fd, "MaxDgramRassySize: %d\n", shorttmp);
		}
		p += len;
		continue;
	      case DEF_IP_TIME_LIVE_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "IPtimeToLive: %d\n", (int)(*p));
		p += len;
		continue;
	      case PATH_MTU_AGE_TMOUT_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &lease_t1, 4);
		    fprintf(fd,"PathMTUageTimeout: %d\n", lease_t1);
		}
		p += len;
		continue;
	      case PATH_MTU_PLT_TABLE_TAG:
		len = *p++;
		print_short_array(fd, (char *)p, len, "MTUlist:");
		p+=len;
		continue;
	      case ROUTER_DISC_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "RouterDiscovery: %d\n", (int)(*p));
		p += len;
		continue;
	      case ROUTER_SOLICIT_ADDR_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &smask, 4);
		    ipz.s_addr = smask;
		    fprintf(fd, "RouterSolicitAddress: %s\n",
			    inet_ntoa(ipz));
		}
		p += len;
		continue;
	      case TRAILER_ENCAPS_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "TrailerEncapsulation: %d\n", (int)(*p));
		p += len;
		continue;
	      case ARP_CACHE_TIMEOUT_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &lease_t1, 4);
		    fprintf(fd,"ArpCacheTimeout: %d\n", lease_t1);
		}
		p += len;
		continue;
	      case ETHERNET_ENCAPS_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "EthernetEncapsulation: %d\n", (int)(*p));
		p += len;
		continue;
	      case TCP_DEFAULT_TTL_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "TCPdefaultTTL: %d\n", (int)(*p));
		p += len;
		continue;
	      case TCP_KEEPALIVE_INTVL_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &lease_t1, 4);
		    fprintf(fd,"TCPkeepaliveInterval: %d\n", lease_t1);
		}
		p += len;
		continue;
	      case TCP_KEEPALIVE_GARBG_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "TCPdefaultTTL: %d\n", (int)(*p));
		p += len;
		continue;
	      case NETBIOS_NMSERV_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len,
				   "NetBIOSNameServerAddress:");
		p += len;
		continue;
	      case NETBIOS_DISTR_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len,
				   "NetBIOSDistributionServerAddress:");
		p += len;
		continue;
	      case NETBIOS_NODETYPE_TAG:
		len = *p++;
		if (len == 1)
		    fprintf(fd, "NetBIOSnodeType: %d\n", (int)(*p));
		p += len;
		continue;
	      /*case NETBIOS_SCOPE_TAG:
		if(reqP->GET_NETBIOS_SCOPE) { 
		if(cfg->p_NetBIOS_scope && *(cfg->p_NetBIOS_scope)) {
		len = strlen(cfg->p_NetBIOS_scope);
		encode_ptr_tag(&p, NETBIOS_SCOPE_TAG, cfg->p_NetBIOS_scope, len);
		} */
	      case X_FONTSERVER_ADDR_TAG: 
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "XfontServerAddress:");
		p += len;
		continue;
	      case X_DISPLAYMGR_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "XdisplayManager:");
		p += len;
		continue;
	      case NISPLUS_DOMAIN_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		fprintf(fd, "NIS+domainName: %s\n", nbuf);
		p += len;
		continue;
	      case NISPLUS_SERVER_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "NIS+ServerAddress:");
		p += len;
		continue;
	      case MBLEIP_HMAGENT_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "NIS+ServerAddress:");
		p += len;
		continue;
	      case SMTP_SERVER_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "SMTPserverAddress:");
		p += len;
		continue;
	      case POP3_SERVER_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "POP3serverAddress:");
		p += len;
		continue;
	      case NNTP_SERVER_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "NNTPserverAddress:");
		p += len;
		continue;
	      case WWW_SERVER_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "WWWserverAddress:");
		p += len;
		continue;
	      case FINGER_SERVER_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "FINGERserverAddress:");
		p += len;
		continue;
	      case IRC_SERVER_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "IRCserverAddress:");
		p += len;
		continue;
	      case STTALK_SERVER_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "STTALKserverAddress:");
		p += len;
		continue;
	      case STDA_SERVER_ADDR_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "STDAserverAddress:");
		p += len;
		continue;
	      case TIME_OFFSET_TAG:
		len = *p++;
		if(len == 4) {
		    bcopy(p, &lease_t1, 4);
		    fprintf(fd,"TCPkeepaliveInterval: %d\n", lease_t1);
		}
		p += len;
		continue;
	      case NAME_SERVER116_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "Name116serverAddress:");
		p += len;
		continue;
	      case IMPRESSSERVERS_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "IMPRESSserverAddress:");
		p += len;
		continue;
	      case MERITDUMP_FILE_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		fprintf(fd, "MeritDumpPathName: %s\n", nbuf);
		p += len;
		continue;
	      case ROOT_PATH_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		fprintf(fd, "RootPathname: %s\n", nbuf);
		p += len;
		continue;
	      case EXTENSIONS_PATH_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		fprintf(fd, "ExtensionsPathname: %s\n", nbuf);
		p += len;
		continue;
	      case NTP_SERVERS_TAG:
		len = *p++;
		print_ipaddr_array(fd, (char *)p, len, "Name116serverAddress:");
		p += len;
		continue;
	      case TFTP_SERVER_NAME_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		fprintf(fd, "TFTPserverName: %s\n", nbuf);
		p += len;
		continue;
	      case BOOTFILE_NAME_TAG:
		len = *p++;
		bcopy(p, nbuf, len);
		nbuf[len] = '\0';
		fprintf(fd, "BootFilename: %s\n", nbuf);
		p += len;
		continue;
	    default:
		len = *p++;
		p += len;
	}
    }
    return msg_type;
}
