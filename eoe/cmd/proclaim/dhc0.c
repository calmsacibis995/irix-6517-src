#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <syslog.h>
#include <sys/syssgi.h>
#include "dhcp_common.h"

extern char	*InterfaceHostName;
extern u_long	InterfaceAddr;
extern int	InterfaceNum;

static void
write_config_file(char *fname, char *ftype, char *str)
{
    FILE *fp;

    fp = fopen(fname, ftype);
    fputs(str, fp);
    fclose(fp);
}

static int
update_ifconfig_options(int ifnum, u_long s_mask)
{
    char fname[64];
    char fname1[64];
    FILE *fp, *fp1;
    char inbuf[256];
    char *otype = "netmask";
    int found = 0;
    char outbuf1[256];
    char outbuf2[256];
    char *p, *q, *s;
    int len, tlen, n, pt;

    tlen = strlen(otype);
    sprintf(fname, "/etc/config/ifconfig-%d.options", ifnum);
    if(access(fname, 0)) {	/* File does not exist */
	fp = fopen(fname, "w");
	if(!fp) return 1;
	fprintf(fp, "%s 0x%x\n", otype, s_mask);
	fclose(fp);
    }
    else {			/* File Exists */
	bzero(inbuf, 256);
	fp = fopen(fname, "r");
	if(!fp)
	    return 1;
	sprintf(fname1, "%s.tmp", fname);
	fp1 = fopen(fname1, "w");
	if(!fp1)
	    return 1;
	while (fgets(inbuf, 256, fp) != NULL) {
	    if (q = strstr(inbuf, otype)) {	/* found */
		p =inbuf;
		len = strlen(inbuf);
		if(inbuf[len-1] == '\n') {
		    inbuf[len-1] = '\0';
		    len--;
		}
		found = 1;
		pt = q-p+tlen;	/* upto "netmask" */
		strncpy(outbuf1, p, pt);
		outbuf1[pt] = '\0';
		sprintf(outbuf2, "%s 0x%x", outbuf1, s_mask); /* new mask */
		q += (tlen+1);
		n = strspn(q, " \t");
		q += n;
		n = strcspn(q, " \t\0"); /* upto space after the mask */
		q += n;
		if(*q == '\0')
		    fprintf(fp1, "%s\n", outbuf2);
		else
		    fprintf(fp1, "%s%s\n", outbuf2, q);
	    }
	    else {
		fprintf(fp1, "%s", inbuf);
	    }
	}
	if (!found)
	    fprintf(fp1, "%s 0x%x\n", otype, s_mask);
	fclose(fp);
	fclose(fp1);
	if (rename(fname1, fname) == -1) {
	    syslog(LOG_ERR, "Rename error %s to %s", fname1, fname);
	    return 1;
	}
    }
    return 0;
}

