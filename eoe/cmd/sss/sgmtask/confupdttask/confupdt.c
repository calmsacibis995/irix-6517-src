/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                */
/* All Rights Reserved.                                                      */
/*                                                                           */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics Inc.;     */
/* contents of this file may not be disclosed to third parties, copied or    */
/* duplicated in any form, in whole or in part, without the prior written    */
/* permission of Silicon Graphics, Inc.                                      */
/*                                                                           */
/* RESTRICTED RIGHTS LEGEND:                                                 */
/* Use,duplication or disclosure by the Government is subject to restrictions*/
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data    */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or  */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -   */
/* rights reserved under the Copyright Laws of the United States.            */
/*                                                                           */
/*---------------------------------------------------------------------------*/

#ident           "$Revision: 1.7 $"

/*---------------------------------------------------------------------------*/
/* getdata.c                                                                 */
/*   This file contains an internal function of Client, SGMIRPCGetData       */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <libgen.h>
#include <syslog.h>
#include <ssdbapi.h>
#if 0
#include <klib/klib.h>
#include <sss/configmon_api.h>
#endif
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include "sgmauth.h"
#include "subscribe.h"

#define    OPT_STR           "vhcs:t:o:I:"
#define    VERSIONNUM        "1.0"
#define    TABLEDELIMITER               '#'
#define    HEADERDELIMITER              '!'
#define    RECORDDELIMITER              '@'
#define    SYSTEMIDDELIMITER            'S'
#define    CONFIGTABLE                  "configmon"
#define    CONFIGTABLE_TABLENAMES       "table_name"
#define    STRTOULL(s)                  (strtoull(s, (char **)NULL, 16))

char usage[] = "[-h] [-v] [-s <MaxTrasnferSize>] [-t <timekey>] <hostname>";
char       filename[256];
char *cmd;

static __uint32_t updateDB(char *file, char *hostname);

static int writetoFile(FILE *fp, char *data, int bytestowrite)
{
    int     num_retries = 0;
    int     byteswritten = 0;
    int     tmp = 0, tmp2 = 0;

    if ( !data || !bytestowrite ) return(0);

    while ( num_retries < 10 ) {
	tmp = fwrite(data, 1, bytestowrite, fp);

	if ( tmp > 0 ) byteswritten += tmp;

	if ( byteswritten < bytestowrite ) {
	    data += byteswritten;
	    bytestowrite -= byteswritten;
	} else break;

	num_retries++;
    }

    return(byteswritten);

}

