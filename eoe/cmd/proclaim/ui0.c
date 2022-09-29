#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dhcp_common.h"

extern char  *InterfaceHostName;
extern u_long InterfaceAddr;
extern u_long deflt_hostid;

char *
ui0_confirm_hostname(char *assigned_name, char *msg, u_long off_ipa,
		     u_long off_smask, ulong *req_addr, int *typ)
{
    char new_name[MAXHOSTNAMELEN+2];
    char new_addr[20];
    char xb[3];
    char u_input[16];
    int len, ix;
    struct in_addr ipz;
    int msgtype = 0;
    int nameonly = 0;
    int cnt = 0;

    if(!assigned_name)
	return((char *)0);
    if(msg) {
	if((msg[1] == ':') || (msg[2] == ':')) {
	    xb[0] = msg[0];
	    if (msg[1] == ':') {
		xb[1] = '\0';
		fprintf(stdout, "\n\t%s\n\n", &msg[2]);
	    }
	    else {
		xb[1] = msg[1];
		xb[2] = 0;
		fprintf(stdout, "\n\t%s\n\n", &msg[3]);
	    }
	    msgtype = atoi(xb);
	}
	else {
	    fprintf(stdout, "\n\t%s\n\n", msg);
	}
    }
    if(msgtype != NAME_INUSE)
    	fprintf(stdout,
		"\n\tYour workstation has been named '%s'.\n\n",
		assigned_name);
    if((msgtype == NAME_INUSE) || (msgtype == ADDR_INUSE) ||
       (msgtype == ADDR_INVALID)) {
	cnt = 1;
	fprintf(stdout, "\tPlease make one of the following selections:\n\n");
	fprintf(stdout, "\t%d - To accept the Hostname '%s'\n",
		cnt++, assigned_name);
	fprintf(stdout, "\t%d - To enter a new Hostname\n", cnt++);
	fprintf(stdout,
		"\t%d - To enter a new Hostname and IP Address\n", cnt++);
	fprintf(stdout, "\t%d - To exit and disable DHCP in future\n", cnt++);
	if(InterfaceAddr != deflt_hostid) {
	    if( (off_ipa & off_smask) == (InterfaceAddr & off_smask) ) {
		ipz.s_addr = InterfaceAddr;
		fprintf(stdout,
		    "\t%d - To retain the Hostname '%s' and IP Address '%s'\n",
		    cnt, InterfaceHostName, inet_ntoa(ipz));
	    }
	    else {
		ipz.s_addr = off_ipa;
		fprintf(stdout,
		    "\t%d - To retain the Hostname '%s' and get new IP Address '%s'\n",
		    cnt, InterfaceHostName, inet_ntoa(ipz));
		nameonly++;
	    }
	}
      doagain:
	fprintf(stdout, "\tPlease enter [1-%d]: ", cnt);
	fgets(u_input, 16, stdin);
	if( (u_input && ((len = strlen(u_input)) > 2)) || (*u_input == '\n') ) {
	    fprintf(stdout, "\tInvalid Response\n");
	    goto doagain;
	}
	if(u_input[1] == '\n')
	    u_input[1] = '\0';
	ix = atoi(u_input);
	if( (ix < 1) || (ix > cnt) ) {
	    fprintf(stdout, "\tInvalid Response\n");
	    goto doagain;
	}
	    
	switch(ix) {
	    case 1:
		*typ = OFFERED_NAME;
		return (char *)0;
	    case 2:
		*typ = SELECTED_NAME;
		fprintf(stdout, "\tPlease enter the desired name: ");
		fgets(new_name, MAXHOSTNAMELEN, stdin);
		if(new_name && *new_name == '\n')
		    goto doagain;
		len = strlen(new_name);
		new_name[len -1] = '\0';
		return(strdup(new_name));
	    case 3:	/* Specify new name and Address */
		*typ = NEW_NAMEADDR;
		fprintf(stdout, "\tPlease enter the desired name: ");
		fgets(new_name, MAXHOSTNAMELEN, stdin);
		if(new_name && *new_name == '\n') {
		    fprintf(stdout, "\tInvalid Response\n");
		    goto doagain;
		}
		len = strlen(new_name);
		new_name[len -1] = '\0';
		fprintf(stdout,
			"\tPlease enter the IP Address in dot notation: ");
		fgets(new_addr, MAXHOSTNAMELEN, stdin);
		if(new_addr && *new_addr == '\n') {
		    fprintf(stdout, "\tInvalid Response\n");
		    goto doagain;
		}
		len = strlen(new_addr);
		new_addr[len -1] = '\0';
		if( (*req_addr = inet_addr(new_addr)) == INADDR_NONE) {
		    fprintf(stdout, "\tIllegaly formed IP Aaddress\n");
		    goto doagain;
		}
		if(off_ipa & off_smask != *req_addr & off_smask) {
		    fprintf(stdout,
			"\tRequested IP Address invalid for this Subnet.\n");
		    goto doagain;
		}
		return (strdup(new_name));
	    case 4:
		*typ = NO_DHCP;
		return (char *)0;
	    case 5:
		if(nameonly)
		    *typ = KEEP_OLD_NAMEONLY;
		else
		    *typ = KEEP_OLD_NAMEADDR;
		return (char *)0;
	    default:
		fprintf(stdout, "\tInvalid Response\n");
		goto doagain;
	}
    }
    else {
	fprintf(stdout, "\tIf this name is acceptable to you press <Enter>\n");
	fprintf(stdout, "\tOtherwise type in the desired name and then press <Enter> ");
	fgets(new_name, MAXHOSTNAMELEN, stdin);
	*typ = OFFERED_NAME;
	if(new_name && *new_name == '\n')
	    return((char *)0);
	len = strlen(new_name);
	new_name[len -1] = '\0';
	*typ = SELECTED_NAME;
	return (strdup(new_name));
    }
}

