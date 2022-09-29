/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.4 $"


#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <pfmt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <rpc/rpc.h>
#include "xfs_utils.h"
#include "xfs_rpc_defs.h"
#include "xfs_info_defs.h"

boolean_t
xfsParseObjectSignature(const char *expect_class,
			const char *o_sig,
			char *o_host,
			char *o_name,
			char *o_class,
			char *o_type,
			char **errbuf)
{
	boolean_t	bad_format = B_FALSE;
	char		str[BUFSIZ];
	char		*l_sig,
			*l_host,
			*l_name,
			*l_class,
			*l_type;

	if((l_sig = strdup(o_sig)) == NULL) {
		return B_FALSE;
	}

	if( ((l_host  = strtok(l_sig, "{ ")) == NULL) ||
	    ((l_class = strtok(NULL,  " " )) == NULL) ||
	    ((l_name  = strtok(NULL,  " " )) == NULL) ||
	    ((l_type  = strtok(NULL,  " }")) == NULL)) {
		/*
		 *	TODO:	Put a real error message in errbuf
		 */
	        sprintf(str, "Bad object signature: -%s-\n", o_sig);
		add_to_buffer(errbuf, str);
		bad_format = B_TRUE;
	} else {
		if(o_host  != NULL) { strcpy(o_host,  l_host); }
		if(o_name  != NULL) { strcpy(o_name,  l_name); }
		if(o_type  != NULL) { strcpy(o_type,  l_type); }
		if(o_class != NULL) {
		    strcpy(o_class, l_class);
		    if(expect_class != NULL && 
		       strcmp(o_class, expect_class) != 0) {
			    sprintf(str, gettxt(":98",
			 	"Bad object type.  Expecting %s, got %s.\n"));
		    }
		}
	}

	free(l_sig);

	return( !bad_format );
}

boolean_t
xfsRpcCallInit(char *server, CLIENT **client, char **errbuf)
{
        struct timeval	tv;
	char		str[BUFSIZ];
	int xfs_authunix_create(AUTH** authptr);

	if(server == NULL || client == NULL || errbuf == NULL) {
		return B_FALSE;
	}

        /* Set the RPC client timeout value */
        tv.tv_sec = XFS_RPC_TIMEOUT_VAL;
        tv.tv_usec = 0;

	if((*client = clnt_create(server, XFSPROG, XFSVERS, "tcp")) == NULL) {
		sprintf(str, gettxt(":79","Error creating client:\n%s"),
					clnt_spcreateerror(server));
		add_to_buffer(errbuf, str);
		return B_FALSE;
	}
	else if ( xfs_authunix_create( &(*client)->cl_auth ) ) {
		sprintf(str, gettxt(":119",
		"Error in setting client authentication information."));
		add_to_buffer(errbuf, str);
		return B_FALSE;
	}
	else if(clnt_control(*client, CLSET_TIMEOUT, &tv) == 0) {
		sprintf(str, gettxt(":104",
			"Error in setting timeout value for RPC client."));
		add_to_buffer(errbuf, str);
		return B_FALSE;
	}

	return B_TRUE;
}

boolean_t
xfsConvertNameToDevice(char *from, char *to, boolean_t raw)
{
	struct stat	stat_buf;
	char		wbuf[PATH_MAX + 1];
	char		*ptr;
	mode_t		bmode;

	if(from == NULL || to == NULL || *from == '\0') {
		return(B_FALSE);
	}

	/*
	 *	If the name contains a ':' then we assume that it
	 *	is an NFS name and do not want to alter it.
	 */
	if(strchr(from, ':') != NULL) {
		strcpy(to, from);
		return B_TRUE;
	}

	/*
	 *	Check to see if the what was passed in already the
	 *	complete device name.
	 */
	if(stat(from, &stat_buf) == 0) {
		/*
		 *	Check to make sure that the device type is
		 *	that which we are looking for.
		 */
		bmode = stat_buf.st_mode & S_IFMT;
		if((bmode == S_IFBLK && raw == B_FALSE) ||
		   (bmode == S_IFCHR && raw == B_TRUE)) {
			strcpy(to, from);
			return B_TRUE;
		}
	}

	/*
	 *	Strip off anything up to and including the last '/'
	 */
	if((ptr = strrchr(from, '/')) != NULL) {
		ptr++;
	} else {
		ptr = from;
	}

	/*
	 *	Look first in the standard device directory and then
	 *	in the xlv device directory
	 */
	if(raw == B_FALSE) {
		sprintf(to, "/dev/dsk/%s", ptr);
	} else {
		sprintf(to, "/dev/rdsk/%s", ptr);
	}

	if(stat(to, &stat_buf) == 0) {
		return B_TRUE;
	}
	else {
		sprintf(to, "/dev/xlv/%s", ptr);
		if(stat(to, &stat_buf) == 0) {
			return B_TRUE;
		}
	}

	to[0] = '\0';
	return(B_FALSE);
}


/*
 *	xfs_authunix_create(authptr)
 *	
 *	This routine creates and returns an RPC authentication handle
 *
 */

int
xfs_authunix_create(AUTH** authptr)
{
	char hostname[MAXHOSTNAMELEN+1];
	char parsed_hostname[MAXHOSTNAMELEN+1];
	char domainname[MAXHOSTNAMELEN+1];
	gid_t gidset[NGROUPS];
	int ngrps;

	/* get the hostname */
	if (gethostname(hostname, MAXHOSTNAMELEN+1) != 0)
		return(1) ;

	strcpy(parsed_hostname, strtok (hostname, "."));

	/* append the domainname */
	if (getdomainname(domainname,  MAXHOSTNAMELEN+1) != 0)
		return(1) ;

	strcat(parsed_hostname, ".");
	strcat(parsed_hostname, domainname);	

	ngrps = getgroups(NGROUPS, gidset);
	if (ngrps == -1)
		return(1);


	/* create rpc authentication structure */
	*authptr = (AUTH*) authunix_create ( parsed_hostname, getuid(), getgid(), ngrps, gidset);

	if (authptr == NULL)
		return (1);
	else
		return(0);
}


boolean_t
xfsmGetKeyValue(char *s, char **key, char **value)
{
	char		*p;
	boolean_t	rval = B_TRUE;

	if((p = strchr(s, ':')) != NULL) {
		*p = NULL;
		*key = s;
		*value = ++p;
	} else {
		*key = *value = NULL;
		rval = B_FALSE;
	}

	return rval;
}