static __uint32_t SGMIGetConfigData(char *host, int xfrsz, int key)
{
    __uint32_t    err = 0;
    sgmtask_t     task;
    char          authStr[AUTHSIZE];
    int           authlen = 0, authType = 0;
    char          mydata[128];
    int           nobytes = 0;
    char          *retPtr = NULL;
    char          bytessent[10], bytesremaining[10], offset[10];
    int           ibytessent = 0, ibytesremaining = 0;
    char          serverfilename[256];
    char          proc[10];
    struct stat   statb;
    FILE          *fp = NULL;

    memset(mydata, 0, sizeof(mydata));
    memset(serverfilename, 0, sizeof(serverfilename));
    if ( !host || !xfrsz ) {
	return(ERR(CLIENTERR, PARAMERR, INVALIDARGS));
    }

    if ( (authlen = SGMAUTHIGetAuth(host, authStr, &authType, 0 )) == 0) {
	return(ERR(CLIENTERR, AUTHERR, CANTGETAUTHINFO));
    }

    nobytes = snprintf(mydata, sizeof(mydata), 
	      "XTRACT %d %d", xfrsz, key);

    task.datalen = pack_data((char **)&task.dataptr, authStr, authlen, mydata,
			     nobytes+1, NULL, 0, 0);

    if (!task.datalen ) return(ERR(CLIENTERR, AUTHERR, CANTPACKDATA));

    task.rethandle = NULL;

    err = SGMIrpcclient(host, CONFIGUPDTPROC, &task, 0);

    SGMIfree("configupdtproc", task.dataptr);

    if ( err ) return(err);
    else {
	if ( !unpack_data(authStr, authlen, (void **) &retPtr, 0, NULL, 
			  task.rethandle->dataptr, task.rethandle->datalen, 0)) {
	    if ( task.rethandle) SGMIfree("xxx", task.rethandle);
	    return(ERR(CLIENTERR, AUTHERR, CANTUNPACKDATA));
        }
    }

    if ( stat("/tmp", &statb) == 0 ) {
	snprintf(filename, sizeof(filename), "/tmp/SgMcXXXXXX");
    } else if ( stat("/var/tmp", &statb) == 0 ) {
	snprintf(filename, sizeof(filename), "/var/tmp/SgMcXXXXXX");
    } else {
	err = ERR(CLIENTERR, OSERR, errno);
	goto end;
    }

    mktemp(filename);
    nobytes = 0;

    /* Data received is always in the following format :
       PROC BYTESSENT BYTESREMAINING OFFSET SERVERFILENAME DATA...
    */

    nobytes = sscanf(retPtr, "%s %d %d %s %s DATA:",
		   proc, &ibytessent, &ibytesremaining, offset, serverfilename);

    if ( !nobytes ) {
	err = ERR(SERVERERR, PARAMERR, INCOMPLETEDATA);
    } else if ( ibytessent == 0 ) {
	err = SSSERR(SERVERERR, SSDBERR, NORECORDSFOUND);
    } else {
        nobytes = strstr(retPtr, "DATA:") - retPtr + 6;
    }

    if ( err ) goto end;

    if ( (fp = fopen(filename, "w")) == NULL ) {
	return(ERR(CLIENTERR, OSERR, errno));
    }

    while ( 1 ) {
	nobytes = writetoFile(fp, retPtr+nobytes, ibytessent);
	if ( nobytes != ibytessent ) {
	    err = ERR(CLIENTERR, OSERR, 0);
	    goto end;
	}
	    
	if ( ibytesremaining ) {
            if ( task.rethandle->dataptr ) SGMIfree("xxx", task.rethandle->dataptr);
	    if ( task.rethandle ) SGMIfree("xxx", task.rethandle);
	    ibytessent = ((ibytesremaining >= xfrsz) ? xfrsz : ibytesremaining);
            nobytes = snprintf(mydata, sizeof(mydata),
			       "XFR %d %s %s",
			       ibytessent, offset, serverfilename);
	    task.datalen = pack_data((char **)&task.dataptr, authStr, authlen, mydata,
				 nobytes+1, NULL, 0, 0);

            if ( !task.datalen ) {
	        err = ERR(CLIENTERR, AUTHERR, CANTPACKDATA);
	        goto end;
	    }

	    task.rethandle = NULL;
	    err = SGMIrpcclient(host, CONFIGUPDTPROC, &task, authType);
	    SGMIfree("configupdtproc", task.dataptr);

	    if ( err ) goto end;
	    else {
	        if ( !unpack_data(authStr, authlen, (void **) &retPtr, 0, NULL,
		         task.rethandle->dataptr, task.rethandle->datalen, 0)) {
		     err = ERR(CLIENTERR, AUTHERR, CANTUNPACKDATA);
		     goto end;
                }
	    }

	    nobytes = sscanf(retPtr, "%s %d %d %s %s DATA:",
			       proc, &ibytessent, &ibytesremaining, offset, 
                               serverfilename);
            

	    if ( !nobytes || !ibytessent ) {
		err = ERR(SERVERERR, PARAMERR, INCOMPLETEDATA);
		goto end;
            } else {
                nobytes = strstr(retPtr, "DATA:") - retPtr + 6;
            }
	} else {
	    break;
	}
    }

    fclose(fp);

    /* Do clean-up here */

    if ( task.rethandle->dataptr) SGMIfree ("xxx", task.rethandle->dataptr);
    if ( task.rethandle ) SGMIfree("Xxx", task.rethandle);

    nobytes = snprintf(mydata, sizeof(mydata), "CLEAN %s", serverfilename);
    task.datalen = pack_data((char **)&task.dataptr, authStr, authlen, mydata,
			     nobytes+1, NULL, 0, 0);
    task.rethandle = NULL;
    err = SGMIrpcclient(host, CONFIGUPDTPROC, &task, authType);
    SGMIfree("configupdtproc", task.dataptr);

    end:
        if ( err ) unlink(filename);
        else { 
            err = updateDB(filename, host);
            if ( err ) err = SSSERR(CLIENTERR, SSDBERR, err);
	    unlink(filename);
        }
	return(err);
}

