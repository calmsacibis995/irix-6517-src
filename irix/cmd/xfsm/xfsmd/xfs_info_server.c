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

#ident "xfs_info_server.c: $Revision: 1.7 $"

/*
 *	The RPC server daemon implementation. The xfs system services
 *	are provided by the server. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/debug.h>
#include <rpc/rpc.h>
#include <rpc/svc.h>
#include <rpc/auth_unix.h>
#include <sys/dir.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <pwd.h>
#include <unistd.h>
#include "xfs_rpc_defs.h"
#include "xfs_utils.h"
#include "xfs_info_defs.h"
#include "xfs_info_server.h"

int doass = 1;

#ifdef DEBUG
	char debug_str[BUFSIZ];
#endif

extern int ruserok(char *rhost, int superuser, char *ruser, char *luser);

/*
 *      getlogname()
 *
 *      Returns logname corresponding to uid in rusername
 *      Assumes that rusername has enough memory 
 *      to hold logname
 *      
 *      Returns 1 on error 
 */
int
getlogname (char *rusername, int uid)
{
  struct passwd* pwEntry;

  /* Assumption is that uid's are in sync across the machines */
  pwEntry = getpwuid(uid);
  if (pwEntry)
    {
      strcpy (rusername, pwEntry->pw_name);
      return (0);
    }
  return (1);
}

/*
 *	authenticate()
 *      This routine authenticates the client depending
 *      on the level of security indicated by authtype
 *
 *      authtype = 0: check if client is trusted on server 
 *      authtype = 1: check if client has root privileges on
 *                    server 
 *	Return Value : 	0 (if authentication succeeds)
 *			1 (otherwise)
 */

int
authenticate(struct svc_req *rqstp, int authtype)
{
	int returnValue = 1; 
	struct authunix_parms *aup=NULL;
	char rusername[L_cuserid], lusername[L_cuserid] ;
	char hostname[MAXHOSTNAMELEN+1];
	char parsed_hostname[MAXHOSTNAMELEN+1];
	char domainname[MAXHOSTNAMELEN+1];
 
	/* get the local hostname */
	gethostname(hostname, MAXHOSTNAMELEN+1);
	strcpy(parsed_hostname, strtok (hostname, "."));

	/* append the local domainname */
	getdomainname(domainname,  MAXHOSTNAMELEN+1);
	strcat(parsed_hostname, ".");
	strcat(parsed_hostname, domainname);	

	/* check if credentials sent */
	switch (rqstp->rq_cred.oa_flavor) 
	{
		case AUTH_UNIX: {
			/* Get the authentication information */
			aup = (struct authunix_parms *) (rqstp->rq_clntcred);
			if (aup == NULL)
				break;

			/* bypass authentication if client is root on server machine */
			if ((aup->aup_uid == 0) &&
				(strcmp (parsed_hostname, aup->aup_machname) == 0))  
				return(0);
				
	
			switch (authtype)
			{
				/* Is client trusted on server */
				case 0:
					/* Get logname of client */
					if (getlogname (rusername, aup->aup_uid))
					{
						break;   /* Illegal remote uid */
					}

					if (!cuserid(lusername))
					{
						break; /* Unable to get process owners name */
					}
	  
	   
					if (!ruserok(aup->aup_machname, 0, rusername, lusername))
						returnValue = 0;
					break;

					/* Does client have root privileges on server machine */
				case 1:
					/* Get logname of client */
					if (getlogname (rusername, aup->aup_uid))
					{
						break;   /* Illegal remote uid */
					}

					if (!ruserok(aup->aup_machname, 1, rusername, "root"))
						returnValue = 0;

					break;

					/* Add any additional authentication schemes in case 2: etc. */

				default:
					break;
			}
			break;
		}

		case AUTH_DES:
		case AUTH_NULL:
		default:
			break;

	}
 
	return(returnValue);
}