int
dhc0_setupTheConfiguration(char *hsname, u_long ipaddr, char *eaddr,
			   u_long smask, char *sname, u_long saddr,
			   u_long sdist_addr, char *nisdm,int start_nis,
			   char *dnsdm)
{
    char *ipchar = 0;
    char *serv_ipchar = 0;
    char *sdist_ipchar = 0;
    char *etheraddr;
    struct ether_addr ead;
    struct in_addr ipn, ipz;
    int rval;
    char commandbuf[256];
    char arg1[128], arg2[128];

    bcopy(eaddr, ead.ea_addr, 6);
    etheraddr = ether_ntoa(&ead);
    ipn.s_addr = ipaddr;
    ipchar = strdup(inet_ntoa(ipn));

    if(saddr) {
	ipn.s_addr = saddr;
	serv_ipchar = strdup(inet_ntoa(ipn));
    }

    if(sdist_addr) {
	ipn.s_addr = sdist_addr;
	sdist_ipchar = strdup(inet_ntoa(ipn));
    }

    rval = sys0_removeFromHostsFile(InterfaceAddr);
    if(rval) {
	ipz.s_addr = InterfaceAddr;
	syslog(LOG_ERR, "Error removing entry for host %s from the hosts file.",
		inet_ntoa(ipz) );
    }

    if (dnsdm != 0)
	rval = sys0_addToHostsFile(ipaddr, hsname, dnsdm);
    else if (start_nis)
	rval = sys0_addToHostsFile(ipaddr, hsname, nisdm);
    else
	rval = sys0_addToHostsFile(ipaddr, hsname, (char*)0);

    if(rval) {
	syslog(LOG_ERR, "Error adding entry for host %s to the hosts file.",
		hsname);
	exit(1);
    }
    if( sname && *sname && (gethostbyname(sname) == NULL) ) {
	if (dnsdm != 0)
	    rval = sys0_addToHostsFile(saddr, sname, dnsdm);
	else if (start_nis)
	    rval = sys0_addToHostsFile(saddr, sname, nisdm);
	else
	    rval = sys0_addToHostsFile(saddr, sname, (char*)0);
	if(rval) {
	    syslog(LOG_ERR,
		   "Warning: Cannot add entry for server %s to hosts file.",
		   sname);
	}
    }
    /* This first removes the old entry */
    rval = sys0_addToEthersFile(&ead, hsname);
    if(rval) {
	syslog(LOG_ERR, "Error adding entry for host %s to the ethers file.",
		hsname);
    }

    /* Check if the smask is non default, if so save it in the
     * ifconfig.options file.
     */
    /* smask : check if primary interface, or interface number specified */
    if(smask)
	update_ifconfig_options(InterfaceNum, smask);

    chmod("/.rhosts", 0600);
    if(sdist_ipchar) {
	sprintf(commandbuf, "%s\n", sdist_ipchar);
	write_config_file("/.rhosts", "a", commandbuf);
    }

    if(InterfaceNum == 1) {
	sprintf(commandbuf, "%s\n", hsname);
	write_config_file("/etc/sys_id", "w", commandbuf);

	sethostid(ipaddr);
	sethostname(hsname, strlen(hsname));

	strcpy(arg1, "netaddr");
	strcpy(arg2, ipchar);

	syssgi(SGI_SETNVRAM, arg1, arg2);
    }

    sprintf(commandbuf, "/etc/chkconfig network on");
    system(commandbuf);

    if( (InterfaceNum == 1) && start_nis) {
	if(nisdm && *nisdm) {
	    sprintf(commandbuf, "%s\n", nisdm);
	    write_config_file("/usr/etc/yp/ypdomain", "w", commandbuf);
	    /* YP Client configed with nsd under 6.5 */
	    /*
	     * sprintf(commandbuf, "/etc/chkconfig yp on");
	     * system(commandbuf);
	     */
	}
	sprintf(commandbuf, "/etc/chkconfig nfs on");
	system(commandbuf);
    }

    sprintf(commandbuf, "/etc/init.d/network stop");
    rval = system(commandbuf);

    /* This is nsd specific and required only for 6.5 release */
    sprintf(commandbuf, "/sbin/rm -rf /ns/.local/hosts*");
    rval = system(commandbuf);


    sprintf(commandbuf, "/etc/init.d/network start");
    rval = system(commandbuf);

    InterfaceAddr = ipaddr;
    if(InterfaceHostName)
    	free(InterfaceHostName);
    InterfaceHostName = strdup(hsname);

    if(ipchar)
    	free(ipchar);
    if(saddr && serv_ipchar)
	free(serv_ipchar);
    if(sdist_addr && sdist_ipchar)
	free(sdist_ipchar);
    return 0;
}

int
dhc0_shutdownNetwork()
{
    char commandbuf[256];
    int rval;

    sprintf(commandbuf, "/etc/chkconfig network off");
    system(commandbuf);

    sprintf(commandbuf, "/etc/init.d/network stop");
    rval = system(commandbuf);

    sprintf(commandbuf, "/etc/init.d/network start");
    system(commandbuf);
    return 0;
}