static int SGMICheckSystem(char *hostname, char *sysid)
{
    int   nrows = 0, ncols = 0;
    char  sqlstmt[1024];
    int   err = 0;
    char  *myvalue = NULL;
    int   retvalue = 2;

    if ( !hostname || !sysid ) return (0);

    snprintf(sqlstmt, sizeof(sqlstmt), 
	     "select local from system where sys_id = '%s' and active = 1", 
	     sysid);

    err = SGMSSDBGetTableData(sqlstmt, &myvalue, &nrows, &ncols, "@", "|");
    if ( err ) return(0);
    else if ( !nrows || !myvalue ) return(0);
    else if ( nrows > 1 ) return(0);
    else {
	if ( atoi(strtok(myvalue, "@|")) == 1 ) 
	    retvalue = 1;
    }
    free(myvalue);
    return(retvalue);
}

#if 0
static int SGMICheckHostNameChange(time_t chngtime, char *oldhostname, char *sys_id)
{
    char  newhostname[512];
    cm_hndl_t    cm_history, cm_event, change_item, icmp, item, config;
    time_t       change_time;
    int          type = 0, item_type = 0;
    k_uint_t     sysid = STRTOULL(sys_id);
    char         *mystring = NULL;

    if ( !sysid || !oldhostname ) return(0);
    memset(newhostname, 0, sizeof(newhostname));

    cm_init();
    if ( !(config = cm_alloc_config("ssdb", 1))) {
	return(0);
    }

    if ( !(cm_history = cm_event_history(config,sysid,0,0,SYSINFO_CHANGE))) {
	cm_free_config(config);
	return(0);
    }

    if ( !(cm_event = cm_item(cm_history, CM_LAST))) {
	cm_free_list(config, cm_history);
	cm_free_config(config);
	return(0);
    }

    change_time = CM_UNSIGNED(cm_field(cm_event,CM_EVENT_TYPE,CE_TIME));
    icmp = cm_change_items(config,change_time);

    if ( !icmp ) {
	cm_free_list(config, cm_history);
	cm_free_list(config, cm_event);
	cm_free_config(config);
	return(0);
    }

    if ( !(change_item = cm_item(icmp, CM_FIRST))) {
	cm_free_list(config, icmp);
	cm_free_list(config, cm_event);
	cm_free_list(config, cm_history);
	cm_free_config(config);
	return(0);
    }

    do {
	type = CM_SIGNED(cm_field(change_item,CHANGE_ITEM_TYPE,CI_TYPE));
	if ( type == SYSINFO_OLD || type == SYSINFO_CURRENT ) {
	    item = cm_get_change_item(config, change_item);
	    item_type = cm_change_item_type(change_item);
	    if ( item_type == SYSINFO_TYPE || !item_type ) {
		if ( type == SYSINFO_CURRENT ) {
                    mystring = CM_STRING(cm_field(item,SYSINFO_TYPE,SYS_HOSTNAME));
		    if ( mystring ) strcpy(newhostname, mystring);
		} else {
                    mystring = CM_STRING(cm_field(item,SYSINFO_TYPE,SYS_HOSTNAME));
		    if ( mystring ) strcpy(oldhostname, mystring);
		}
	    }
	}
	cm_free_item(config, item, item_type);
    }  while (change_item = cm_item(icmp,CM_NEXT));
    cm_free_list(config, icmp);
    /*cm_free_list(config, cm_event);*/
    cm_free_list(config, cm_history);
    cm_free_config(config);

    if ( strcmp(newhostname, oldhostname) ) return(1);
    return(0);
}
#endif

static void SGMISendSystemChange(char *hnames, int proc, sysinfo_t *oldid)
{
    char          *p = NULL;
    sgmtask_t     task;

    if ( !hnames || !proc || !oldid ) return;

    doencr(oldid, STANDARD_KEY, STANDARD_MASK, sizeof(sysinfo_t));
    task.dataptr = oldid;
    task.datalen = sizeof(sysinfo_t);
    task.rethandle = NULL;

    p = strtok(hnames, "@|");
    while ( p ) {
	SGMIrpcclient(p, proc, &task, STANDARD_AUTH|FULLENCR);
	p = strtok(NULL, "@|");
    }
    doencr(oldid, STANDARD_KEY, STANDARD_MASK, sizeof(sysinfo_t));
    return;
}