/*
 *	xfsgetobj_1()
 *	The server implementation of the RPC call, getObj(). The
 *	parameters are extracted and the getObjectsInternal() function
 *	is invoked. The results are returned in the xdr structure,
 *	xfs_info_res.
 */

xfs_info_res *
xfsgetobj_1(criteria)
nametype *criteria;
{
	static xfs_info_res result;

	/* Free the memory allocated to store the results in the last 
	   invocation of this function */
	if ((result.info != NULL) && (result.warnmsg != NULL))
	{
		xdr_free(xdr_xfs_info_res,&result);
	}

	result.info = NULL;
	result.warnmsg = NULL;

#ifdef DEBUG
	dump_to_debug_file("getObjectsInternal()");
	dump_to_debug_file((const char*)*criteria);
#endif

	/* Execute the llocal function, getObjectsInternal() to get the
	   objects on the local host */
	result.errno = getObjectsInternal((const char*)*criteria,
					 (char**)&result.info,
					 (char**)&result.warnmsg);


	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.info == NULL)
	{
		result.info = (char*)malloc(1);
		memset(result.info,'\0',1);
	}
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*)malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Results");
	dump_to_debug_file(result.info);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif


	return(&result);
}

/*
 *	xfsgetinfo_1()
 *	The server implementation of the RPC call getInfo(). The
 *	parameters are extracted and the getInfoInternal() function is
 *	invoked. The results are returned in xfs_info_res strcuture.
 */

xfs_info_res *
xfsgetinfo_1(objectId)
nametype *objectId;
{
	static xfs_info_res result;

	/* Free the memory allocated to store the results in the last 
	   invocation of this function */
	if ((result.info != NULL) && (result.warnmsg != NULL))
	{
		xdr_free(xdr_xfs_info_res,&result);
	}

	result.info = NULL;
	result.warnmsg = NULL;

#ifdef DEBUG
	dump_to_debug_file("getInfoInternal()");
	dump_to_debug_file((const char*)*objectId);
#endif


	/* Invoke the local function to get information about the object */
	result.errno = getInfoInternal((const char*)*objectId,
					(char**)&result.info,
					(char**)&result.warnmsg);

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.info == NULL)
	{
		result.info = (char*)malloc(1);
		memset(result.info,'\0',1);
	}
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*)malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Results");
	dump_to_debug_file(result.info);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}

/*
 *	xfscreate_1()
 *	The server implementation of the RPC call, xfsCreate() to create
 *	an xfs filesystem. The filesystem name and other options are 
 *	extracted and the function xfsCreateInternal() is invoked to 
 *	create the filesystem on the local host. The warnings/error 
 *	messages are returned.
 */

xfs_result *
xfscreate_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

