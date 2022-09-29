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
 *	The interface functions for the GUI to xfs are implemented here.
 *	Each of the functions invokes the RPC call to perform the requested
 *	operation on a remote host.
 *	The interface functions implemented here are listed below:
 *	getObjects(), getInfo(), xfsCreate(), xfsEdit(), xfsDelete()
 *	xfsGetPartitionTable() and xfsSetPartitionTable().
 */

#ident  "xfs_info_client.c: $Revision: 1.7 $"

#include <stdio.h>
#include <string.h>
#include <pfmt.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <sys/xlv_base.h>
#include "xfs_rpc_defs.h"
#include "xfs_info_defs.h"
#include "xfs_info_server.h"
#include "xfs_fs_defs.h"
#include "xlv_cmd_key.h"
#include "xfs_info_client.h"

extern int errno;
int doass = 1;

/*
 *      getObjects(criteria, objs, msg)
 *
 *      This routine takes the criteria for finding the file systems/disks/
 *      XLV objects on the host & stores the matching objects in objs string.
 *      Any warnings/error messages are stored in msg.
 *
 *      The format of criteria is a series of keyword value pairs with
 *      newline as seperator.
 *              HOST_PATTERN:hostname
 *              OBJ_PATTERN:reg_expression
 *              OBJ_TYPE:fs/disk/vol
 *              QUERY_DEFN:{attribute_id:operator:value:query_clause_operator}
 *
 *      The objects are stored in the objs string in the format below.
 *              {hostname obj_type obj_name subtype}
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       success
 *              1       failure
 *              2       partial success
 *
 */

int 
getObjects(const char* criteria,char** objs,char** msg)
{
	CLIENT 	*cl=NULL;
	char	*server=NULL;
	xfs_info_res	*result=NULL;
	int     returnVal = 0;
	char    *line=NULL;
	char    *ptr_next_line=NULL;
	char    *keyword=NULL;
	char    *token=NULL;
	char    str[BUFSIZ];
	char	*dup_criteria=NULL;

	/* Retreive the host to get the information from */
	if ((criteria == NULL) || (objs == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":82",
			"Invalid parameters supplied to getObjects()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	/* Make a copy of criteria string as strtok() messes
	   it up */
	dup_criteria = strdup(criteria);
	assert(dup_criteria!=NULL);

	/* Parse the criteria to extract the hostname */
	line = strtok(dup_criteria,"\n");
	while (line != NULL) {
		ptr_next_line = (line + strlen(line) + 1);
		strncpy(str,line,BUFSIZ);
		keyword = strtok(str,":");
		if (keyword != NULL) {
			if (strcmp(keyword,HOST_PATTERN_KEY)==0) {
				if ((token=strtok(NULL,"\n"))!=NULL) {
					server = strdup(token);
					assert(server!=NULL);
				}
			}
		}

		/* Get the next line */
		line = strtok(ptr_next_line,"\n");
	}

	if (server == NULL) {
		sprintf(str,
		gettxt(":78","RPC server hostname not specified\n"));
		returnVal = 1;
	}
	else {

		if(xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
			returnVal = 1;
		}
		else if ((result = xfsgetobj_1(&criteria, cl)) == NULL) {
			sprintf(str,gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl,server));
			add_to_buffer(msg,str);
		}
		else {
			if (result->info != NULL) {
				*objs = (char*)strdup(result->info);
				assert(*objs!=NULL);
			}
			if (result->warnmsg != NULL) {
				*msg = (char*)strdup(result->warnmsg);
				assert(*msg!=NULL);
			}

			returnVal = result->errno;
			if (returnVal != 0 && result->warnmsg == NULL) {
				sprintf(str,gettxt(":83",
				"Error in getObjectsInternal()\n"));
				add_to_buffer(msg,str);
			}

			xdr_free(xdr_xfs_info_res,result);
		}
		free(server);
	}

	if (dup_criteria != NULL) {
		free(dup_criteria);
	}
	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *      getInfo(objectId, info, msg)
 *
 *      This routine returns information on the XFS object identified by
 *      objectId. The object may be a disk, filesystem or XLV object.
 *      Any warnings/error messages are stored in msg.
 *
 *      The format of objectId is shown below.
 *              {hostname obj_type obj_name subtype}
 *
 *      The information about the object is stored as a series of keyword
 *      value pairs with newline as seperator. The keyword is the attribute
 *      id and its corresponding immediately follows it.
 *              DISK_NAME:/dev/rdsk/ips0d3vh
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       success
 *              1       failure
 *              2       partial success
 *
 */

int 
getInfo(const char* objectId,char** objs,char** msg)
{
	CLIENT 		*cl = NULL;
	xfs_info_res	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		o_host [256];

	/* Retreive the host to get the information from */
	if ((objectId == NULL) || (objs == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":77",
			"Invalid parameters supplied to getInfo()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	*objs = NULL;
	*msg = NULL;

	if(xfsParseObjectSignature(FS_TYPE, objectId, o_host, NULL,
					NULL, NULL, msg) == B_FALSE) {
		return(1);
	}

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfsgetinfo_1(&objectId, cl)) == NULL) {
		sprintf(str, gettxt(":80",
			"Error in executing RPC call:%s\n"),
			clnt_sperror(cl, o_host));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else {
		if (result->info != NULL) {
			*objs = (char*) strdup(result->info);
			assert(*objs!=NULL);
		}
		if (result->warnmsg != NULL) {
			*msg = (char*) strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str, gettxt(":81",
			    "Error in getInfoInternal()\n")); 
			add_to_buffer(msg, str);
		}
		xdr_free(xdr_xfs_info_res, result);
	}

	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *      xfsCreate(objectId, parameters, msg)
 *
 *      A new filesystem is created by invoking the underlying
 *      the fs_create() function. The filesystem name is indicated in
 *	the string, objectId in the format
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is FS, obj_name is filesystem name.
 *
 *	The arguments are extracted from the parameters string.
 *      The format of parameters string is a series of keyword value pairs
 *	with newline as seperator. All the arguments pertinent to creating
 *	the filesystem must be specified in parameters string. For example,
 *	the mount directory of the filesystem is indicated as shown below.
 *		XFS_MNT_DIR:/d2
 *
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 *
 */

int 
xfsCreate(const char* objectId,const char* parameters,char** msg)
{
	CLIENT 		*cl = NULL;
	xfs_result	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		*params=NULL;

	char		o_host [256];
	char		o_name [256];
	char		o_class[32];
	char		o_type [32];

	/* Check the arguments */
	if ((objectId == NULL) || (parameters == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":84",
			"Invalid parameters supplied to xfsCreate()\n"));
		add_to_buffer(msg,str);
		return(1);
	}

	if(xfsParseObjectSignature(FS_TYPE, objectId, o_host, o_name,
					o_class, o_type, msg) == B_FALSE) {
		return(1);
	}

	/* The RPC call xfscreate_1() accepts a string with
	   keyword value pairs. Construct the params string
	   from filesystem name and parameters string supplied*/
	sprintf(str, "%s:%s\n", XFS_FS_NAME_STR, o_name);
	add_to_buffer(&params, str);
	add_to_buffer(&params, (char*)parameters);

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfscreate_1(&params, cl)) == NULL) {
		sprintf(str, gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl, o_host));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else {
		if (result->warnmsg != NULL) {
			*msg = strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str, gettxt(":85",
				"Error in xfsCreateInternal()\n"));
			add_to_buffer(msg,str);
		}
		xdr_free(xdr_xfs_result, result);
	}

	/* Release memory allocated to params string */
	if (params != NULL) {
		free(params);
	}
	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *      xfsEdit(objectId, parameters, msg)
 *
 *      An existing filesystem is modified by invoking the underlying
 *      the fs_edit() function. The objectId contains the filesystem name.
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is FS, obj_name is filesystem name.
 * 
 *	The arguments are extracted from the parameters string. The parameters
 *	string is a series of keyword, value pairs seperated by newline.
 *	
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 *
 */