static __uint32_t SGMINotifySystemChange(time_t time, char *hostname, char *sysid)
{
    char          oldhname[512];
    char          *hnames = NULL;
    char          *p = NULL;
    int           nrows = 0, ncols = 0;
    char          sqlstr[1024];
    __uint32_t    err = 0;
    sysinfo_t     mysys;


    if ( !sysid || !hostname ) return(ERR(CLIENTERR, PARAMERR, INVALIDARGS));
    /*else if ( !SGMICheckHostNameChange(time, oldhname, sysid) ) return(0);*/

    if ( (err = SGMSSDBGetSystemDetails(&mysys, 1, 1)) != 0 ) {
        return(SSSERR(CLIENTERR, SSDBERR, err));
    }

    /* First lets check if we are an SGM.  This can be found if system table
       has entries with active = 1 and local = 0
    */

    snprintf(sqlstr, sizeof(sqlstr), 
	     "select hostname from system where active = 1 and local = 0");
    
    err = SGMSSDBGetTableData(sqlstr, &hnames, &nrows, &ncols, "@", "|");

    if ( err ) return(SSSERR(CLIENTERR, SSDBERR, err));
    else if ( nrows > 0 ) {
	/* Start contacting each of the subscribed hosts and inform them of
	   SGM Hostname Change */
	SGMISendSystemChange(hnames, SGMHNAMECHANGEPROC, &mysys);
	SGMIfree("hnames", hnames);
    }

    /* Now check if somebody subscribed to us !  We also need to inform
       them of our hostname change
    */

    hnames = NULL;
    snprintf(sqlstr, sizeof(sqlstr),
	     "select forward_hostname from event_action where forward_hostname != ''");

    err = SGMSSDBGetTableData(sqlstr, &hnames, &nrows, &ncols, "@", "|");

    if ( err ) return(SSSERR(CLIENTERR, SSDBERR, err));
    else if ( nrows > 0 && hnames != NULL ) {
	/*SGMISendSystemChange(hnames, SEMHNAMECHANGEPROC, oldhname);*/
	SGMISendSystemChange(hnames, SEMHNAMECHANGEPROC, &mysys);
	SGMIfree("hnames", hnames);
    }

    return(0);
}

static __uint32_t updateDB(char *file, char *hostname)
{
    FILE   *fp;
    char   table_name[64];
    char   *k = NULL;
    char   *mysysid = 0;
    int    nrows = 0, ncols = 0;
    char   sqlstmt[1024];
    char   buffer[4096];
    char   sysid[20];
    int    err = 0;
    int    reqType;
    int    updtreqd = 0;
    int    nrecords = 0;
    int    headerflag = 0;

    if ( file == NULL || !hostname ) return(0);

    if ( (fp = fopen(file, "r")) == NULL ) {
	errorexit(cmd, 0, LOG_ERR, "Can't open %s for reading",
		  file);
        return(0);
    }

    while ( fgets(buffer, 4096, fp) != NULL ) {
	/* Remove the newline character from buffer */
	k = strrspn(buffer, "\n");
	*k = '\0';
	switch(buffer[0]) {
            case SYSTEMIDDELIMITER:
                k = strtok(&buffer[2], " ");
		if ( k ) {
		    sprintf(sqlstmt, "select hostname from system where sys_id = '%s'",
			    k);
		    err = SGMSSDBGetTableData(sqlstmt, &mysysid, &nrows, &ncols, "@", "|");
		    if ( err ) return SSSERR(CLIENTERR, SSDBERR, err);
		    else if ( !nrows || !mysysid ) 
			return(SSSERR(CLIENTERR, SSDBERR, NORECORDSFOUND));
		    else {
			strcpy(sysid, k);
			free(mysysid);
		    }
		}
		break;
	    case TABLEDELIMITER:
		headerflag = nrecords = nrows = ncols = updtreqd = 0;
		k = strtok(&buffer[2], " ");
		if ( k ) strcpy(table_name, k);
		else {
		    return(SSSERR(CLIENTERR, RPCERR, CLOBBEREDDATA));
		}

		/* Check table name */

		sprintf(sqlstmt, "select %s from %s where %s = '%s'", 
			CONFIGTABLE_TABLENAMES, CONFIGTABLE,
			CONFIGTABLE_TABLENAMES, table_name);

                err = SGMSSDBGetTableData(sqlstmt, &mysysid, &nrows, 
						  &ncols, "@", "|");

		if ( err ) return SSSERR(CLIENTERR, SSDBERR, err);
		else if ( !nrows || !mysysid ) {
		    return(SSSERR(CLIENTERR, SSDBERR, NORECORDSFOUND));
		} else {
		    
		}

		/* Get the header flag */

		k = strtok(NULL, " ");
		if ( k ) headerflag = atoi(k);

		k = strtok(NULL, " ");
		if ( k ) ncols = atoi(k);

		k = strtok(NULL, " ");
		if ( k ) nrows = atoi(k);


		if ( nrows ) {
		    sprintf(sqlstmt, "delete from %s where sys_id = '%s'",
			    table_name, sysid);
		    reqType = SSDB_REQTYPE_DELETE;
		    updtreqd = 1;
		} else {
		    updtreqd = 0;
		}
		break;
	    case HEADERDELIMITER:
                continue;
		break;
	    case RECORDDELIMITER:
		sprintf(sqlstmt, "insert into %s values (%s)", table_name, &buffer[2]);
		reqType = SSDB_REQTYPE_INSERT;
		break;
            default:
		break;
	}

	if ( !updtreqd ) continue;
	if ( (err = SGMSSDBSetTableData(sqlstmt, table_name, reqType)) != 0 ) {
	    goto end;
	}
    }

    end:
	if ( fp ) fclose(fp);
	return(err);
}

