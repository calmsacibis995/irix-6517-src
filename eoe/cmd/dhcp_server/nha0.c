#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/soioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netdb.h>
#include <nlist.h>
#include <rpcsvc/ypclnt.h>
#include <syslog.h>
#include "dhcpdefs.h"

#define	MAX_BROADCAST_SIZE	1400
#define	YP_TRUE			1

char NFSOPT[] = "/etc/havenfs";

extern char *alt_sysname;

int
nha0_haveNfsOption(void)
{
    FILE *fip;

    if( access(NFSOPT, 1) )
	return(0);

    fip = popen(NFSOPT, "r");
    if(!fip)
	return 0;
    if(pclose(fip))
	return(0);
    return(1);
}

int
nha0_ypbindIsUp(char *dmn)
{
    int so;
    int yellow;

    if ((so = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return 0;
    close(so);
    yellow = !yp_bind(dmn);
    return(yellow);
}

char *
nha0_get_primary_interface_name(long hstid)
{
    int ss;
    struct ifconf ifc;
    struct ifreq  fir;
    char xbuf[MAX_BROADCAST_SIZE];
    int  i;
    struct sockaddr_in *sin1;
    char *intrfc;

    ss = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ss < 0) {
	return (char *)0;
    }
    bzero(&ifc, sizeof(ifc));
    bzero(&fir, sizeof(fir));
    bzero(xbuf, sizeof(xbuf));
    ifc.ifc_len = MAX_BROADCAST_SIZE;
    ifc.ifc_buf = xbuf;
    if(ioctl(ss, SIOCGIFCONF, (char *)&ifc) < 0) {
	close(ss);
	return (char *)0;
    }
    close(ss);

    i = 0;
    fir = ifc.ifc_req[i];
    while(strcmp(fir.ifr_name, "") ) {	/* I18N */
	sin1 = (struct sockaddr_in *)&fir.ifr_addr;
	if(sin1->sin_addr.s_addr == hstid)
	    break;
	fir = ifc.ifc_req[++i];
    }
    intrfc = strdup(fir.ifr_name);		/* I18N */
    return(intrfc);
}

u_long
nha0_getBroadcastAddr(char *interface)
{
    struct  sockaddr_in s_baddr;
    int af = AF_INET;
    struct ifreq gifr;
    int ss;

    bzero(&gifr, sizeof(gifr));
    strncpy(gifr.ifr_name, interface, IFNAMSIZ);	/* I18N */
    ss = socket(af, SOCK_DGRAM, 0);
    if (ss < 0) {
	return 0;
    }

    if (ioctl(ss, SIOCGIFBRDADDR, (caddr_t)&gifr) < 0) {
	close(ss);
	return 0;
    }
    s_baddr = *(struct sockaddr_in *)&gifr.ifr_addr;
    close(ss);

    return(s_baddr.sin_addr.s_addr);
}

u_long
nha0_getSubnetmask(char *interface)
{
    struct  sockaddr_in s_mask;
    int af = AF_INET;
    struct ifreq gifr;
    int ss;

    bzero(&gifr, sizeof(gifr));
    strncpy(gifr.ifr_name, interface, IFNAMSIZ);	/* I18N */
    ss = socket(af, SOCK_DGRAM, 0);
    if (ss < 0) {
	return 0;
    }

    if (ioctl(ss, SIOCGIFNETMASK, (caddr_t)&gifr) < 0) {
	close(ss);
	return 0;
    }
    s_mask = *(struct sockaddr_in *)&gifr.ifr_addr;
    close(ss);

    return(s_mask.sin_addr.s_addr);
}

/********

static u_long yp_server_list[256];
static int yp_server_cnt;

static int
yp_all_callback(int status, char *key, int kl, char *val, int vl, char *notused)
{
    struct hostent *hp;

    if(status == YP_TRUE) {
	if(hp = gethostbyname(key))
	    yp_server_list[yp_server_cnt++] = *((u_long *)hp->h_addr);
	return 0;
    }
    else
	return 1;
}

u_long *
nha0_get_nis_servers(char *nisdm)
{
    int err;
    struct ypall_callback cbinfo;

    yp_server_cnt = 0;
    cbinfo.foreach = yp_all_callback;
    cbinfo.data = (char *) NULL;
    err = yp_all(nisdm, "ypservers", &cbinfo);
    yp_server_list[yp_server_cnt] = 0;
    return (u_long *)yp_server_list;
}

********/

#define KERNBASE 0x80000000
/* #define KERNBASE 0x8000000000000000 */

struct nlist nl[] = {
    { "ifnet" },
    "",
};

static off_t
klseek(int kmem, off_t base)
{
    base &= ~KERNBASE;
    return (lseek(kmem, base, 0));
}

int
nha0_get_kmem_data(char *ifname, int *mtu)
{
    char *sysname;
    int kmem;
    off_t ifnetaddr;
    char xbuf[IFNAMSIZ];
    struct ifnet ifnet;
    char ybuf[IFNAMSIZ+2];
    int found = 0;

    if(alt_sysname)
	sysname = alt_sysname;
    else
	sysname = "/unix";
    /*
     * Seek into the kernel for a value.
     */
    kmem = open("/dev/kmem", 0);
    if (kmem < 0) {
	syslog(LOG_ERR,"Cannot open /dev/kmem (%m)");
	return 1;
    }
    nlist(sysname, nl);
    if (nl[0].n_type == 0) {
	syslog(LOG_ERR,"Cannot read system namelist (%m)");
	close(kmem);
	return 1;
    }

    ifnetaddr = nl[0].n_value;
    if(klseek(kmem, ifnetaddr) < 0) {
	syslog(LOG_ERR,"Error seeking in kmem (%m)");
	close(kmem);
	return 1;
    }
    if(read(kmem, &ifnetaddr, sizeof(ifnetaddr)) < 0) {
	syslog(LOG_ERR,"Error reading kmem: ifnetaddr (%m)");
	close(kmem);
	return 1;
    }
    for ( ;ifnetaddr != 0; ifnetaddr = (off_t) ifnet.if_next) {
	/* Read ifnet structure from kernel */
	klseek(kmem, ifnetaddr);
	if(read(kmem, &ifnet, sizeof ifnet) < 0) {
	    syslog(LOG_ERR,"Error reading kmem: ifnet (%m)");
	    close(kmem);
	    return 1;
	}
	klseek(kmem, (off_t)ifnet.if_name);
	if (read(kmem, xbuf, IFNAMSIZ-2) < 0) {
	    syslog(LOG_ERR,"Error reading kmem: if_name (%m)");
	    close(kmem);
	    return 1;
	}
	xbuf[IFNAMSIZ-2] = '\0';
	sprintf(ybuf, "%s%d", xbuf, ifnet.if_unit);
	if(strcmp(ifname, ybuf) == 0) {
	    found++;
#ifdef sgi
	    *mtu = ifnet.if_data.ifi_mtu;
#else
	    *mtu = ifnet.if_mtu;
#endif
	    break;
	}
    }
    close(kmem);

    if(found)
	return 0;
    return 1;
}