#ifdef DEBUG
	dump_to_debug_file("xfsCreateInternal()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


	/* Authenticate the client - must be root to create filesystem */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xfsCreateInternal() 
		   to create the filesystem on the host */
		result.errno = xfsCreateInternal(
					(const char*)*parameters,
					(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}

/*
 *	xfsedit_1()
 *	The server implementation of the RPC call, xfsEdit() to edit
 *	an xfs filesystem. The filesystem name and other options are 
 *	extracted and the function xfsEditInternal() is invoked to 
 *	edit the filesystem on the local host. The warnings/error 
 *	messages are returned.
 */

xfs_result *
xfsedit_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;

#ifdef DEBUG
	dump_to_debug_file("xfsEditInternal()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Authenticate the client - must be root to edit filesystem options*/
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xfsEditInternal() to edit the filesystem
		   on the host */
		result.errno = xfsEditInternal((const char*)*parameters,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}

/*
 *	xfsexport_1()
 *	The server implementation of the RPC call, xfsExport() to export 
 *	an filesystem. The function xfsExportInternal() is invoked to 
 *	perform the export operation on the local host. The 
 *	warnings/error messages are returned.
 */

xfs_result *
xfsexport_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xfsExportInternal()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Authenticate the client - must be root to export filesystem */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xfsExportInternal() to mount/export the 
		   filesystem on the host */
		result.errno = xfsExportInternal((const char*)*parameters,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}

/*
 *	xfsunexport_1()
 *	The server implementation of the RPC call, xfsUnexport() to 
 *	unexport an filesystem. The function xfsUnexportInternal() 
 *	is invoked to perform the unexport operation on the local host.
 *	The warnings/error messages are returned.
 */

xfs_result *
xfsunexport_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xfsUnexportInternal()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Authenticate the client - must be root to unexport filesystem */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xfsUnexportInternal() to unexport the 
		   filesystem on the host */
		result.errno = xfsUnexportInternal((const char*)*parameters,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}


/*
 *	xfsmount_1()
 *	The server implementation of the RPC call, xfsMount() to mount and/or
 *	export an xfs filesystem. The function xfsMountInternal() is invoked to 
 *	perform the mount/export operation on the local host. The 
 *	warnings/error messages are returned.
 */

xfs_result *
xfsmount_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xfsMountInternal()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Authenticate the client - must be root to mount filesystem */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xfsMountInternal() to mount/export the 
		   filesystem on the host */
		result.errno = xfsMountInternal((const char*)*parameters,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}

/*
 *	xfsunmount_1()
 *	The server implementation of the RPC call, xfsUnmount() to unmount 
 *	and/or unexport an xfs filesystem. The function xfsUnmountInternal() 
 *	is invoked to perform the unmount/unexport operation on the local host.
 *	The warnings/error messages are returned.
 */

xfs_result *
xfsunmount_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xfsUnmountInternal()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Authenticate the client - must be root to unmount filesystem */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xfsUnmountInternal() to 
		   unmount the filesystem on the host */
		result.errno = xfsUnmountInternal((const char*)*parameters,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}


/*
 *	xfsdelete_1()
 *	The server implementation of the RPC call, xfsDelete() to delete
 *	an xfs filesystem. The filesystem name is 
 *	extracted and the function xfsDeleteInternal() is invoked to 
 *	delete the filesystem on the local host. The warnings/error 
 *	messages are returned.
 */

xfs_result *
xfsdelete_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xfsDeleteInternal()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Authenticate the client - must be root to create filesystem */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xfsDeleteInternal() to delete 
		   the filesystem on the host */
		result.errno = xfsDeleteInternal((const char*)*parameters,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}

/*
 *	xfsgetpartitiontable_1()
 *	The server implementation of the RPC call to get the partition table
 *	of a specific disk. The diskname is extracted and the local function,
 *	get_partition_info() is invoked to get the data from the disk. 
 */

xfs_info_res *
xfsgetpartitiontable_1(parameters)
nametype *parameters;
{
	static xfs_info_res result;
	
	/* Free the memory allocated to store the results in the last 
	   invocation of this function */
	if ((result.info != NULL) && (result.warnmsg != NULL))
	{
		xdr_free(xdr_xfs_info_res,&result);
	}

	result.info = NULL;
	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("get_partition_info()");
	dump_to_debug_file((const char*)*parameters);
#endif

	result.info = get_partition_info(*parameters,(char**)&result.warnmsg);
	if (result.info != NULL)
	{
		result.errno = 0;
	}
	else
	{
		result.errno = 1;
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.info == NULL)
	{
		result.info = (char*)malloc(1);
		memset(result.info,'\0',1);
	}
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*)malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Results");
	dump_to_debug_file(result.info);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}

/*
 *	xfssetpartitiontable_1()
 *	The server implementation of the RPC call to update the partition table
 *	of a specific disk. The partition table is extracted and the local 
 *	function, update_partition_info() is invoked to update the partition
 *	table on the disk. 
 */


xfs_result *
xfssetpartitiontable_1(ptable,rqstp)
nametype *ptable;
struct svc_req *rqstp;
{
	static xfs_result result;
	int	returnVal;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages in the
	   last invocation of this function. */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;

#ifdef DEBUG
	dump_to_debug_file("update_partition_table()");
	dump_to_debug_file((const char*)*ptable);
#endif


	/* Authenticate the client - must be root to update partition table */
	if (authenticate(rqstp, 1)==0)
	{
		returnVal = update_partition_table(*ptable,
						(char**)&result.warnmsg);
		if (returnVal == 0)
		{
			result.errno = 0;
		}
		else
		{
			result.errno = 1;
		}
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}

/*
 *	xfsgroup_1()
 *	The server implementation of the RPC call xfsGroup(). The
 *	parameters are extracted and the xfsGroupInternal() function is
 *	invoked. The results are returned in xfs_info_res strcuture.
 */

xfs_info_res *
xfsgroup_1(param,rqstp)
nametype *param;
struct svc_req *rqstp;
{
	static xfs_info_res result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the results in the last 
	   invocation of this function */
	if ((result.info != NULL) && (result.warnmsg != NULL))
	{
		xdr_free(xdr_xfs_info_res,&result);
	}

	result.info = NULL;
	result.warnmsg = NULL;

#ifdef DEBUG
	dump_to_debug_file("xfsGroupInternal()");
	dump_to_debug_file((const char*)*param);
#endif


	/* Authenticate the client - must be root to modify groups */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the local function to perform the operation 
		   on the group */
		result.errno = xfsGroupInternal((const char*)*param,
						(char**)&result.info,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.info == NULL)
	{
		result.info = (char*)malloc(1);
		memset(result.info,'\0',1);
	}
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*)malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Results");
	dump_to_debug_file(result.info);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif


	return(&result);
}

/*
 *	xfsgethosts_1()
 *	The server implementation of the RPC call xfsGetHosts(). The
 *	parameters are extracted and the xfsGetHostsInternal() function is
 *	invoked. The results are returned in xfs_info_res strcuture.
 */

xfs_info_res *
xfsgethosts_1(param)
nametype *param;
{
	static xfs_info_res result;

	/* Free the memory allocated to store the results in the last 
	   invocation of this function */
	if ((result.info != NULL) && (result.warnmsg != NULL))
	{
		xdr_free(xdr_xfs_info_res,&result);
	}

	result.info = NULL;
	result.warnmsg = NULL;

#ifdef DEBUG
	dump_to_debug_file("xfsGetHostsInternal()");
	dump_to_debug_file((const char*)*param);
#endif


	/* Invoke the local function to get the hosts information */
	result.errno = xfsGetHostsInternal((const char*)*param,
					(char**)&result.info,
					(char**)&result.warnmsg);

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.info == NULL)
	{
		result.info = (char*)malloc(1);
		memset(result.info,'\0',1);
	}
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*)malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Results");
	dump_to_debug_file(result.info);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}