int 
xfsEdit(const char* objectId, const char* parameters,char** msg)
{
	CLIENT 		*cl = NULL;
	xfs_result	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		*params=NULL;

	char		o_host [256];
	char		o_name [256];
	char		o_class[32];
	char		o_type [32];

	/* Retreive the hostname */
	if ((objectId == NULL) || (parameters == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":86",
			"Invalid parameters supplied to xfsEdit()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if(xfsParseObjectSignature(FS_TYPE, objectId, o_host, o_name,
					o_class, o_type, msg) == B_FALSE) {
		return(1);
	}

	/* The RPC call xfsedit_1() accepts a string with
	 * keyword value pairs. Construct the params string
	 * from filesystem name and parameters string supplied
	 */
	sprintf(str, "%s:%s\n", XFS_FS_NAME_STR, o_name);
	add_to_buffer(&params, str);
	add_to_buffer(&params, (char*)parameters);
	
	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfsedit_1(&params, cl)) == NULL) {
		sprintf(str,gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl, o_host));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else {
		if (result->warnmsg != NULL) {
			*msg = (char*) strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str,gettxt(":87",
				"Error in xfsEditInternal()\n"));
			add_to_buffer(msg,str);
		}
		xdr_free(xdr_xfs_result,result);
	}

	/* Release memory allocated to params string */
	if (params != NULL) {
		free(params);
	}
	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *	xfsExport(objectId,parameters, msg)
 *
 *	The specified filesystem is exported. 
 *	The fileesystem name is stored in the objectId string.
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is FS, obj_name is filesystem name.
 *
 *	The parameters string is a series of keyword value pairs seperated by
 *	newlines. The mount directory and the flag value are specified in
 *	parameters string.
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
xfsExport(const char* objectId, const char* parameters, char** msg)
{
	CLIENT 		*cl = NULL;
	xfs_result	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		*params=NULL;

	char		o_host [256];
	char		o_name [256];
	char		o_class[32];
	char		o_type [32];

#ifdef DEBUG
	if ((objectId != NULL) && (parameters != NULL))
	{
		printf("DEBUG: xfsExport() ObjectId %s Parameters %s\n",
				objectId, parameters);
	}
#endif

	/* Retreive the hostname */
	if ((objectId == NULL) || (parameters == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":113",
			"Invalid parameters supplied to xfsExport()\n"));
		add_to_buffer(msg,str);
		return(1);
	}

	if(xfsParseObjectSignature(FS_TYPE, objectId, o_host, o_name,
					o_class, o_type, msg) == B_FALSE) {
		return(1);
	}

	/* The RPC call xfsexport_1() accepts a string with
	 * keyword value pairs. Construct the params string
	 * from filesystem name, and parameters string 
	 * supplied
	 */
	sprintf(str, "%s:%s\n", XFS_FS_NAME_STR, o_name);
	add_to_buffer(&params,str);
	add_to_buffer(&params,(char*)parameters);

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfsexport_1(&params, cl))==NULL) {
		sprintf(str,gettxt(":80",
			"Error in executing RPC call:%s\n"),
			clnt_sperror(cl,o_host));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else {
		if (result->warnmsg != NULL) {
			*msg = (char*)strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str,gettxt(":114",
				"Error in xfsExportInternal()\n"));
			add_to_buffer(msg,str);
		}
		xdr_free(xdr_xfs_result,result);
	}

	/* Release memory allocated to params string */
	if (params != NULL) {
		free(params);
	}
	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *	xfsUnexport(objectId, parameters, msg)
 *
 *	The specified filesystem is unexported.
 *	The fileesystem name is stored in the objectId string.
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is FS, obj_name is filesystem name.
 *
 *	The parameters string is a series of keyword value pairs seperated by
 *	newlines. 
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
xfsUnexport(const char* objectId, const char* parameters, char** msg)
{
	CLIENT 		*cl = NULL;
	xfs_result	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		*params=NULL;
	char		o_host [256];
	char		o_name [256];
	char		o_class[32];
	char		o_type [32];

	/* Retreive the hostname */
	if ((objectId == NULL) || (parameters == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":115",
			"Invalid parameters supplied to xfsUnexport()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if(xfsParseObjectSignature(FS_TYPE, objectId, o_host, o_name,
					o_class, o_type, msg) == B_FALSE) {
		return(1);
	}

	/* The RPC call xfsunexport_1() accepts a string with
	 * keyword value pairs. Construct the params string
	 * from filesystem name and parameters string supplied
	 */
	sprintf(str, "%s:%s\n", XFS_FS_NAME_STR, o_name);
	add_to_buffer(&params,str);
	add_to_buffer(&params,(char*)parameters);

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfsunexport_1(&params, cl))==NULL) {
		sprintf(str,gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl,o_host));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else {
		if (result->warnmsg != NULL) {
			*msg = (char*)strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str,gettxt(":116",
				"Error in xfsUnexportInternal()\n"));
			add_to_buffer(msg,str);
		}
		xdr_free(xdr_xfs_result,result);
	}

	/* Release memory allocated to params string */
	if (params != NULL) {
		free(params);
	}
	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *	xfsMount(objectId,parameters, msg)
 *
 *	The specified filesystem is mounted and/or exported depending on
 *	the flag value. The filesystem name is stored in the objectId string.
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is FS, obj_name is filesystem name.
 *
 *	The parameters string is a series of keyword value pairs seperated by
 *	newlines. The mount directory and the flag value are specified in
 *	parameters string.
 *
 *	The different values of the flag are as follows.
 *		1 - mount filesystem, fs_name
 *		2 - export filesyste, fs_name
 *		3 - mount and export filesystem, fs_name
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
xfsMount(const char* objectId,const char* parameters,char** msg)
{
	CLIENT 		*cl = NULL;
	xfs_result	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		*params=NULL;

	char		o_host [256];
	char		o_name [256];
	char		o_class[32];
	char		o_type [32];

	/* Retreive the hostname */
	if ((objectId == NULL) || (parameters == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":94",
			"Invalid parameters supplied to xfsMount()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if(xfsParseObjectSignature(FS_TYPE, objectId, o_host, o_name,
					o_class, o_type, msg) == B_FALSE) {
		return(1);
	}

	/* The RPC call xfsmount_1() accepts a string with
	 * keyword value pairs. Construct the params string
	 * from filesystem name, type and parameters string 
	 * supplied
	 */
	sprintf(str, "%s:%s\n", XFS_FS_NAME_STR, o_name);
	add_to_buffer(&params,str);
	sprintf(str, "%s:%s\n", XFS_FS_TYPE_STR, o_type);
	add_to_buffer(&params, str);
	add_to_buffer(&params, (char*)parameters);

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfsmount_1(&params, cl))==NULL) {
		sprintf(str, gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl, o_host));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else {
		if (result->warnmsg != NULL) {
			*msg = (char*)strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str, gettxt(":95",
				"Error in xfsMountInternal()\n"));
			add_to_buffer(msg, str);
		}
		xdr_free(xdr_xfs_result,result);
	}

	/* Release memory allocated to params string */
	if (params != NULL) {
		free(params);
	}
	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *	xfsUnmount(objectId, parameters, msg)
 *
 *	The specified filesystem is unmounted and/or unexported depending on
 *	the flag value. The fileesystem name is stored in the objectId string.
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is FS, obj_name is filesystem name.
 *
 *	The parameters string is a series of keyword value pairs seperated by
 *	newlines. The mount directory and the flag value are specified in
 *	parameters string.
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
xfsUnmount(const char* objectId, const char* parameters,char** msg)
{
	CLIENT 		*cl = NULL;
	xfs_result	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		*params=NULL;
	char		o_host [256];
	char		o_name [256];
	char		o_class[32];
	char		o_type [32];

	/* Retreive the hostname */
	if ((objectId == NULL) || (parameters == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":96",
			"Invalid parameters supplied to xfsUnmount()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if(xfsParseObjectSignature(FS_TYPE, objectId, o_host, o_name,
					o_class, o_type, msg) == B_FALSE) {
		return(1);
	}

	/* The RPC call xfsunmount_1() accepts a string with
	 * keyword value pairs. Construct the params string
	 * from filesystem name and parameters string supplied
	 */
	sprintf(str, "%s:%s\n", XFS_FS_NAME_STR, o_name);
	add_to_buffer(&params, str);
	add_to_buffer(&params, (char*)parameters);

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfsunmount_1(&params, cl)) == NULL) {
		sprintf(str,gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl, o_host));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else {
		if (result->warnmsg != NULL) {
			*msg = (char*)strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str,gettxt(":97",
				"Error in xfsUnmountInternal()\n"));
			add_to_buffer(msg,str);
		}
		xdr_free(xdr_xfs_result, result);
	}

	/* Release memory allocated to params string */
	if (params != NULL) {
		free(params);
	}
	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *      xfsDelete(objectId,parameters, msg)
 *
 *      The filesystem is deleted by invoking the underlying
 *      the fs_delete() function. The filesystem name is extracted
 *      from the objectId string.
 *      The format of objectId string is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is FS, obj_name is filesystem name.
 *
 *	The parameters string contains the delete flag. The value 
 *	of this flag determines whether the /etc/fstab and 
 *	/etc/exports entries in the file need to be removed.
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 *
 */

