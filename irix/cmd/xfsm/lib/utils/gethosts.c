/*
 * Copyright (c) 1991 by Silicon Graphics, Inc.
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/debug.h>
#include <netdb.h>
#include <unistd.h>
#include "xfs_utils.h"
#include "xfs_get_hosts_key.h"

/*
 *	gethostbyipaddr()
 *	Stores in the info string the hostname corresponding to the IP address
 *	in addr.  The system file specified in string hostfile is looked up
 *	to obtain the hostname. If the IP address is located,
 *	the keyword XFS_GETHOSTS_HOST identfies each the hostname.
 *	Any errors encountered are logged in msg string.
 *	Return Value: 	0 success
 *			1 failure
 */

int
gethostbyipaddr(char* hostfile,char* addr,char** info,char** msg)
{
	char line[256];
	char adr[256];
	char *trailer=NULL;
	char *hostname=NULL;
	char localhost[BUFSIZ];
	register char *cp=NULL;
	FILE *fp=NULL;
	int size=0;
	int returnValue = 0;
	char str[BUFSIZ];

	if (hostfile == NULL)
	{
		sprintf(str,gettxt(":124",
				"Hostfile not specified for xfsGetHosts()\n"));
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else if (addr == NULL)
	{
		sprintf(str,gettxt(":126",
			"IP address not specified for xfsGetHosts()\n"));
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else if ((fp=fopen(hostfile,"r")) == NULL) 
	{
		sprintf(str, gettxt(":123",
				"Error opening file %s\n%s\n"),
				hostfile,
				strerror(errno));
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else
	{
		while (fgets(line, sizeof(line), fp)) 
		{
			struct in_addr in;

	                if ((cp = strpbrk(line, "#\n")) != NULL)
			{
                       		*cp = '\0';
			}

			if ((trailer=strpbrk(line," \t"))==NULL)
	                        continue;

	                sscanf(line, "%s", adr);
	                if (inet_isaddr(adr, &in.s_addr)) 
			{
				/* Compare the IP addresses */
				if (((hostname=strtok(trailer," \t"))
						!=NULL) &&
					(strcmp(inet_ntoa(in),addr)==0))
				{
					sprintf(str,"%s:%s\n",
						XFS_GETHOSTS_HOST_STR,
						hostname);
					add_to_buffer(info,str);
					break;
				}
			} 
		}

		fclose(fp);
	}

	return(returnValue);
}

/*
 *	getlocalhostipaddr()
 *	Stores in the info string the IP address of the local host. 
 *	The routine gethostname() gets the hostname of the local host. It
 *	is looked up in the given system file, hostfile.
 *	The keyword XFS_GET_SERVER_IP_ADDR gives the IP address of
 *	the local host.
 *	Any errors encountered are logged in msg string.
 *	Return Value: 	0 success
 *			1 failure
 */

int
getlocalhostipaddr(char* hostfile,char** info,char** msg)
{
	char line[256];
	char adr[256];
	char *trailer=NULL;
	char *hostname=NULL;
	char *short_host_name=NULL;
	char localhost[BUFSIZ];
	register char *cp=NULL;
	FILE *fp=NULL;
	int size=0;
	int returnValue = 0;
	char str[BUFSIZ];

	if (hostfile == NULL)
	{
		sprintf(str,gettxt(":124",
				"Hostfile not specified for xfsGetHosts()\n"));
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else if (gethostname(localhost,BUFSIZ)!=0)
	{
		sprintf(str, gettxt(":125",
			"Error getting local hostname in xfsGetHosts()\n%s\n"),
			strerror(errno));
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else if ((fp=fopen(hostfile,"r")) == NULL) 
	{
		sprintf(str, gettxt(":123",
				"Error opening file %s\n%s\n"),
				hostfile,
				strerror(errno));
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else
	{
		while (fgets(line, sizeof(line), fp)) 
		{
			struct in_addr in;

	                if ((cp = strpbrk(line, "#\n")) != NULL)
			{
                       		*cp = '\0';
			}

			if ((trailer=strpbrk(line," \t"))==NULL)
	                        continue;

	                sscanf(line, "%s", adr);
	                if (inet_isaddr(adr, &in.s_addr)) 
			{
				/* Print IP address */
				/*
				fputs(inet_ntoa(in), stdout);
				*/

				/* Compare the hostname in the long form
				   and then the short form as given 
				   in /etc/hosts */
				if ((((hostname=strtok(trailer," \t"))
						!=NULL) && 
					(strcmp(hostname,localhost)==0)) ||
				   (((short_host_name = strtok(NULL," \t"))
						!=NULL) &&
					(strcmp(short_host_name,localhost)==0)))
				{
					sprintf(str,"%s:%s\n",
						XFS_GET_SERVER_IP_ADDR_STR,
						inet_ntoa(in));
					add_to_buffer(info,str);
					break;
				}
			} 
		}

		fclose(fp);
	}

	return(returnValue);
}


/*
 *	gethostsinfo_local()
 *	Stores in the info string the list of hosts that could be reached 
 *	from the local machine by reading the hostfile. This is the system 
 *	file specified by the user and is typically /etc/hosts.
 *	The keyword XFS_GETHOSTS_HOST identfies each individual host.
 *	Any errors encountered are logged in msg string.
 *	Return Value: 	0 success
 *			1 failure
 */

int
gethostsinfo_local(char* hostfile,char** info,char** msg)
{
	char line[256];
	char adr[256];
	char *trailer=NULL;
	char *hostname=NULL;
	register char *cp=NULL;
	FILE *fp=NULL;
	int size=0;
	int returnValue = 0;
	char str[BUFSIZ];

	if (hostfile == NULL)
	{
		sprintf(str,gettxt(":124",
				"Hostfile not specified for xfsGetHosts()\n"));
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else if ((fp=fopen(hostfile,"r")) == NULL) 
	{
		sprintf(str, gettxt(":123",
				"Error opening file %s\n%s\n"),
				hostfile,
				strerror(errno));
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else
	{
		while (fgets(line, sizeof(line), fp)) 
		{
			struct in_addr in;

	                if ((cp = strpbrk(line, "#\n")) != NULL)
			{
                       		*cp = '\0';
			}

			if ((trailer=strpbrk(line," \t"))==NULL)
	                        continue;

	                sscanf(line, "%s", adr);
	                if (inet_isaddr(adr, &in.s_addr)) 
			{
				/* Print IP address */
				/*
				fputs(inet_ntoa(in), stdout);
				*/

				/* Print hostname */
				if ((hostname=strtok(trailer," \t"))
					!=NULL)
				{
					sprintf(str,"%s\n", hostname);
					add_to_buffer(info,str);
				}
			} 
		}

		fclose(fp);
	}

	return(returnValue);
}

/*
 *	gethostsinfo_nis()
 *	Stores in the info string the list of hosts that could be reached 
 *	from the local machine by reading the NIS database.
 *	The keyword XFS_GETHOSTS_HOST identfies each individual host.
 *	Any errors encountered are logged in msg string.
 *	Return Value: 	0 success
 *			1 failure
 */

int
gethostsinfo_nis(char** info,char** msg)
{
	struct hostent *host=NULL;
	int returnValue = 0;
	char str[BUFSIZ];

	/* Get the hostnames */ 
	while ((host = gethostent()) != NULL)
	{
		if (host->h_name != NULL)
		{
			sprintf(str,"%s\n", host->h_name);
			add_to_buffer(info,str);
		}
	}
	endhostent();

	return(returnValue);
}

/*
 *	gethostsinfo_netgrp()
 *	Stores in the info string the list of hosts that could be reached 
 *	from the local machine by reading the file /etc/netgroup.
 *	In case of netgroups, the output keywords are XFS_GETHOSTS_NGRP_NAME,
 *	XFS_GETHOSTS_NGRP_HOST, XFS_GETHOSTS_NGRP_USER and
 *	XFS_GETHOSTS_NGRP_DOMAIN. Each group starts with
 *	the keywords XFS_GETHOSTS_NGRP_NAME and ends with XFS_GETHOSTS_NGRP_END.
 *	The members of the group are enclosed with the above keywords.
 *	Each member is identified by the tuple (hostname,username,domainname)
 *	using keywords XFS_GETHOSTS_NGRP_HOST, XFS_GETHOSTS_NGRP_USER and
 *	XFS_GETHOSTS_NGRP_DOMAIN respectively.
 *
 *	Any errors encountered are logged in msg string.
 *	Return Value: 	0 success
 *			1 failure
 */

int
gethostsinfo_netgrp(char** info,char** msg)
{
	struct hostent *host=NULL;
	char**	machinep=NULL;
	char**	userp=NULL;
	char**	domainp=NULL;
	int 	returnValue = 0;
	char 	str[BUFSIZ];
	int	val;
	char 	line[256];
	char 	group_name[256];
	char*	trailer=NULL;
	register char *cp=NULL;
	FILE	*fp=NULL;

	/* Read the network group file */
	fp = fopen(ETC_NETGROUP_HOSTFILE, "r");
	if (fp == NULL) 
	{
		sprintf(str, gettxt(":123",
				"Error opening file %s\n%s\n"),
				ETC_NETGROUP_HOSTFILE,
				strerror(errno));
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else
	{
		while (fgets(line, sizeof(line), fp)) 
		{

	                if ((cp = strpbrk(line, "#\n")) != NULL)
			{
                       		*cp = '\0';
			}

			if ((trailer=strpbrk(line," \t"))==NULL)
	                        continue;

			/* Access a group at a time */
	                sscanf(line, "%s", group_name);
			setnetgrent(group_name);

			sprintf(str,"%s:%s\n",
					XFS_GETHOSTS_NGRP_NAME_STR,group_name);
			add_to_buffer(info,str);

			/* Get the group entries */ 
			while ((val=getnetgrent(machinep,userp,domainp)) == 1)
			{
				/* Store the network group */
				if (machinep == NULL)
				{
					sprintf(str,"%s:NULL\n",
						XFS_GETHOSTS_NGRP_HOST_STR);
					add_to_buffer(info,str);
				}
				else
				{
					sprintf(str,"%s:%s\n",
						XFS_GETHOSTS_NGRP_HOST_STR,
						*machinep);
					add_to_buffer(info,str);
				}

				if (userp == NULL)
				{
					sprintf(str,"%s:NULL\n",
						XFS_GETHOSTS_NGRP_USER_STR);
					add_to_buffer(info,str);
				}
				else
				{
					sprintf(str,"%s:%s\n",
						XFS_GETHOSTS_NGRP_USER_STR,
						*userp);
					add_to_buffer(info,str);
				}

				if (domainp == NULL)
				{
					sprintf(str,"%s:NULL\n",
						XFS_GETHOSTS_NGRP_DOMAIN_STR);
					add_to_buffer(info,str);
				}
				else
				{
					sprintf(str,"%s:%s\n",
						XFS_GETHOSTS_NGRP_DOMAIN_STR,
						*domainp);
					add_to_buffer(info,str);
				}
			}

			endnetgrent();

			sprintf(str,"%s:%s\n",
					XFS_GETHOSTS_NGRP_END_STR,group_name);
			add_to_buffer(info,str);

		}

		fclose(fp);
	}

	return(returnValue);
}