static int LockSession (char *filename)
{
    int            lfd = 0;

    if (!filename) return(-1);
    lfd = open(filename, O_WRONLY|O_CREAT, 0700);
    if ( lfd >= 0 ) 
    {
	if ( flock(lfd, LOCK_EX|LOCK_NB) ) {
	    return(-1);
	}
    } else {
        unlink(filename);
	return(-1);
    }

    return(lfd);
}

static void UnLockSession(int fd, char *filename)
{
    if ( !filename ) return;

    flock(fd, LOCK_UN|LOCK_NB);
    unlink(filename);
    return;
}

int main (int  argc,  char **argv)
{
    int        c = 0;
    int        objid = 0;
    time_t     timekey = 0;
    int        maxxfrsize = 16392;
    char       hostname[256];
    char       sysid[24];
    char       sessionfile[256];
    char       *rptr;
    __uint32_t err = 0;
    int        sesslock = 0;
    int        cflag = 0;

    memset(hostname, 0, sizeof(hostname));
    memset(sysid, 0, sizeof(sysid));
    cmd = argv[0];

    signal_setup();

    if ( argc < 2 ) {
	errorexit(cmd, 1, 7, "Usage: %s  %s", cmd, usage);
    }

    while ( (c = getopt(argc, argv, OPT_STR)) != -1 ) {
	switch(c) {
	    case 'v':
	        printf("%s V%s\n", cmd, VERSIONNUM);
	        exit(0);
	    case 'h':
		errorexit(cmd, 1, 7, "Usage: %s  %s", cmd, usage);
	    case 's':
		maxxfrsize = atoi(optarg);
		break;
            case 'o':
                objid = atoi(optarg);
                break;
	    case 'I':
		strcpy(sysid, optarg);
		break;
	    case 'c':
		cflag = 1;
		break;
	    case 't':
		timekey = atoi(optarg);
		break;
	    case '?':
		errorexit(cmd, 1, 7, "Usage: %s  %s", cmd, usage);
	}
    }

    if ( optind < argc ) {
	strcpy(hostname, argv[optind]);
    }

    if ( maxxfrsize <= 0 ) maxxfrsize = 16392;

    if ( cflag && sysid[0] == '\0' ) {
	errorexit(cmd, 1, 7, "Need a System ID to proceed");
    }

    if ( !strlen(hostname) ) {
	errorexit(cmd, 1, 7, "Need a hostname to proceed");
    }

    snprintf(sessionfile, sizeof(sessionfile), "/tmp/.%s.%d.lock",
	     hostname, timekey);

    if ( (sesslock = LockSession(sessionfile)) < 0 ) {
	errorexit(cmd, 1, 7, "Unable to get session lock for session");
    }


    SGMSSDBInit(1);

    /* Check whether request came from localhost or remote host.  If it
       came from localhost, then, there might be a hostname change of
       SGM/SEM system */

    if ( cflag ) {
	if ( SGMICheckSystem(hostname, sysid) != 1 ) {
	    err = ERR(CLIENTERR, PARAMERR, INVALIDARGS);
	} else {
	    err = SGMINotifySystemChange(timekey, hostname, sysid);
	}
    } else {
	err = SGMIGetConfigData(hostname, maxxfrsize, timekey);
    }

    SGMSSDBDone(1);

    UnLockSession(sesslock, sessionfile);

    if ( err ) {
        pSSSErr(err);
	errorexit(cmd, 1, 2, "Unable to get configuration info from %s (%d)",
		  hostname, err);
    }

    exit(0);
}