int 
xfsDelete(const char* objectId,const char* parameters,char** msg)
{
	CLIENT 		*cl = NULL;
	xfs_result	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		*params=NULL;

	char		o_host [256];
	char		o_name [256];
	char		o_class[32];
	char		o_type [32];

	/* Retreive the hostname */
	if ((objectId == NULL) ||(parameters == NULL)||(msg == NULL)) {
		sprintf(str, gettxt(":88",
			"Invalid parameters supplied to xfsDelete()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if(xfsParseObjectSignature(FS_TYPE, objectId, o_host, o_name,
					o_class, o_type, msg) == B_FALSE) {
		return(1);
	}

	/* The RPC call xfsdelete_1() accepts a string with
	 * keyword value pairs. Construct the params string
	 * from filesystem name supplied
	 */
	sprintf(str, "%s:%s\n", XFS_FS_NAME_STR, o_name);
	add_to_buffer(&params, str);
	add_to_buffer(&params, (char*)parameters);

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfsdelete_1(&params, cl)) == NULL) {
		sprintf(str, gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl, o_host));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else {
		if (result->warnmsg != NULL) {
			*msg = (char*) strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str,gettxt(":89",
				"Error in xfsDeleteInternal()\n"));
			add_to_buffer(msg,str);
		}
		xdr_free(xdr_xfs_result,result);
	}
	/* Release memory allocated to params string */
	if (params != NULL) {
		free(params);
	}
	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *	xfsSimpleFsInfo(objectId, data_in, data_out, msg)
 *
 *      This routine gets simple information regrading a filesystem:
 *      mount directory, device name etc.
 *
 *      The filesystem name is stored in the objectId string.
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is FS, obj_name is filesystem name.
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
xfsSimpleFsInfo(const char *objectId,
		const char *data_in,
		char **data_out,
		char **msg)
{
	CLIENT 		*cl = NULL;
	xfs_info_res	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		*params=NULL;

	char		o_host [256];

	if ((objectId == NULL) || (data_in == NULL) ||
	    (data_out == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":167",
		"Invalid parameters supplied to xfsSimpleFsInfo()\n"));
		add_to_buffer(msg,str);
		return(1);
	}

	if(xfsParseObjectSignature(FS_TYPE, objectId, o_host, NULL,
					NULL, NULL, msg) == B_FALSE) {
		return(1);
	}

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfssimplefsinfo_1(&data_in, cl)) == NULL) {
		sprintf(str, gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl, o_host));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else {
		if (result->info != NULL) {
			*data_out = (char*)strdup(result->info);
			assert(*data_out!=NULL);
		}
		if (result->warnmsg != NULL) {
			*msg = (char*) strdup(result->warnmsg);
			assert(*msg!=NULL);
		}
		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str,gettxt(":89",
				"Error in xfssimplefsinfo_1()\n"));
			add_to_buffer(msg, str);
		}
		xdr_free(xdr_xfs_result,result);
	}

	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *      xfsGetBBlk(objectId, bblks, msg)
 *
 *      Return Value:
 *              0       success
 *              1       failure
 */
int 
xfsGetBBlk(const char *objectId, char **bblks, char **msg)
{
	CLIENT 		*cl = NULL;
	xfs_info_res	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		o_host [256];

	*bblks = NULL;
	*msg = NULL;

	/* Retreive the host to get the information from */
	if ((objectId == NULL) || (bblks == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":77",
			"Invalid parameters supplied to xfsGetBBlk()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if(xfsParseObjectSignature(DISK_TYPE, objectId, o_host, NULL,
					NULL, NULL, msg) == B_FALSE) {
		return(1);
	}

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfsgetbblk_1(&objectId, cl)) == NULL) {
		sprintf(str, gettxt(":80",
			"Error in executing RPC call:%s\n"),
			clnt_sperror(cl, o_host));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else {
		if (result->info != NULL) {
			*bblks = (char*) strdup(result->info);
			assert(*bblks!=NULL);
		}
		if (result->warnmsg != NULL) {
			*msg = (char*) strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str, gettxt(":81",
			    "Error in getBBlkInternal()\n")); 
			add_to_buffer(msg, str);
		}
		xdr_free(xdr_xfs_info_res, result);
	}

	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *      xfsGetPartitionTable(parameters, info, msg)
 *
 *      This routine reads the partition table for the specified disk
 *	on the host machine.
 *      The format of the parameters is shown below.
 *	{ host obj_type disk_name subtype }
 *
 *	The format of the info string which contains the partition table is
 *	a series of entries of the format below. Each entry corresponds to
 *	a partition and successive entries are seperated by newlines.
 *	{ partition_no partition_type start_block last_block }
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       success
 *              1       failure
 *              2       partial success
 *
 */