/*
 *	xfsgetdefaults_1()
 *	The server implementation of the RPC call xfsGetDefaults(). The
 *	parameters are extracted and the xfsGetDefaultsInternal() function is
 *	invoked. The results are returned in xfs_info_res strcuture.
 */

xfs_info_res *
xfsgetdefaults_1(param)
nametype *param;
{
	static xfs_info_res result;

	/* Free the memory allocated to store the results in the last 
	   invocation of this function */
	if ((result.info != NULL) && (result.warnmsg != NULL))
	{
		xdr_free(xdr_xfs_info_res,&result);
	}

	result.info = NULL;
	result.warnmsg = NULL;

#ifdef DEBUG
	dump_to_debug_file("xfsGetDefaultsInternal()");
	dump_to_debug_file((const char*)*param);
#endif


	/* Invoke the local function to get the default values */
	result.errno = xfsGetDefaultsInternal((const char*)*param,
					(char**)&result.info,
					(char**)&result.warnmsg);

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.info == NULL)
	{
		result.info = (char*)malloc(1);
		memset(result.info,'\0',1);
	}
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*)malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Results");
	dump_to_debug_file(result.info);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}


void
assfail(char* err_txt,char* file,int line_no)
{
        fprintf(stderr,"Internal error in %s at line %d\n",file,line_no);
        abort();
}

