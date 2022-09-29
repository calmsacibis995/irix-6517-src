/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *	The interface function for the RPC call to get hosts information.
 */

#ident "xfs_get_hosts.c: $Revision: 1.2 $"

#include <stdio.h>
#include <stdlib.h>
#include <sys/debug.h>
#include <string.h>
#include <pfmt.h>
#include "xfs_utils.h"
#include "xfs_get_hosts_key.h"

/*
 *	xfsGetHostsInternal()
 *	This function gets the list of hostnames from either the system file
 *	specified by the user such as /etc/hosts 
 *	or the NIS database depending on the option indicated. The list of
 *	network groups stored in /etc/netgroup can also be obtained using 
 *	this function. 
 *	The parameters string must include the keyword, value pair for
 *	the option namely, XFS_GETHOSTS_SRC. The defined options are
 *	ETC_HOSTS, NIS_HOSTS and ETC_NETGROUP.
 *
 *	The information obtained is returned as a series of keyword value pairs.
 *	The hostname is identified by keyword, XFS_GETHOSTS_HOST.
 *	In case of netgroups, the output keywords are XFS_GETHOSTS_NGRP_NAME,
 *	XFS_GETHOSTS_NGRP_HOST, XFS_GETHOSTS_NGRP_USER, XFS_LOCAL_HOSTS_FILE
 *	and XFS_GETHOSTS_NGRP_DOMAIN.
 *
 *	The warning/error messages in msg string are of the format
 *		{message1}{message2}.....
 *
 *	Return Value:
 *		0       Success
 *		1       Failure
 *		2       Partial success
 *
 */

int
xfsGetHostsInternal(const char* parameters, char **info, char **msg)
{
	int	returnVal = 0;
	char	*dup_parameters=NULL;
	char	*line=NULL;
	char	*key=NULL;
	char	*value=NULL;
	char	str[BUFSIZ];

	char	*source=NULL;
	char	*hostfile=NULL;
	char	*server_ip_addr_flag=NULL;
	char	*ip_addr=NULL;

	/* Check the input parameters */
	if ((parameters == NULL) || (info == NULL) || (msg == NULL))
	{
		sprintf(str,gettxt(":120",
		"Invalid parameters passed to xfsGetHostsInternal()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		*msg = NULL;
		*info = NULL;

		/* Make a copy of the parameters for string processing */
		dup_parameters = strdup(parameters);
		ASSERT(dup_parameters!=NULL);

		/* Parse the parameters string */
		for(line = strtok(dup_parameters, "\n");
		    line != NULL;
		    line = strtok(NULL, "\n")) {

			/* Extract the keyword and value pair */
			if(xfsmGetKeyValue(line, &key, &value)) {
				if (strcmp(key,XFS_GETHOSTS_SRC_STR)==0)
				{
					source = value;
				}
				else if (strcmp(key,
						XFS_LOCAL_HOSTS_FILE_STR)==0)
				{
					hostfile = value;
				}
				else if (strcmp(key,
						XFS_GET_SERVER_IP_ADDR_STR)==0)
				{
					server_ip_addr_flag = value;
				}
				else if (strcmp(key,
						XFS_GET_NAME_BY_IP_ADDR_STR)==0)
				{
					ip_addr = value;
				}
			}
		}

		if (source == NULL) 
		{
			sprintf(str,gettxt(":121",
				"Source to fetch hostnames not specified\n"));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else if (strcmp(source,ETC_HOSTS_STR)==0)
		{
			if ((server_ip_addr_flag != NULL) &&
				((strcmp(server_ip_addr_flag,"true")==0) ||
				(strcmp(server_ip_addr_flag,"TRUE")==0)))
			{
				/* Get the server ip address */
				returnVal = getlocalhostipaddr(hostfile,
								info,msg);
			}
			else if (ip_addr != NULL)
			{
				/* Get the hostname given the ip address */
				returnVal = gethostbyipaddr(hostfile,ip_addr,
								info,msg);
			}
			else
			{
				/* Get the hostnames from /etc/hosts file */
				returnVal = gethostsinfo_local(hostfile,
								info,msg);
			}
		}
		else if (strcmp(source,NIS_HOSTS_STR)==0)
		{
			/* Get the hostnames from NIS database */
			returnVal = gethostsinfo_nis(info,msg);
		}
		else if (strcmp(source,ETC_NETGROUP_STR)==0)
		{
			/* Get the network groups info from /etc/netgroup*/
			returnVal = gethostsinfo_netgrp(info,msg);
		}
		else
		{
			sprintf(str,gettxt(":122",
			"Unknown source %s specified for xfsGetHosts()\n"),
				source);
			add_to_buffer(msg,str);
			returnVal = 1;
		}

		if (dup_parameters != NULL)
		{
			free(dup_parameters);
		}
	}

	return(returnVal);
}