int
xfsGetPartitionTable(const char* objectId, char** ptable, char** msg)
{
	CLIENT 		*cl = NULL;
	xfs_info_res	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		*params=NULL;

	char		o_host [256];
	char		o_name [256];
	char		o_class[32];
	char		o_type [32];

	if ((objectId == NULL) || (ptable == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":90",
		"Invalid parameters supplied to xfsGetPartitionTable()\n"));
		add_to_buffer(msg,str);
		return(1);
	}

	if(xfsParseObjectSignature(DISK_TYPE, objectId, o_host, o_name,
					o_class, o_type, msg) == B_FALSE) {
		return(1);
	}

	/* Construct the parameters string in the format 
	 * expected by xfsgetpartitiontable_1 
	 * diskname:diskxyz
	 */
	sprintf(str, "%s:%s\n", "diskname", o_name);
	add_to_buffer(&params, str);

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfsgetpartitiontable_1(&params,cl))==NULL) {
		sprintf(str,gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl, o_name));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else {
		if (result->info != NULL) {
			*ptable = (char*)strdup(result->info);
			assert(*ptable!=NULL);
		}
		if (result->warnmsg != NULL) {
			*msg = (char*)strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str,gettxt(":91",
				"Error in xfsgetpartitiontable_1()\n"));
			add_to_buffer(msg,str);
		}

		xdr_free(xdr_xfs_info_res,result);
	}

	if (params != NULL) {
		free(params);
	}
	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *      xfsSetPartitionTable(parameters, info, msg)
 *
 *      This routine updates the partition table for the specified disk
 *	on the host machine.
 *      The format of the parameters is shown below.
 *	{ host obj_type disk_name subtype }
 *
 *	The format of the info string which contains the partition table is
 *	a series of entries of the format below. Each entry corresponds to
 *	a partition and successive entries are seperated by newlines.
 *	{ partition_no partition_type start_block last_block }
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       success
 *              1       failure
 *              2       partial success
 *
 */
int
xfsSetPartitionTable(const char* objectId, char* ptable, char** msg)
{
	CLIENT 		*cl = NULL;
	xfs_result	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		*params=NULL;

	char		o_host [256];
	char		o_name [256];
	char		o_class[32];
	char		o_type [32];

	if ((objectId == NULL) || (ptable == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":92",
		"Invalid parameters supplied to xfsSetPartitionTable()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if(xfsParseObjectSignature(DISK_TYPE, objectId, o_host, o_name,
					o_class, o_type, msg) == B_FALSE) {
		return(1);
	}

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfssetpartitiontable_1(&ptable,cl))==NULL) {
		sprintf(str,gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl, o_host));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else {
		if (result->warnmsg != NULL) {
			*msg = (char*)strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str, gettxt(":93",
				"Error in xfssetpartitiontable_1()\n"));
			add_to_buffer(msg,str);
		}
		xdr_free(xdr_xfs_result,result);
	}

	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *      xfsChkMounts(objectId, bblks, msg)
 *
 *      Return Value:
 *              0       success
 *              1       failure
 */
