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
 *	The interface function for the RPC call to perform xlv commands
 *	such as creating/modifying/deleting an xlv object.
 */

#ident "xlv_cmd.c: $Revision: 1.9 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>
#include <pfmt.h>
#include "xfs_info_defs.h"
#include "xlv_cmd_key.h"
#include <sys/types.h>
#include <sys/uuid.h>
#include <errno.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <sys/xlv_attr.h>
#include <sys/syssgi.h>
#include <locale.h>
#include <tcl.h>


/*
 *	The following is for xlv_admin functions
 */
static char	*xlvMgrCmd = "/sbin/xlv_mgr";
static char	*xlvMakeCmd = "/usr/sbin/xlv_make";
static char	*xlvRmObjNm = "___del_me___";
static int	parse_data(const char *buf, char *obj_name, char *name, 
			   int *op_type, char *svol_str, int *plex, 
			   int *ve, int *svol_flag);

extern int	runXlvCommand(char *, char *, char *, char **, boolean_t);
extern void	init_admin(void);

/*
 *      xlvDeleteInternal(parameters, msg)
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 */
int
xlvDeleteInternal(const char* parameters, char **msg)
{
	int	returnVal = 0;
	char	*dup_parameters = NULL;
	char	*line = NULL;
	char	*obj_name = NULL;
	char	*key = NULL;
	char	*value = NULL;	
	char	str[BUFSIZ];
	char	argbuf[512];

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		return(1);
	}

	*msg = NULL;

	/* Make a copy of the parameters */
	dup_parameters = strdup(parameters);
	ASSERT(dup_parameters!=NULL);

	/* Parse the parameters string */
	for(line = strtok((char *)parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value)) {
			obj_name = value;
		}
	}

	if (obj_name != NULL) {
		sprintf(argbuf, "delete object %s", obj_name);
		returnVal = runXlvCommand("xlvDelete", xlvMgrCmd,
					argbuf, msg, B_FALSE);
	}
	else {
		sprintf(str, gettxt(":16",
			"Object to be deleted was not specified.\n"));
		add_to_buffer(msg, str);
		returnVal = 1;
	}

	if (dup_parameters != NULL) {
		free(dup_parameters);
	}

	return(returnVal);
}