/*
 *	xfsxlvcmd_1()
 *	The server implementation of the RPC call xfsXlvCmd(). The
 *	parameters are extracted and the xfsXlvCmdInternal() function is
 *	invoked. The results are returned in xfs_info_res strcuture.
 */

xfs_info_res *
xfsxlvcmd_1(param,rqstp)
nametype *param;
struct svc_req *rqstp;
{
	static xfs_info_res result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the results in the last 
	   invocation of this function */
	if ((result.info != NULL) && (result.warnmsg != NULL))
	{
		xdr_free(xdr_xfs_info_res,&result);
	}

	result.info = NULL;
	result.warnmsg = NULL;

#ifdef DEBUG
	dump_to_debug_file("xfsXlvCmdInternal()");
	dump_to_debug_file((const char*)*param);
#endif


	/* Authenticate the client - must be root to execute xlv commands */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the local function to perform an XLV operation */
		/* TAC TAC TAC TAB
		result.errno = xfsXlvCmdInternal((const char*)*param,
						(char**)&result.info,
						(char**)&result.warnmsg);
		
		printf("TAC xfsXlvCmdInternal has been removed \n");*/
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.info == NULL)
	{
		result.info = (char*)malloc(1);
		memset(result.info,'\0',1);
	}
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*)malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Results");
	dump_to_debug_file(result.info);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}

/*
 *	xlvcreate_1()
 *	The server implementation of the RPC call, xfsXlvCreate() to create 
 *	an XLV object. The function xlvCreateInternal() is invoked to 
 *	perform the create operation on the local host. The 
 *	warnings/error messages are returned.
 */

xfs_result *
xlvcreate_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xlvCreateInternal()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Authenticate the client - must be root to create an XLV object */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xlvCreateInternal() to create
		   the XLV object on the host */
		result.errno = xlvCreateInternal((const char*)*parameters,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}





/*
 *	xlvdelete_1()
 *	The server implementation of the RPC call, xlvDelete() to delete 
 *	an XLV object. The function xlvDeleteInternal() is invoked to 
 *	perform the delete operation on the local host. The 
 *	warnings/error messages are returned.
 */

xfs_result *
xlvdelete_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xlvdelete_1()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Authenticate the client - must be root to delete XLV object */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xlvDeleteInternal() to delete the 
		   XLV object on the host */
		result.errno = xlvDeleteInternal((const char*)*parameters,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}



/*
 *	xlvdetach_1()
 *	The server implementation of the RPC call, xlvDetach() to detach 
 *	an XLV object. The function xlvDetachInternal() is invoked to 
 *	perform the detach operation on the local host. The 
 *	warnings/error messages are returned.
 */

xfs_result *
xlvdetach_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xlvdetach_1()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Authenticate the client - must be root to detach XLV object */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xlvDetachInternal() */
		result.errno = xlvDetachInternal((const char*)*parameters,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}


/*
 *	xlvattach_1()
 *	The server implementation of the RPC call, xlvAttach() to attach 
 *	an XLV object. The function xlvAttachInternal() is invoked to 
 *	perform the attach operation on the local host. The 
 *	warnings/error messages are returned.
 */

xfs_result *
xlvattach_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xlvattach_1()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Authenticate the client - must be root to attach XLV object */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xlvAttachInternal() */
		result.errno = xlvAttachInternal((const char*)*parameters,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}


/*
 *	xlvremove_1()
 *	The server implementation of the RPC call, xlvRemove() to remove 
 *	an XLV object. The function xlvRemoveInternal() is invoked to 
 *	perform the remove operation on the local host. The 
 *	warnings/error messages are returned.
 */