int 
xfsChkMounts(const char *objectId, char **info, char **msg)
{
	CLIENT 		*cl = NULL;
	xfs_info_res	*result = NULL;
	int		returnVal = 0;
	char		str[BUFSIZ];
	char		o_host [256];

	*info = NULL;
	*msg = NULL;

	/* Retreive the host to get the information from */
	if ((objectId == NULL) || (info == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":77",
		    "Invalid parameters supplied to xfsChkMounts()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if(xfsParseObjectSignature(DISK_TYPE, objectId, o_host, NULL,
					NULL, NULL, msg) == B_FALSE) {
		return(1);
	}

	if(xfsRpcCallInit(o_host, &cl, msg) == B_FALSE) {
		returnVal = 1;
	}
	else if ((result = xfschkmounts_1(&objectId, cl)) == NULL) {
		sprintf(str, gettxt(":80",
			"Error in executing RPC call:%s\n"),
			clnt_sperror(cl, o_host));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else {
		if (result->info != NULL) {
			*info = (char*) strdup(result->info);
			assert(*info!=NULL);
		}
		if (result->warnmsg != NULL) {
			*msg = (char*) strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str, gettxt(":81",
			    "Error in xfschkmounts_1()\n")); 
			add_to_buffer(msg, str);
		}
		xdr_free(xdr_xfs_info_res, result);
	}

	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *      xfsGroup(param, info, msg)
 *
 *	This function implements group management on any host. The various 
 *	operations defined on groups are creation,deletion,listing of a group. 
 *	In addition a new member can be added to the group or an existing 
 *	member deleted from the group. All the groups are saved in a user
 *	specified file. The parameters string indicates the file associated 
 *	with the group, the information associated with each individual operation
 *      through defined keyword value pairs. 
 *	The keywords defined for this function are listed below.
 *		XFS_HOSTNAME, XFS_GROUP_FILE, XFS_GROUP_NAME, XFS_GROUP_TYPE,
 *		XFS_GROUP_OPER.
 *	The different operations defined for groups are as follows.
 *		XFS_GROUP_LIST, XFS_GROUP_CREATE, XFS_GROUP_DELETE,
 *		XFS_GROUP_ADD_ELM and XFS_GROUP_RM_ELM.
 *
 *	The results are returned in the character string, info as keyword
 *	value pairs. 
 *      Any warnings/error messages are stored in msg.
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       success
 *              1       failure
 *              2       partial success
 *
 */
int 
xfsGroup(const char* param,char** info,char** msg)
{
	CLIENT 	*cl=NULL;
	char	*server=NULL;
	xfs_info_res	*result=NULL;
	int     returnVal = 0;
	char    *token=NULL;
	char    str[BUFSIZ];
	char	*dup_param=NULL;

	/* Check for valid parameters */
	if ((param == NULL) || (info == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":107",
			"Invalid parameters supplied to xfsGroup()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	*info = NULL;
	*msg = NULL;

	/* Make a copy of parameters string as strtok() messes it up */
	dup_param = strdup(param);
	assert(dup_param!=NULL);
		
	/* Retreive the host to get the information from */
	token = strtok((char*)dup_param,":");
	while (token != NULL)
	{
		/* Look for hostname keyword */
		if (strcmp(token,XFS_HOSTNAME_STR)==0)
		{
			/* Get the hostname */
			server = strtok(NULL,"\n");
		}
		else
		{
			/* Skip the value of the keyword */
			strtok(NULL,"\n");
		}

		/* Get the next keyword */
		token = strtok(NULL,":");
	}

	if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
	  returnVal = 1;
	}
	/* Execute the RPC call */
	else if ((result = xfsgroup_1(&param,cl))==NULL)
	{
		sprintf(str,gettxt(":80",
			"Error in executing RPC call:%s\n"),
			clnt_sperror(cl,server));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	/* Return the results */
	else
	{
		if (result->info != NULL) {
			*info = (char*)strdup(result->info);
			assert(*info!=NULL);
		}
		if (result->warnmsg != NULL) {
			*msg = (char*)strdup(result->warnmsg);
			assert(*msg!=NULL);
		}

		returnVal = result->errno;
		if (returnVal != 0 && result->warnmsg == NULL) {
			sprintf(str, gettxt(":108",
			"Error in xfsGroupInternal()\n"));
			add_to_buffer(msg, str);
		}
		xdr_free(xdr_xfs_info_res,result);
	}

	if (dup_param != NULL) {
		free(dup_param);
	}
	if(cl != NULL) {
		clnt_destroy(cl);
	}

	return(returnVal);
}

/*
 *      xfsGetHosts(param, info, msg)
 *
 *      This routine fetches the hostnames of all nodes that can be accessed
 *	from a particular host. This information can be obtained in different
 *	ways. They are the user specified hostfile containing the list 
 *	of hostnames, the NIS database and the network group information 
 *	in /etc/netgroup.
 *	The XFS_GETHOSTS_SRC keyword is used to indicate the method to 
 *	fetch the hostnames. The defined values are ETC_HOSTS, NIS_HOSTS and
 *	ETC_NETGROUP.
 *	The XFS_LOCAL_HOSTS_FILE keyword specifies the hosts file to lookup. 
 *	The param string contains the keyword, value pairs describing 
 *	these parameters.
 *	The results are returned in the character string, info as keyword
 *	value pairs. 
 *      Any warnings/error messages are stored in msg.
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       success
 *              1       failure
 *              2       partial success
 *
 */
int 
xfsGetHosts(const char* param,char** info,char** msg)
{
	CLIENT 	*cl=NULL;
	char	*server = NULL;
	xfs_info_res	*result=NULL;
	int     returnVal = 0;
	char    *token=NULL;
	char    str[BUFSIZ];
	char	*dup_param=NULL;

	/* Check for valid parameters */
	if ((param == NULL) || (info == NULL) || (msg == NULL))
	{
		sprintf(str,gettxt(":109",
			"Invalid parameters supplied to xfsGetHosts()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		*info = NULL;
		*msg = NULL;

		/* Make a copy of parameters string as strtok() messes
		   it up */
		dup_param = strdup(param);
		assert(dup_param!=NULL);
		
		/* Retreive the host to get the information from */
		token = strtok((char*)dup_param,":");
		while (token != NULL)
		{
			/* Look for hostname keyword */
			if (strcmp(token,XFS_HOSTNAME_STR)==0)
			{
				/* Get the hostname */
				server = strtok(NULL,"\n");
			}
			else
			{
				/* Skip the value of the keyword */
				strtok(NULL,"\n");
			}

			/* Get the next keyword */
			token = strtok(NULL,":");
		}

		if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
			returnVal = 1;
		}
		/* Execute the RPC call */
		else if ((result = xfsgethosts_1(&param,cl))==NULL)
		{
			sprintf(str,gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl,server));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		/* Return the results */
		else
		{
			if (result->info != NULL) {
				*info = (char*)strdup(result->info);
				assert(*info!=NULL);
			}
			if (result->warnmsg != NULL) {
				*msg = (char*)strdup(result->warnmsg);
				assert(*msg!=NULL);
			}

			returnVal = result->errno;
			if (returnVal != 0 && result->warnmsg == NULL) {
				sprintf(str, gettxt(":110",
				"Error in xfsGetHostsInternal()\n"));
				add_to_buffer(msg, str);
			}
			xdr_free(xdr_xfs_info_res,result);
		}

		if (dup_param != NULL) {
			free(dup_param);
		}
		if(cl != NULL) {
			clnt_destroy(cl);
		}
	}
	return(returnVal);
}

/*
 *      xfsGetDefaults(param, info, msg)
 *
 *	This routine gets the default values for a xfs filesystem type.
 *	The filesystem type is indicated by the keyword, XFS_FS_TYPE in
 *	string param.
 *	The default values corresponding to the options for the given
 *	filesystem type are returned as keyword value pairs in string, info.
 *
 *      Any warnings/error messages are stored in msg.
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       success
 *              1       failure
 *              2       partial success
 *
 */

int 
xfsGetDefaults(const char* param,char** info,char** msg)
{
	CLIENT 	*cl=NULL;
	char	*server=NULL;
	xfs_info_res	*result=NULL;
	int     returnVal = 0;
	char    *token=NULL;
	char    str[BUFSIZ];
	char	*dup_param=NULL;

	/* Check for valid parameters */
	if ((param == NULL) || (info == NULL) || (msg == NULL))
	{
		sprintf(str,gettxt(":109",
			"Invalid parameters supplied to xfsGetHosts()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		*info = NULL;
		*msg = NULL;

		/* Make a copy of parameters string as strtok() messes
		   it up */
		dup_param = strdup(param);
		assert(dup_param!=NULL);
		
		/* Retreive the host to get the information from */
		token = strtok((char*)dup_param,":");
		while (token != NULL)
		{
			/* Look for hostname keyword */
			if (strcmp(token,XFS_HOSTNAME_STR)==0)
			{
				/* Get the hostname */
				server = strtok(NULL,"\n");
			}
			else
			{
				/* Skip the value of the keyword */
				strtok(NULL,"\n");
			}

			/* Get the next keyword */
			token = strtok(NULL,":");
		}

		if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
			returnVal = 1;
		}
		/* Execute the RPC call */
		else if ((result = xfsgetdefaults_1(&param,cl))==NULL)
		{
			sprintf(str,gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl,server));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		/* Return the results */
		else
		{
			if (result->info != NULL) {
				*info = (char*)strdup(result->info);
				assert(*info!=NULL);
			}
			if (result->warnmsg != NULL) {
				*msg = (char*)strdup(result->warnmsg);
			}

			returnVal = result->errno;
			if (returnVal != 0 && result->warnmsg == NULL) {
				sprintf(str, gettxt(":112",
				"Error in xfsGetDefaultsInternal()\n"));
				add_to_buffer(msg, str);
			}
			xdr_free(xdr_xfs_info_res,result);
		}

		if (dup_param != NULL) {
			free(dup_param);
		}
		if(cl != NULL) {
			clnt_destroy(cl);
		}
	}
	return(returnVal);
}

/*
 *      xfsXlvCmd(param, info, msg)
 *
 *      This routine takes the xlv parameters and performs the specified
 *	operation. The defined operations are creating an xlv 
 *	object (XLV_CREATE_OBJ), modifying an xlv object (XLV_MODIFY_OBJ), 
 *	modifying xlv object state (XLV_MODIFY_OBJ_STATE), deleting an xlv
 *	object (XLV_DEL_OBJ) and deleting the xlv label (XLV_DEL_LABEL). 
 *	The keyword, XFS_XLV_CMD identifies the operation to be performed.
 *	The results are returned in the character string, info. 
 *      Any warnings/error messages are stored in msg.
 *
 *      The format of param is a series of keyword, value pairs specifying
 *	the xlv object to be created.
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       success
 *              1       failure
 *              2       partial success
 *
 */

int 
xfsXlvCmd(const char* param,char** info,char** msg)
{
	CLIENT 	*cl=NULL;
	char	*server=NULL;
	xfs_info_res	*result=NULL;
	int     returnVal = 0;
	char    *token=NULL;
	char    str[BUFSIZ];
	char	*dup_param=NULL;

	/* Check for valid parameters */
	if ((param == NULL) || (info == NULL) || (msg == NULL))
	{
		sprintf(str,
		gettxt(":105","Invalid parameters supplied to xfsXlvCmd()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		*info = NULL;
		*msg = NULL;

		/* Make a copy of parameters string as strtok() messes it up */
		dup_param = strdup(param);
		assert(dup_param!=NULL);
		
		/* Retreive the host to get the information from */
		token = strtok((char*)dup_param,":");
		while (token != NULL)
		{
			/* Look for hostname keyword */
			if (strcmp(token,XFS_HOSTNAME_STR)==0)
			{
				/* Get the hostname */
				server = strtok(NULL,"\n");
			}
			else
			{
				/* Skip the value of the keyword */
				strtok(NULL,"\n");
			}

			/* Get the next keyword */
			token = strtok(NULL,":");
		}

		if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
			returnVal = 1;
		}
		/* Execute the RPC call */
		else if ((result = xfsxlvcmd_1(&param,cl))==NULL)
		{
			sprintf(str,gettxt(":80",
				"Error in executing RPC call:%s\n"),
				clnt_sperror(cl,server));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		/* Return the results */
		else
		{
			if (result->info != NULL) {
				*info = (char*)strdup(result->info);
				assert(*info!=NULL);
			}
			if (result->warnmsg != NULL) {
				*msg = (char*)strdup(result->warnmsg);
				assert(*msg!=NULL);
			}

			returnVal = result->errno;
			if (returnVal != 0 && result->warnmsg == NULL) {
				sprintf(str, gettxt(":106",
				"Error in xfsXlvCmdInternal()\n"));
				add_to_buffer(msg, str);
			}
			xdr_free(xdr_xfs_info_res,result);
		}

		if (dup_param != NULL) {
			free(dup_param);
		}
		if(cl != NULL) {
			clnt_destroy(cl);
		}
	}
	return(returnVal);
}


void
assfail(char* err_txt,char* file,int line_no)
{
        fprintf(stderr,"Internal error in %s at line %d\n",file,line_no);
        abort();
}

/*
 *	deleteXlvObject(objectId, msg)
 *
 *	The specified XLV object is deleted. 
 *	The XLV object name is stored in the objectId string.
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is VOL, obj_name is XLV object name.
 *
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
deleteXlvObject(const char* objectId, char** msg)
{
	CLIENT 	*cl=NULL;
	char	server[256];
	xfs_result	*result=NULL;
	int     returnVal = 0;
	char	str[BUFSIZ];
	char	*params=NULL;
	char	fs_name[256];
	char	obj_type[32];
	char o_class[32];

#ifdef DEBUG
	printf("deleteXlvObject(%s)\n", objectId);
#endif

	/* Retreive the hostname */
	if ((objectId == NULL) || (msg == NULL))
	{
		sprintf(str,
		gettxt(":113","Invalid parameters supplied to deleteXlvObject()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		if (xfsParseObjectSignature(VOL_TYPE, objectId, server, fs_name,
				      o_class, obj_type, msg) == B_FALSE) {
			returnVal = 1;
		}
		else
		{
			/* The RPC call xlvdelete_1() accepts a string with
			   keyword value pair. Construct the params string
			   from volume name supplied */

			sprintf(str,"%s:%s\n","NAME",fs_name);
			add_to_buffer(&params,str);

			if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
			  returnVal = 1;
			}
			else if ((result = xlvdelete_1(&params, cl))==NULL)
			{
				sprintf(str,gettxt(":80",
					"Error in executing RPC call:%s\n"),
					clnt_sperror(cl,server));
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			else
			{
				if (result->warnmsg != NULL) {
					*msg = (char*)strdup(result->warnmsg);
					assert(*msg!=NULL);
				}

				returnVal = result->errno;
				if (returnVal != 0 && result->warnmsg == NULL) {
					sprintf(str,gettxt(":114",
					"Error in deleteXlvObject()\n"));
					add_to_buffer(msg, str);
				}
				xdr_free(xdr_xfs_result,result);
			}

			/* Release memory allocated to params string */
			if (params != NULL)
			{
				free(params);
			}
			if(cl != NULL) {
			  clnt_destroy(cl);
			}
		}
	}
	return(returnVal);
}

/*
 *	attachXlvObject(objectId, parameters, msg)
 *
 *	The specified xlv object is attached. 
 *	The XLV object name to be operated on is stored in the objectId string.
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is VOL, obj_name is XLV object name.
 *
 *	The parameters string is a series of keyword value pairs seperated by
 *	newlines. The plex number, ve number and the name of the
 *	xlv object to be attached may be specified in parameters string.
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
attachXlvObject(const char* objectId,const char* parameters,char** msg)
{
	CLIENT 	*cl=NULL;
	char	server[256];
	xfs_result	*result=NULL;
	int     returnVal = 0;
	char	str[BUFSIZ];
	char	*params=NULL;
	char	fs_name[256];
	char	obj_type[32];
	char o_class[32];

	/* Retreive the hostname */
	if ((objectId == NULL) || (parameters == NULL) || (msg == NULL))
	{
		sprintf(str,
		gettxt(":113","Invalid parameters supplied to attachXlvObject()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		if (xfsParseObjectSignature(VOL_TYPE, objectId, server, fs_name,
				      o_class, obj_type, msg) == B_FALSE) {
			returnVal = 1;
		}  
		else
		{
			/* The RPC call xlvattach_1() accepts a string with
			   keyword value pairs. Construct the params string
			   from xlv object name, and parameters string 
			   supplied */

			sprintf(str,"%s:%s\n","OBJ_NAME",fs_name);
			add_to_buffer(&params,str);
			add_to_buffer(&params,(char*)parameters);

			if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
				returnVal = 1;
			}
			else if ((result = xlvattach_1(&params, cl))==NULL)
			{
				sprintf(str,gettxt(":80",
					"Error in executing RPC call:%s\n"),
					clnt_sperror(cl,server));
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			else
			{
				if (result->warnmsg != NULL) {
					*msg = (char*)strdup(result->warnmsg);
					assert(*msg!=NULL);
				}

				returnVal = result->errno;
				if (returnVal != 0 && result->warnmsg == NULL) {
					sprintf(str,gettxt(":114",
					"Error in xlvAttachInternal()\n"));
					add_to_buffer(msg, str);
				}
				xdr_free(xdr_xfs_result,result);
			}

			/* Release memory allocated to params string */
			if (params != NULL)
			{
				free(params);
			}
			if(cl != NULL) {
			  clnt_destroy(cl);
			}
		}

	}
	return(returnVal);
}


/*
 *	detachXlvObject(objectId,parameters, msg)
 *
 *	The specified XLV object is detached. 
 *	The XLV object to be operated on is stored in the objectId string.
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is VOL, obj_name is XLV object name.
 *
 *	The parameters string is a series of keyword value pairs separated by
 *	newlines. The subvolume, plex, ve number and name of the detached xlv
 *	object may be specified in parameters string.
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
detachXlvObject(const char* objectId,const char* parameters,char** msg)
{
	CLIENT 	*cl=NULL;
	
	xfs_result	*result=NULL;
	int     returnVal = 0;
	char	server[256];
	char	str[BUFSIZ];
	char	*params=NULL;
	char	fs_name[256];
	char	obj_type[32];
	char o_class[32];

	/* Retreive the hostname */
	if ((objectId == NULL) || (parameters == NULL) || (msg == NULL))
	{
		sprintf(str,
		gettxt(":113","Invalid parameters supplied to detachXlvObject()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		if (xfsParseObjectSignature(VOL_TYPE, objectId, server, fs_name,
				      o_class, obj_type, msg) == B_FALSE) {
			returnVal = 1;
		}
		else
		{
			/* The RPC call xlvdetach_1() accepts a string with
			   keyword value pairs. Construct the params string
			   from XLV object name, and parameters string 
			   supplied */

			sprintf(str,"%s:%s\n","OBJ_NAME",fs_name);
			add_to_buffer(&params,str);
			add_to_buffer(&params,(char*)parameters);

			if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
				returnVal = 1;
			}
			else if ((result = xlvdetach_1(&params, cl))==NULL)
			{
				sprintf(str,gettxt(":80",
					"Error in executing RPC call:%s\n"),
					clnt_sperror(cl,server));
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			else
			{
				if (result->warnmsg != NULL) {
					*msg = (char*)strdup(result->warnmsg);
					assert(*msg!=NULL);
				}

				returnVal = result->errno;
				if (returnVal != 0 && result->warnmsg == NULL) {
					sprintf(str,gettxt(":114",
					"Error in detachXlvObject()\n"));
					add_to_buffer(msg, str);
				}
				xdr_free(xdr_xfs_result,result);
			}

			/* Release memory allocated to params string */
			if (params != NULL)
			{
				free(params);
			}
			if(cl != NULL) {
			  clnt_destroy(cl);
			}
		}

	}
	return(returnVal);
}


/*
 *	removeXlvObject(objectId,parameters, msg)
 *
 *	The specified xlv object is detached and removed.
 *	The xlv object name operated on is stored in the objectId string.
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is VOL, obj_name is the name of the xlv object operated on.
 *
 *	The parameters string is a series of keyword value pairs seperated by
 *	newlines. The subvolume, plex and ve number are specified in
 *	parameters string.
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
removeXlvObject(const char* objectId,const char* parameters,char** msg)
{
	CLIENT 	*cl=NULL;
	char	server[256];
	xfs_result	*result=NULL;
	int     returnVal = 0;
	char	str[BUFSIZ];
	char	*params=NULL;
	char	fs_name[256];
	char	obj_type[32];
	char o_class[32];

	/* Retreive the hostname */
	if ((objectId == NULL) || (parameters == NULL) || (msg == NULL))
	{
		sprintf(str,
		gettxt(":113","Invalid parameters supplied to removeXlvObject()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		if (xfsParseObjectSignature(VOL_TYPE, objectId, server, fs_name,
				      o_class, obj_type, msg) == B_FALSE) {
			returnVal = 1;
		}  
		else
		{
			/* The RPC call xlvremove_1() accepts a string with
			   keyword value pairs. Construct the params string
			   from XLV object name, and parameters string 
			   supplied */

			sprintf(str,"%s:%s\n","OBJ_NAME",fs_name);
			add_to_buffer(&params,str);
			add_to_buffer(&params,(char*)parameters);

			if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
				returnVal = 1;
			}
			else if ((result = xlvremove_1(&params, cl))==NULL)
			{
				sprintf(str,gettxt(":80",
					"Error in executing RPC call:%s\n"),
					clnt_sperror(cl,server));
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			else
			{
				if (result->warnmsg != NULL) {
					*msg = (char*)strdup(result->warnmsg);
					assert(*msg!=NULL);
				}

				returnVal = result->errno;
				if (returnVal != 0 && result->warnmsg == NULL) {
					sprintf(str,gettxt(":114",
					"Error in xfsRemoveInternal()\n"));
					add_to_buffer(msg, str);
				}
				xdr_free(xdr_xfs_result,result);
			}

			/* Release memory allocated to params string */
			if (params != NULL)
			{
				free(params);
			}
			if(cl != NULL) {
			  clnt_destroy(cl);
			}
		}

	}
	return(returnVal);
}


/*
 *	createXlvObject(objectId,parameters, msg)
 *
 *	The specified XLV object is created.
 *	The XLV object name is stored in the objectId string.
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is VOL, obj_name is XLV object name.
 *
 *	The parameters string is a series of keyword value pairs seperated by
 *	newlines.
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
createXlvObject(const char* objectId,const char* parameters,char** msg)
{
	CLIENT 	*cl=NULL;
	char	server[256];
	xfs_result	*result=NULL;
	int     returnVal = 0;
	char	str[BUFSIZ];
	char	*params=NULL;
	char	fs_name[256];
	char	obj_type[32];
	char o_class[32];

	/* Retreive the hostname */
	if ((objectId == NULL) || (parameters == NULL) || (msg == NULL))
	{
		sprintf(str,
		gettxt(":113","Invalid parameters supplied to createXlvObject()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		if (xfsParseObjectSignature(VOL_TYPE, objectId, server, fs_name,
				      o_class, obj_type, msg) == B_FALSE) {
			returnVal = 1;
		}
		else
		{
			/* The RPC call xlvcreate_1() accepts a string with
			   keyword value pairs. Construct the params string
			   from xlv object name, and parameters string 
			   supplied */

/* TAC
			sprintf(str,"%s:%s\n","NAME",fs_name);
			add_to_buffer(&params,str);
*/
			add_to_buffer(&params,(char*)parameters);

			if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
				returnVal = 1;
			}
			else if ((result = xlvcreate_1(&parameters, cl))==NULL)
			{
				sprintf(str,gettxt(":80",
					"Error in executing RPC call:%s\n"),
					clnt_sperror(cl,server));
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			else
			{
				if (result->warnmsg != NULL) {
					*msg = (char*)strdup(result->warnmsg);
					assert(*msg!=NULL);
				}

				returnVal = result->errno;
				if (returnVal != 0 && result->warnmsg == NULL) {
					sprintf(str,gettxt(":114",
					"Error in createXlvObject()\n"));
					add_to_buffer(msg, str);
				}
				xdr_free(xdr_xfs_result,result);
			}

			/* Release memory allocated to params string */
			if (params != NULL)
			{
				free(params);
			}
			if(cl != NULL) {
			  clnt_destroy(cl);
			}
		}

	}
	return(returnVal);
}





/*
 *	getXlvSynopsis(objectId, msg)
 *
 *	This routine returns information on the XLV object identified by
 *      objectId. The object may be a volume, plex or ve.
 *
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is VOL, obj_name is XLV object name.
 *
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
getXlvSynopsis(const char* objectId, char** msg)
{
	CLIENT 	*cl=NULL;
	char	server[256];
	xfs_result	*result=NULL;
	int     returnVal = 0;
	char	str[BUFSIZ];
	char	*params=NULL;
	char	fs_name[256];
	char	obj_type[32];
	char o_class[32];

#ifdef DEBUG
	printf("Entering getXlvSynopsis: %s\n", objectId);
#endif

	/* Retreive the hostname */
	if ((objectId == NULL) || (msg == NULL))
	{
		sprintf(str,
		gettxt(":113","Invalid parameters supplied to getXlvSynopsis()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		if (xfsParseObjectSignature(VOL_TYPE, objectId, server, fs_name,
				      o_class, obj_type, msg) == B_FALSE) {
			returnVal = 1;
		}  
		else
		{
			/* The RPC call xlvsynopsis_1() accepts a string with
			   keyword value pairs. Construt the params string
			   from object name */

			sprintf(str,"%s:%s\n","OBJ_NAME",fs_name);
			add_to_buffer(&params,str);

			if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
				returnVal = 1;
			}
			else if ((result = xlvsynopsis_1(&params, cl))==NULL)
			{

				sprintf(str,gettxt(":80",
					"Error in executing RPC call:%s\n"),
					clnt_sperror(cl,server));
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			else
			{
				if (result->warnmsg != NULL) {
					*msg = (char*)strdup(result->warnmsg);
					assert(*msg!=NULL);
				}

				returnVal = result->errno;
				if (returnVal != 0 && result->warnmsg == NULL) {
					sprintf(str,gettxt(":114",
					"Error in getSynopsisInternal()\n"));
					add_to_buffer(msg, str);
				}
				xdr_free(xdr_xfs_result,result);
			}

			/* Release memory allocated to params string */
			if (params != NULL)
			{
				free(params);
			}
			if(cl != NULL) {
			  clnt_destroy(cl);
			}
		}
	}

	return(returnVal);
}


/*
 *	infoXlvObject(objectId, msg)
 *      
 *      This routine generates the xlv_make(1m) needed to create 
 *      the specified xlv object
 *	
 *	The format of the objectId is
 *		{ hostname obj_type obj_name subtype }
 *	where obj_type is VOL, obj_name is XLV object name.
 *
 *	The parameters string is a series of keyword value pairs seperated by
 *	newlines. The mount directory and the flag value are specified in
 *	parameters string.
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
infoXlvObj(const char* objectId, char** msg)
{
	CLIENT 	*cl=NULL;
	char	server[256];
	xfs_result	*result=NULL;
	int     returnVal = 0;
	char	str[BUFSIZ];
	char	*params=NULL;
	char	fs_name[256];
	char	obj_type[32];
	char o_class[32];

#ifdef DEBUG
	printf("TAC****** Entered infoXlvObj objectid =%s \n",objectId);
#endif

	/* Retreive the hostname */
	if ((objectId == NULL) || (msg == NULL))
	{
		sprintf(str,
		gettxt(":113","Invalid parameters supplied to infoXlvObject()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		if (xfsParseObjectSignature(VOL_TYPE, objectId, server, fs_name,
				      o_class, obj_type, msg) == B_FALSE) {
			returnVal = 1;
		}
		else
		{
			/* The RPC call xlvinfo_1() accepts a string with
			   keyword value pairs. Construct the params string
			   from XLV object name, and parameters string 
			   supplied */

			sprintf(str,"%s:%s\n","OBJ_NAME",fs_name);
			add_to_buffer(&params,str);
#ifdef DEBUG
			printf("TAC infoXlvObj: params =%s \n",params);
#endif
			if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
				returnVal = 1;
			}
			else if ((result = xlvinfo_1(&params, cl))==NULL)
			{
				sprintf(str,gettxt(":80",
					"Error in executing RPC call:%s\n"),
					clnt_sperror(cl,server));
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			else
			{
				if (result->warnmsg != NULL) {
					*msg = (char*)strdup(result->warnmsg);
					assert(*msg!=NULL);
				}

				returnVal = result->errno;
				if (returnVal != 0 && result->warnmsg == NULL) {
					sprintf(str,gettxt(":114",
					"Error in xfsinfoInternal()\n"));
					add_to_buffer(msg, str);
				}
				xdr_free(xdr_xfs_result,result);
			}

			/* Release memory allocated to params string */
			if (params != NULL)
			{
				free(params);
			}
			if(cl != NULL) {
			  clnt_destroy(cl);
			}
		}

	}
	return(returnVal);
}