/*
 *      xlvAttachInternal(parameters, msg)
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 */
int
xlvAttachInternal(const char* parameters, char **msg)
{
	int	returnVal = 0;
	char	str[BUFSIZ];
	char	aname[64];
	char 	obj_name[64];
	int	svol_flag = 0;
	char	svol_str[16];
	char	argbuf[512];
	int 	op_type;
	int	plex = -1;
	int	ve = -1;

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		sprintf(str,
			"Invalid parameters passed to xlvAttachInternal()\n");
		add_to_buffer(msg, str);
		return(1);
	}

	*msg = NULL;
	obj_name[0] = '\0';
	aname[0] = '\0';
	svol_str[0] = '\0';
	returnVal = parse_data(parameters, obj_name, aname,
				&op_type, svol_str, &plex,&ve, &svol_flag);

	if (obj_name[0] == '\0') {
		sprintf(str, gettxt(":16", "Volume name not specified.\n"));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	if (aname[0] == '\0') {
		sprintf(str, gettxt(":16", "Object name not specified.\n"));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	if (returnVal != 0) {
#ifdef	DEBUG
	fprintf(stderr, "DBG: xlvAttach input data format error\n");
#endif
		return(1);
	}

	if(op_type == XLV_OBJ_TYPE_PLEX && svol_flag == 1) {
		/*
		 *	Attach a plex to a volume
		 */
		sprintf(argbuf, "attach plex %s %s.%s ", 
				aname, obj_name, svol_str);
	}
	else if(op_type == XLV_OBJ_TYPE_VOL_ELMNT &&
		svol_flag == 1 && plex != -1 && ve != -1) {
		/*
		 *	Attach a ve to a volume
		 */
		sprintf(argbuf, "insert ve %s %s.%s.%d.%d", 
				aname, obj_name, svol_str, plex, ve);
	}
	else if(op_type == XLV_OBJ_TYPE_VOL_ELMNT &&
		svol_flag == 1 && plex != -1 && ve == -1) {
		/*
		 *	Insert a ve to a volume
		 */
		sprintf(argbuf, "attach ve %s %s.%s.%d", 
				aname, obj_name, svol_str, plex);
	}
	else if(op_type == XLV_OBJ_TYPE_VOL_ELMNT &&
		svol_flag == 0 && ve != -1) {
		/*
		 *	Attach a ve to a plex
		 */
		sprintf(argbuf, "insert ve %s %s.%d", aname, obj_name, ve);
	}
	else if(op_type == XLV_OBJ_TYPE_VOL_ELMNT &&
		svol_flag == 0 && ve == -1) {
		/*
		 *	Attach a ve to a plex
		 */
		sprintf(argbuf, "attach ve %s %s", aname, obj_name);
	}
	else {
                sprintf(str,
			"Invalid parameters passed to xlvAttachInternal()\n");
		add_to_buffer(msg, str);
		return(1);
	}

	returnVal = runXlvCommand("xlvAttach", xlvMgrCmd, argbuf, msg, B_FALSE);

#ifdef	DEBUG
	fprintf(stderr, "DBG: xlvAttach rval=%d '%s'\n", returnVal, argbuf);
#endif

	if(returnVal == 0) {
		init_admin();
	}
	return(returnVal);
}


/*
 *      xlvDetachInternal(parameters, msg)
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 */
int
xlvDetachInternal(const char* parameters, char **msg)
{
	int	returnVal = 0;
	char	str[BUFSIZ];
	char 	obj_name[64];
	char	aname[64];
	char	argbuf[512];
	int	svol_flag = 0;
	char	svol_str[16];
	int 	op_type;
	int	plex = -1;
	int	ve = -1;

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		sprintf(str,
			"Invalid parameters passed to xlvDetachInternal()\n");
		add_to_buffer(msg, str);
		return(1);
	}

	*msg = NULL;
	obj_name[0] = '\0';
	aname[0] = '\0';
	svol_str[0] = '\0';
	returnVal = parse_data(parameters, obj_name, aname,
				&op_type, svol_str, &plex,&ve, &svol_flag);

	if (obj_name[0] == '\0') {
		sprintf(str, gettxt(":16", "Volume name not specified.\n"));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	if (aname[0] == '\0') {
		sprintf(str, gettxt(":16", "Object name not specified.\n"));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	if (returnVal != 0) {
#ifdef	DEBUG
	fprintf(stderr, "DBG: xlvDetach input data format error\n");
#endif
		return(1);
	}

	if(op_type == XLV_OBJ_TYPE_PLEX && svol_flag == 1 && plex != -1) {
		/*
		 *	Detach a plex from a volume
		 */
		sprintf(argbuf, "detach plex %s.%s.%d %s", 
				obj_name, svol_str, plex, aname);
	}
	else if(op_type == XLV_OBJ_TYPE_VOL_ELMNT &&
		svol_flag == 1 && plex != -1 && ve != -1) {
		/*
		 *	Detach a ve from a volume
		 */
		sprintf(argbuf, "detach ve %s.%s.%d.%d %s", 
				obj_name, svol_str, plex, ve, aname);
		}
	else if(op_type == XLV_OBJ_TYPE_VOL_ELMNT &&
		svol_flag == 0 && ve != -1) {
		/*
		 *	Detach a ve from a plex
		 */
		sprintf(argbuf, "detach ve %s.%d %s", 
				obj_name, ve, aname);
	}
	else {
                sprintf(str,
			"Invalid parameters passed to xlvDetachInternal()\n");
		add_to_buffer(msg, str);
		return(1);
	}

	returnVal = runXlvCommand("xlvDetach", xlvMgrCmd, argbuf, msg, B_FALSE);

#ifdef	DEBUG
	fprintf(stderr, "DBG: xlvDetach rval=%d '%s'\n", returnVal, argbuf);
#endif

	if(returnVal == 0) {
		init_admin();
	}
	return(returnVal);
}


/*
 *      xlvRemoveInternal(parameters, msg)
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 */
int
xlvRemoveInternal(const char* parameters, char **msg)
{
	int	returnVal = 0;
	char	str[BUFSIZ];
	char 	obj_name[64];
	char	aname[64];
	char	argbuf[512];
	int	svol_flag = 0;
	char	svol_str[16];
	int 	op_type;
	int	plex = -1;
	int	ve = -1;
	boolean_t	re_init = B_FALSE;

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		sprintf(str,
			"Invalid parameters passed to xlvRemoveInternal()\n");
		add_to_buffer(msg, str);
		return(1);
	}

	*msg = NULL;
	obj_name[0] = '\0';
	aname[0] = '\0';
	svol_str[0] = '\0';
	returnVal = parse_data(parameters, obj_name, aname,
				&op_type, svol_str, &plex,&ve, &svol_flag);

	if (obj_name[0] == '\0') {
		sprintf(str, gettxt(":16", "Volume name not specified.\n"));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	if (returnVal != 0) {
#ifdef	DEBUG
	fprintf(stderr, "DBG: xlvRemove input data format error\n");
#endif
		return(1);
	}

	strcpy(aname, xlvRmObjNm);

	if(op_type == XLV_OBJ_TYPE_PLEX && svol_flag == 1 && plex != -1) {
		/*
		 *	Remove a plex from a volume
		 */
		sprintf(argbuf, "detach plex %s.%s.%d %s", 
				obj_name, svol_str, plex, aname);
	}
	else if(op_type == XLV_OBJ_TYPE_VOL_ELMNT &&
		svol_flag == 1 && plex != -1 && ve != -1) {
		/*
		 *	Remove a ve from a volume
		 */
		sprintf(argbuf, "detach ve %s.%s.%d.%d %s", 
				obj_name, svol_str, plex, ve, aname);
	}
	else if(op_type == XLV_OBJ_TYPE_VOL_ELMNT &&
		svol_flag == 0 && ve != -1) {
		/*
		 *	Remove a ve from a plex
		 */
		sprintf(argbuf, "detach ve %s.%d %s", 
				obj_name, ve, aname);
	}
	else {
                sprintf(str,
			"Invalid parameters passed to xlvRemoveInternal()\n");
		add_to_buffer(msg, str);
		return(1);
	}	/* end of ve remove */

	returnVal = runXlvCommand("xlvRemove", xlvMgrCmd, argbuf, msg, B_FALSE);
#ifdef	DEBUG
	fprintf(stderr, "DBG: xlvRemove rval=%d '%s'\n", returnVal, argbuf);
#endif
	if (returnVal == 0) {
		re_init = B_TRUE;
		sprintf(argbuf, "delete object %s", xlvRmObjNm);
		returnVal = runXlvCommand("xlvRemove", xlvMgrCmd,
				argbuf, msg, B_FALSE);
#ifdef	DEBUG
	fprintf(stderr, "DBG: xlvRemove rval=%d '%s'\n", returnVal, argbuf);
#endif
	}

	if(returnVal == 0 || re_init == B_TRUE) {
		init_admin();
	}

	return(returnVal);
}

/*
 *      xlvCreateInternal(parameters, msg)
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 */
int
xlvCreateInternal(const char* parameters, char **msg)
{
	int	rval;
	char	*argbuf;

	argbuf = strdup(parameters);

	rval = runXlvCommand("xlvCreate", xlvMakeCmd, argbuf, msg, B_FALSE);

	if(rval == 0) {
		init_admin();
	}

	free(argbuf);
	return(rval);
}


/*
 *      xlvInfoInternal(parameters, msg)
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 */
int
xlvInfoInternal(const char* parameters, char **msg)
{
	int	returnVal = 0;
	char	*dup_parameters = NULL;
	char	*line = NULL;
	char	*key = NULL;
	char	*value = NULL;	
	char	argbuf[512];
	char 	obj_name[64];

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		return(1);
	}

	*msg = NULL;

	/* Make a copy of the parameters */
	dup_parameters = strdup(parameters);
	ASSERT(dup_parameters!=NULL);

	/* Parse the parameters string */
	for(line = strtok((char *)parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value)) {
			strcpy(obj_name, value);
		}
	}

	sprintf(argbuf, "script object %s", obj_name);
	returnVal = runXlvCommand("xlvInfo", xlvMgrCmd, argbuf, msg, B_TRUE);

	if (dup_parameters != NULL) {
		free(dup_parameters);
	}

	return(returnVal);
}