xfs_result *
xlvremove_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;
	char str[BUFSIZ];

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xlvremove_1()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Authenticate the client - must be root to remove XLV object */
	if (authenticate(rqstp, 1)==0)
	{
		/* Invoke the function, xlvRemoveInternal() */
		result.errno = xlvRemoveInternal((const char*)*parameters,
						(char**)&result.warnmsg);
	}
	else
	{
		result.errno = 1;
		sprintf(str,gettxt(":151",
			"Client must be root to execute this operation\n"));
		add_to_buffer((char**)&result.warnmsg,str);
	}

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}






/*
 *	xlvsynopsis_1()
 *	The server implementation of the RPC call, xlvSynopsis() to  
 *	get data of XLV object. The function xlvSynopsisInternal() is  
 *	invoked to perform the operation on the local host. The 
 *	warnings/error messages are returned.
 */

xfs_result *
xlvsynopsis_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xlvsynopsis_1()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Invoke the function, xlvSynopsisInternal() */
	result.errno = xlvSynopsisInternal((const char*)*parameters,
					(char**)&result.warnmsg);

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}


/*
 *	xlvinfo_1()
 *	The server implementation of the RPC call, xlvInfo() to generate 
 *	xlv_make(1m) needed to create XLV object. The function xlvInfoInternal() 
 *	is invoked to perform the export operation on the local host. The 
 *	warnings/error messages are returned.
 */

xfs_result *
xlvinfo_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("xlvinfo_1()");
	dump_to_debug_file((const char*)*parameters);
#endif

	/* Invoke the function, xlvInfoInternal() */
	result.errno = xlvInfoInternal((const char*)*parameters,
					(char**)&result.warnmsg);

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}

/*
 *	getpartsinuse_1()
 *	The server implementation of the RPC call, getPartsInUse() to 
 *	determine partition use list. The function getPartsUseInternal() 
 *	is invoked to perform the operation on the local host. The 
 *	warnings/error messages are returned.
 */

xfs_result *
getpartsinuse_1(parameters,rqstp)
nametype *parameters;
struct svc_req *rqstp;
{
	static xfs_result result;

	/* Free the memory allocated to store the error/warning messages
	   in the last invocation of this function */
	if (result.warnmsg != NULL)
	{
		xdr_free(xdr_xfs_result, &result);
	}

	result.warnmsg = NULL;


#ifdef DEBUG
	dump_to_debug_file("getpartsinuse_1()");
	dump_to_debug_file((const char*)*parameters);
#endif

	result.errno = getPartsUseInternal((const char*)*parameters,
					(char**)&result.warnmsg);

	/* Set the result structure fields to empty strings as strlen is
	   computed in the RPC layer and a NULL value will dump core :-( */
	if (result.warnmsg == NULL)
	{
		result.warnmsg = (char*) malloc(1);
		memset(result.warnmsg,'\0',1);
	}

#ifdef DEBUG
	sprintf(debug_str,"Return Value %d",result.errno);
	dump_to_debug_file(debug_str);
	dump_to_debug_file("Errors/Warnings");
	dump_to_debug_file(result.warnmsg);
	dump_to_debug_file("\n\n");
#endif

	return(&result);
}



/*
 *      xfsgetexportinfo_1()
 *      The server implementation of the RPC call xfsGetExportInfo(). The
 *      parameters are extracted and the xfsGetExportInfoInternal() function is
 *      invoked. The results are returned in xfs_info_res strcuture.
 */