/*
 *	getPartsInUse(objectId, msg)
 *
 *	This routine retrieves the partition use list.
 *
 *	The objectId parameter is not used currently
 *
 *	All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
getPartsInUse(const char* objectId, char** msg)
{
	CLIENT 	*cl=NULL;
	char	server[256];
	xfs_result	*result=NULL;
	int     returnVal = 0;
	char	str[BUFSIZ];
	char	*params=NULL;
	char	fs_name[256];
	char	obj_type[32];
	char o_class[32];

#ifdef DEBUG
	printf("TAC****** Entered getPartsInUse objectid =%s \n",objectId);
#endif

	/* Retreive the hostname */
	if ((objectId == NULL) || (msg == NULL))
	{
		sprintf(str,
		gettxt(":113","Invalid parameters supplied to infoXlvObject()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		if (xfsParseObjectSignature(VOL_TYPE, objectId, server, fs_name,
				      o_class, obj_type, msg) == B_FALSE) {
			returnVal = 1;
		}
		else
		{
			/* The RPC call getpartsinuse_1() accepts a string with
			   keyword value pairs */

			sprintf(str,"%s:%s\n","OBJ_NAME",fs_name);
			add_to_buffer(&params,str);
#ifdef DEBUG
			printf("TAC params =%s \n",params);
#endif
			if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
				returnVal = 1;
			}
			else if ((result = getpartsinuse_1(&params, cl))==NULL)
			{
				sprintf(str,gettxt(":80",
					"Error in executing RPC call:%s\n"),
					clnt_sperror(cl,server));
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			else
			{
				if (result->warnmsg != NULL) {
					*msg = (char*)strdup(result->warnmsg);
					assert(*msg!=NULL);
				}

				returnVal = result->errno;
				if (returnVal != 0 && result->warnmsg == NULL) {
					sprintf(str,gettxt(":114",
					"Error in getPartsInUse()\n"));
					add_to_buffer(msg, str);
				}
				xdr_free(xdr_xfs_result,result);
			}

			/* Release memory allocated to params string */
			if (params != NULL)
			{
				free(params);
			}
			if(cl != NULL) {
			  clnt_destroy(cl);
			}
		}

	}
	return(returnVal);
}