/*
 *      xlvSynopsisInternal(parameters, msg)
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 */
int
xlvSynopsisInternal(const char* parameters, char **msg)
{
	int	returnVal = 0;
	char	*dup_parameters = NULL;
	char	*line = NULL;
	char	*key = NULL;
	char	*value = NULL;	
	char	str[BUFSIZ];
	char 	obj_name[64];

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		return(1);
	}

	*msg = NULL;

	/* Make a copy of the parameters */
	dup_parameters = strdup(parameters);
	ASSERT(dup_parameters!=NULL);

	/* Parse the parameters string */
	for(line = strtok((char *)parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value)) {
			strcpy(obj_name,value);
		}
	}

	returnVal = formatData (obj_name, msg);

	if (dup_parameters != NULL) {
		free(dup_parameters);
	}

	return(returnVal);
}


/*
 *      getPartsUseInternal(parameters, msg)
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 */
int
getPartsUseInternal(const char* parameters, char **msg)
{
	int	returnVal = 0;

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		return(1);
	}

	*msg = NULL;
	returnVal = xlvPartsUse (msg);

	return(returnVal);
}

static int
parse_data(const char *buf, 
	   char *obj_name, 
	   char *name, 
	   int *op_type, 
	   char *svol_str, 
	   int *plex, 
	   int *ve,
	   int *svol_flag)
{
	int	returnVal = 0;
        char    *dup_parameters;
        char    *key;
        char    *value;
        char    *line;
	char	ch;

	*op_type = -1;

        /* Parse the parameters string */
        dup_parameters = strdup(buf);

	for(line = strtok(dup_parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

                /* get the keyword and the value */
		if(xfsmGetKeyValue(line, &key, &value) == B_FALSE) {
			continue;
		}
		ch = *key;

                if(ch == 'S' && (strcmp(key, "SVOL") == 0)) {
		        if (strcmp(value, "data") == 0) {
               		 	strcpy(svol_str, "data");
		        } else if (strcmp(value, "log")== 0) {
				strcpy(svol_str, "log");
		        } else if (strcmp(value, "rt")== 0) {
				strcpy(svol_str, "rt");
		        } else {
		                pfmt(stderr, MM_NOSTD, ":32:Invalid choice \n");
				returnVal = 1;
		        }
			*svol_flag = 1;
                }
                else if(ch == 'P' && (strcmp(key, "PLEX") == 0)) {
			*plex = atoi(value);
                }
                else if(ch == 'V' && (strcmp(key, "VE") == 0)) {
			*ve = atoi(value);
                }
                else if(ch == 'N' && (strcmp(key, "NAME") == 0)) {
                        strcpy(name, value);
                }
                else if(ch == 'O' && (strcmp(key, "OBJ_NAME") == 0)) {
                        strcpy(obj_name, value);
		}
		else if((ch == 'A' && (strcmp(key, "ATTACH_TYPE") == 0)) ||
			(ch == 'D' && (strcmp(key, "DETACH_TYPE") == 0))) {
			if (strcmp(value, "PLEX") == 0) {
				*op_type = XLV_OBJ_TYPE_PLEX;
			}
			else if (strcmp(value, "VE") == 0) {
				*op_type = XLV_OBJ_TYPE_VOL_ELMNT;
			}
		}
        }

#ifdef	DEBUG
fprintf(stderr, "TAC parse_data() obj_name=%s, name=%s, plex=%d, ve=%d, svol=%s\n",
			obj_name, name, *plex, *ve, svol_str);
#endif

	if (dup_parameters != NULL) {
		free(dup_parameters);
	}

	return(returnVal);
}