xfs_info_res *
xfsgetexportinfo_1(param)
nametype *param;
{
        static xfs_info_res result;

        /* Free the memory allocated to store the results in the last
           invocation of this function */
        if ((result.info != NULL) && (result.warnmsg != NULL))
        {
                xdr_free(xdr_xfs_info_res,&result);
        }

        result.info = NULL;
        result.warnmsg = NULL;

        /* Invoke the local function to get the export information */
        result.errno = xfsGetExportInfoInternal((const char*)*param,
                                        (char**)&result.info,
                                        (char**)&result.warnmsg);

        /* Set the result structure fields to empty strings as strlen is
           computed in the RPC layer and a NULL value will dump core :-( */
        if (result.info == NULL)
        {
                result.info = (char*)malloc(1);
                memset(result.info,'\0',1);
        }
        if (result.warnmsg == NULL)
        {
                result.warnmsg = (char*)malloc(1);
                memset(result.warnmsg,'\0',1);
        }

#ifdef DEBUG
if (result.info != NULL)
{
	printf("DEBUG: xfsGetExportInfo() returns %s\n",result.info);
}
#endif

        return(&result);
}


/*
 *      xfssimplefsinfo_1()
 *      The server implementation of the RPC call xfsSimplefsInfo(). The
 *      parameters are extracted and the xfsSimpleFsInfoInternal() function is
 *      invoked. The results are returned in xfs_info_res strcuture.
 */

xfs_info_res *
xfssimplefsinfo_1(nametype *param)
{
        static xfs_info_res	result;

        /* Free the memory allocated to store the results in the last
           invocation of this function */
        if ((result.info != NULL) && (result.warnmsg != NULL)) {
                xdr_free(xdr_xfs_info_res,&result);
        }

        result.info = NULL;
        result.warnmsg = NULL;

        /* Invoke the local function to get the fs information */
        result.errno = xfsSimpleFsInfoInternal((const char*)*param,
                                        (char**)&result.info,
                                        (char**)&result.warnmsg);

        /* Set the result structure fields to empty strings as strlen is
           computed in the RPC layer and a NULL value will dump core :-( */
        if (result.info == NULL) {
                result.info = (char *)calloc(sizeof(char), 1);
        }
        if (result.warnmsg == NULL) {
                result.warnmsg = (char *)calloc(sizeof(char), 1);
        }

        return(&result);
}

xfs_info_res *
xfsgetbblk_1(nametype *objectId)
{
	static xfs_info_res result;

	/* Free the memory allocated to store the results in the last 
	 * invocation of this function
	 */
	if ((result.info != NULL) && (result.warnmsg != NULL)) {
		xdr_free(xdr_xfs_info_res, &result);
	}

	result.info = NULL;
	result.warnmsg = NULL;

	/* Invoke the local function to get information about the object */
	result.errno = xfsGetBBlkInternal((const char*)*objectId,
					(char**)&result.info,
					(char**)&result.warnmsg);

	/* Set the result structure fields to empty strings as strlen is
	 * computed in the RPC layer and a NULL value will dump core :-(
	 */
	if (result.info == NULL) {
		result.info = (char*)malloc(1);
		result.info[0] = '\0';
	}
	if (result.warnmsg == NULL) {
		result.warnmsg = (char*)malloc(1);
		result.warnmsg[0] = '\0';
	}

	return(&result);
}

xfs_info_res *
xfschkmounts_1(nametype *objectId)
{
	static xfs_info_res result;

	/* Free the memory allocated to store the results in the last 
	 * invocation of this function
	 */
	if ((result.info != NULL) && (result.warnmsg != NULL)) {
		xdr_free(xdr_xfs_info_res, &result);
	}

	result.info = NULL;
	result.warnmsg = NULL;

	/* Invoke the local function to get information about the object */
	result.errno = xfsChkMountsInternal((const char*)*objectId,
					(char**)&result.info,
					(char**)&result.warnmsg);

	/* Set the result structure fields to empty strings as strlen is
	 * computed in the RPC layer and a NULL value will dump core :-(
	 */
	if (result.info == NULL) {
		result.info = (char*)malloc(1);
		result.info[0] = '\0';
	}
	if (result.warnmsg == NULL) {
		result.warnmsg = (char*)malloc(1);
		result.warnmsg[0] = '\0';
	}

	return(&result);
}