/*
 *      xfsGetExportInfo(param,info, msg)
 *
 *      This routine reads the /etc/exports information on the specified 
 *      host machine.
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       success
 *              1       failure
 *              2       partial success
 *
 */

int
xfsGetExportInfo(const char* param, char** info, char** msg)
{
        CLIENT  *cl=NULL;
        char    *server = NULL;
        xfs_info_res    *result=NULL;
        int     returnVal = 0;
        char    *token=NULL;
        char    str[BUFSIZ];
        char    *dup_param=NULL;

#ifdef DEBUG 
	printf("DEBUG: xfsGetExportInfo() Invoked\n");
#endif

        /* Check for valid parameters */
        if ((param == NULL) || (info == NULL) || (msg == NULL))
        {
		/* TODO Internationalization */
                sprintf(str,"Invalid parameters supplied to xfsGetExportInfo()\n");
                add_to_buffer(msg,str);
                returnVal = 1;
        }
        else
        {
		*info = NULL;
		*msg = NULL;

                /* Make a copy of parameters string as strtok() messes it up */
                dup_param = strdup(param);
                assert(dup_param!=NULL);

                /* Retreive the host to get the information from */
                token = strtok((char*)dup_param,":");
                while (token != NULL)
                {
                        /* Look for hostname keyword */
                        if (strcmp(token,XFS_HOSTNAME_STR)==0)
                        {
                                /* Get the hostname */
                                server = strtok(NULL,"\n");
                        }
                        else
                        {
                                /* Skip the value of the keyword */
                                strtok(NULL,"\n");
                        }

                        /* Get the next keyword */
                        token = strtok(NULL,":");
                }

		if (xfsRpcCallInit(server, &cl, msg) == B_FALSE) {
		  returnVal = 1;
		}
		/* Execute the RPC call */
		else if ((result = xfsgetexportinfo_1(&param,cl))==NULL)
		{
			sprintf(str,gettxt(":80",
				       "Error in executing RPC call:%s\n"),
					clnt_sperror(cl,server));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		/* Return the results */
		else
		{
			if (result->info != NULL) {
				*info = (char*)strdup(result->info);
				assert(*info!=NULL);
			}
			if (result->warnmsg != NULL)
			{
				*msg = (char*)strdup(result->warnmsg);
				assert(*msg!=NULL);
			}

			returnVal = result->errno;
			if (returnVal != 0 && result->warnmsg == NULL) {
				/* TODO Internationalization */
				sprintf(str,
				"Error in xfsGetExportInfoInternal()\n");
				add_to_buffer(msg, str);
			}
			xdr_free(xdr_xfs_info_res,result);
		}

                if (dup_param != NULL) {
                        free(dup_param);
		}
		if(cl != NULL) {
			clnt_destroy(cl);
		}
        }
        return(returnVal);
}
