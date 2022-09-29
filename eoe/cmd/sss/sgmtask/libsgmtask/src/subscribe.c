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

#ident           "$Revision: 1.10 $"

/*---------------------------------------------------------------------------*/
/* subscribe.c                                                               */
/*   This is the client-side implementation of SUBSCRIBEPROC and UNSUBSCRIBE */
/*   routines.  The main entry function for SUBSCRIBEPROC is subscribe.      */
/*   The main entry function for UNSUBSCRIBEPROC is unsubscribe.             */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*  Include files                                                            */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <ssdbapi.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include "sgmauth.h"
#include "subscribe.h"
#include <sys/types.h>

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

extern int  SGMSSDBInsertSysInfoChange(char *);
extern void *SGMIcalloc(size_t, size_t);
extern int SGMSUBSICheckHostname(char *, char *, int);
extern int pack_data(char **, char *, int, void *, int, void *, int, int);
extern int unpack_data(char *, int, void **, int, void **, char *, int, int);
extern void SGMIfree(char *, void *);
extern int SGMSUBSICheckHostname(char *, char *, int);
extern int SGMSSDBGetSystemDetails(sysinfo_t *, int, int);
extern int SGMSSDBSetTableData(char *, char *, int );
extern int SGMSSDBGetTableData(char *, char **, int *, int *, char *, char *);
extern __uint32_t SGMCLNTSSDBif(char *, subscribe_t *, class_data_t *, int, int);
extern __uint32_t SGMIrpcclient(char *, int, sgmtask_t *, int);
extern int SGMEXTISendSEMRuleConfig(int, int);
extern int SGMEXTISendSEMSgmOps(int, char *);
extern int SGMEXTISendToEventmon(char *, __uint32_t, char *);
extern void SGMEXTIReloadEventMonConfig(void);
extern int SGMAUTHIGetAuth(char *, char *, int *, int);
extern int SGMNETIGetOffHostName(char *, char *);
extern int SGMNETICheckHostIP(char *, char *);
extern int SGMSSDBCheckTableData(char *, int *);
static int SGMSUBSIProcessInputData(class_data_t *, char **);
__uint32_t SGMSUBSISubscribe(class_data_t *, char *, int);
__uint32_t SGMSUBSIUnSubscribe(char *, int);

/*---------------------------------------------------------------------------*/
/* Name    : SGMSUBSICheckHostEvents                                         */
/* Purpose : Right now, just a stub                                          */
/* Inputs  : Event data                                                      */
/* Outputs : None                                                            */
/*---------------------------------------------------------------------------*/

static int SGMSUBSICheckHostEvents(subscribe_t *sem)
{
    char       sqlstr[512];
    __uint32_t err = 0;
    int        nrows = 0;


    if ( !sem ) return(0);

    snprintf(sqlstr, sizeof(sqlstr), 
	"select type_id from event_type where sys_id = '%s' and enabled = 1",
        sem->sysinfo.sys_id);

    err = SGMSSDBCheckTableData(sqlstr, &nrows);

    if ( err ) return(0);
    else if ( nrows ) return(0);
    return(1);
}

static int SGMSUBSIUpdateSubscriptionTable(int proc)
{
    char    sqlstr[1024];
    int     err = 0;

    if ( proc == SUBSCRIBEPROC ) {
	snprintf(sqlstr, sizeof(sqlstr), 
	  "update sss_config set curr_subscriptions=curr_subscriptions+1 where "
	  " curr_subscriptions < max_subscriptions");
    } else {
	snprintf(sqlstr, sizeof(sqlstr),
	  "update sss_config set curr_subscriptions=curr_subscriptions-1 "
	  " where curr_subscriptions > 0");
    }

    err = SGMSSDBSetTableData(sqlstr,"sss_config",SSDB_REQTYPE_UPDATE);

    if ( err ) return(0);
    return(1);
}

/*---------------------------------------------------------------------------*/
/* Name    : process_data                                                    */
/* Purpose : Takes a huge chunk of class data and creates a packet for RPC   */
/* Inputs  : Pointer to data and pointer to return data                      */
/* Outputs : No. of bytes in the packet or 0 for error                       */
/*---------------------------------------------------------------------------*/

static int SGMSUBSIProcessInputData(class_data_t *subscribe_data, char **ptr)
{
    int    i = 0;
    int    iNbytes = 0;
    int    iTmpbytes = 0;
    class_data_t *Ptemp = subscribe_data;
    type_data_t  *Ptemptype;

    if ( !subscribe_data || !ptr ) return(0);

    while ( Ptemp != NULL ) {
	iNbytes += 10 + Ptemp->nooftypes*(12+1);
	Ptemp = Ptemp->next;
    }

    iNbytes += 50;
    iTmpbytes = 0;
    if ( ((*ptr) = (char *) SGMIcalloc(iNbytes, 1)) != NULL ) {
	int          i = 0;

	Ptemp = subscribe_data;
	while ( Ptemp ) {
	    i = 0;
	    iTmpbytes += snprintf((*ptr)+iTmpbytes, iNbytes-iTmpbytes, 
				  "%d", Ptemp->class_id);
	    Ptemptype = Ptemp->type;
	    while ( Ptemptype ) {
		iTmpbytes += snprintf( (*ptr)+iTmpbytes, iNbytes-iTmpbytes,
				       "%c%d", ((i==0) ? '|' : ','),
				       Ptemptype->type_id);
		Ptemptype = Ptemptype->next;
		i++;
	    }

	    iTmpbytes += snprintf( (*ptr)+iTmpbytes, iNbytes-iTmpbytes,"&");
	    Ptemp = Ptemp->next;
	}

	iTmpbytes += 1; /* For the NULL pointer */

    } 

    return(iTmpbytes);
}

/*---------------------------------------------------------------------------*/
/* Name    : SGMSUBSISubscribe                                               */
/* Purpose : Main entry point to subscribe process                           */
/* Inputs  : Event data to subscribe to and remote host                      */
/* Outputs : Error or 0 for success                                          */
/*---------------------------------------------------------------------------*/

__uint32_t SGMSUBSISubscribe(class_data_t *sub_data, char *hname, int proc)
{
    subscribe_t   sgmsub;
    subscribe_t   *semsub = NULL;
    sgmtask_t     task;
    __uint32_t    err = 0;
    __uint32_t    err1 = 0;
    int           nrows = 0, ncols = 0;
    int           opts = 0;
    char          sqlstr[1024];
    sgmtaskrd_t   *retdata;
    char          *sqlData = NULL;
    char          *sgmevdata = NULL;
    char          *semevdata = NULL;
    char          authStr[AUTHSIZE];
    char          hostname[512];
    int           authType = 0, authlen = 0;
    int           configupdatereqd = 0;


    if ( !sub_data || !hname ) 
	return (ERR(CLIENTERR, PARAMERR, INVALIDARGS));

    if ( proc != SUBSCRIBEPROC && proc != UNSUBSCRIBEPROC )
	return (ERR(CLIENTERR, PARAMERR, INVALIDARGS));

    memset(&sgmsub, 0, sizeof(subscribe_t));
    memset(&task, 0, sizeof(sgmtask_t));
    memset(hostname, 0, sizeof(hostname));

    /*------------------------------------------------------------------------*/
    /* Fix for bug # 691104 where in if the host has more than one name, we   */
    /* are inserting a new entry in system table when different alias is      */
    /* specified at a different time.  The fix is to get the official hostname*/
    /* and look for it.                                                       */
    /* We don't need to worry about the system info. data which was received  */
    /* from SEM because the startup script always inserts official hostname   */
    /* on SEM.                                                                */
    /*------------------------------------------------------------------------*/

    if ( !SGMNETIGetOffHostName(hname, hostname) ) {
	return(ERR(CLIENTERR, NETERR, INVALIDHOST));
    }

    /*-----------------------------------------------------------------------*/
    /*  Get local system configuration (SGM) data.  We need to send this     */
    /*  over to SEM especially, the hostname.                                */
    /*-----------------------------------------------------------------------*/

    if ( (authlen = SGMAUTHIGetAuth(hostname, authStr, &authType,0)) == 0 ) {
         err = ERR(CLIENTERR, AUTHERR, CANTGETAUTHINFO);
	 goto end;
    }

    if ( (err = (int) SGMSSDBGetSystemDetails(&sgmsub.sysinfo, 1, 1)) != 0 ) {
        err = SSSERR(CLIENTERR, SSDBERR, err);
	goto end;
    } 

    /*-----------------------------------------------------------------------*/
    /*  Lets do a silly check.  We will see if someone is subscribing to     */
    /*  themselves                                                           */
    /*-----------------------------------------------------------------------*/

    if ( SGMSUBSICheckHostname(sgmsub.sysinfo.hostname, hostname, 0) ) {
	err = ERR(CLIENTERR, PARAMERR, INVALIDHOSTNAME);
	goto end;
    }

    /*-----------------------------------------------------------------------*/
    /*  Start of Phase1 of subscribe.  In this phase, we contact SEM and     */
    /*  ask it to update its tables with TEMP data.  Once SEM updates, it    */
    /*  will send us a PHASE1_OK confirmation.  This leads to phase 2.       */
    /*-----------------------------------------------------------------------*/

    sgmsub.phase   = PHASE1;
    sgmsub.errCode = 0;
    sgmsub.timekey = time(0);

    /*-----------------------------------------------------------------------*/
    /*  First, lets process eventdata passed to us                           */
    /*-----------------------------------------------------------------------*/

    if ( (sgmsub.datalen=SGMSUBSIProcessInputData(sub_data,&sgmevdata)) <= 0 ){
	err = ERR(CLIENTERR, OSERR, MALLOCERR);
	goto end;
    }

    /*-----------------------------------------------------------------------*/
    /*  Now, Form Data Packet to be sent thro' RPC                           */
    /*-----------------------------------------------------------------------*/

    task.datalen = pack_data((char **)&task.dataptr, authStr, authlen, &sgmsub, 
			     sizeof(subscribe_t), (void *) sgmevdata, 
			     sgmsub.datalen, 0);

    if ( !task.datalen )  {
	err = ERR(CLIENTERR, OSERR, MALLOCERR);
	goto end;
    }

    task.rethandle = NULL;

    /*-----------------------------------------------------------------------*/
    /*  Now, initaite RPC client call                                        */
    /*-----------------------------------------------------------------------*/

    err = SGMIrpcclient(hostname, proc, &task, 0);
    SGMIfree("Subscribe:2", task.dataptr);
    task.dataptr = NULL;

    if ( err ) goto end;

    /*-----------------------------------------------------------------------*/
    /*  We need to receive data from SEM.  SEM will send us                  */
    /*  a.  Its own sysinfo details                                          */
    /*  b.  Any records that were rejected while inserting                   */
    /*  This data will be in task.rethandle.  We take the data and unpack it.*/
    /*-----------------------------------------------------------------------*/

    retdata = (sgmtaskrd_t *) task.rethandle;

    if ( !retdata ) {
	err = ERR(SERVERERR, SSSERROR, MALLOCERR);
	goto end;
    } else if ( !retdata->datalen || !retdata->dataptr ) {
	err = ERR(SERVERERR, SSSERROR, MALLOCERR);
	goto end;
    } 
    
    if ( !unpack_data(authStr, authlen, (void **)&semsub,sizeof(subscribe_t),
                   (void **)&semevdata,retdata->dataptr, retdata->datalen, 0)) {
	err = ERR(CLIENTERR, RPCERR, CLOBBEREDDATA); 
        SGMIfree("Subscribe:3", retdata->dataptr);
	goto end;
    }

    /*-----------------------------------------------------------------------*/
    /*  Check whether phase1 went OK with SEM.  If so,  Update Tables :      */
    /*  event_type, system. Then, send a SUBSCRIBE event to SGM              */
    /*-----------------------------------------------------------------------*/

    if ( !semsub || semsub->errCode != 0 ) {
	SGMIfree("Subscribe:4", retdata->dataptr);
	SGMIfree("Subscribe:5", retdata);
	retdata = NULL;
	goto end;
    } else if ( semsub->phase != PHASE1_OK && semsub->phase != UNSUBSCRIBE_OK) {
	if (retdata->dataptr) SGMIfree("Subscribe:6",retdata->dataptr);
	if (retdata) SGMIfree("Subscribe:7",retdata);
	retdata = NULL;
	goto end;
    } else {
        if ( proc == SUBSCRIBEPROC ) {

	    /*----------------------------------------------------------------*/
	    /* Check whether SYSID already exists or not.  If it doesn't exist*/
	    /* then we have a new host.                                       */
	    /*----------------------------------------------------------------*/

            snprintf(sqlstr, sizeof(sqlstr), 
		     "select hostname from system where sys_id = '%s'", 
                     semsub->sysinfo.sys_id);
            nrows = 0;
            err = SGMSSDBCheckTableData(sqlstr, &nrows);
	    if ( !err ) {
                if ( nrows > 0 ) opts |= ENBLHOST;
                else opts |= NEWHOST;

                /*------------------------------------------------------------*/
		/* Lets check whether we reached the Maximum subscriptions    */
		/* limit.  If we reached our limit,  lets bail out..          */
		/*------------------------------------------------------------*/

		nrows = 0;
                snprintf(sqlstr, sizeof(sqlstr), 
			 "select curr_subscriptions from sss_config where "
			 " curr_subscriptions < max_subscriptions");

		SGMSSDBCheckTableData(sqlstr, &nrows);
		configupdatereqd = SGMSUBSICheckHostEvents(semsub);

		if ( nrows == 0 && configupdatereqd) err = TOOMANYSUBSCRIPTIONS;
                else {

		    /*--------------------------------------------------------*/
		    /* Otherwise, lets insert data in SSDB.  If everything    */
		    /* goes fine, then, lets update our sss_config table with */
		    /* a new subscription only if it is a NEWHOST             */
		    /*--------------------------------------------------------*/

		    err = SGMCLNTSSDBif(hostname,semsub, sub_data, 
							    SUBSCRIBE, opts);
		    if (err) {
		       SGMCLNTSSDBif(hostname,semsub,sub_data, CLEANUP, opts);
		    } else if ( configupdatereqd ) {
		       SGMSUBSIUpdateSubscriptionTable(proc);
		    }
		}
            } 
        } else {

            SGMCLNTSSDBif(hostname,semsub, sub_data, UNSUBSCRIBE, 0);

	    /*----------------------------------------------------------------*/
	    /* Lets see if we have all the events unsubscribed.  If so, then  */
	    /* we automatically UNSUBSCRIBE the system so that no events are  */
	    /* accepted from that system.  Also, we will make space for anothe*/
	    /* system if maximum subscription limit is reached.               */
	    /*----------------------------------------------------------------*/

	    if ( SGMSUBSICheckHostEvents(semsub) ) {
		snprintf(sqlstr, sizeof(sqlstr), 
		    "update system set active = 0 where sys_id = '%s'",
		    semsub->sysinfo.sys_id);
		SGMSSDBSetTableData(sqlstr,"system", SSDB_REQTYPE_UPDATE);
		SGMSUBSIUpdateSubscriptionTable(proc);
		SGMEXTISendSEMSgmOps(proc, semsub->sysinfo.sys_id);
	    }
        }

	if ( err ) {
	    sgmsub.phase = PHASE2_NOK;
            err1 = SSSERR(CLIENTERR, SSDBERR, err);
	} else {
	    if ((opts & NEWHOST) || configupdatereqd )
                 SGMEXTISendSEMSgmOps(proc,semsub->sysinfo.sys_id);
            SGMEXTIReloadEventMonConfig();
	    sgmsub.phase = PHASE2;
	}
    }

    /*-----------------------------------------------------------------------*/
    /*  We are all done on SGM side.  Now, Send PHASE2 to SEM so that it will*/
    /*  enable the TEMP data it created before                               */
    /*-----------------------------------------------------------------------*/

    SGMIfree("Subscribe:8",retdata->dataptr);
    SGMIfree("Subscribe:9",retdata);
    retdata = NULL;

    if ( proc == UNSUBSCRIBEPROC ) goto end;

    sgmsub.datalen = 0;

    task.datalen = pack_data((char **)&task.dataptr, authStr, authlen, &sgmsub, 
			     sizeof(subscribe_t), (void *) 0, 
			     sgmsub.datalen, 0);

    if ( !task.datalen )  {
	err = ERR(CLIENTERR, OSERR, MALLOCERR);
	goto end;
    }

    task.rethandle = NULL;


    err = SGMIrpcclient(hostname, SUBSCRIBEPROC, &task, 0);
    SGMIfree("Subscribe:10", task.dataptr);
    task.dataptr = NULL;

    if ( err ) goto end;

    /*-----------------------------------------------------------------------*/
    /*  Check whether we received SUBSCRIBE_OK from SEM.  If so, all went    */
    /*  well.  Otherwise, delete whatever entries we inserted in SSDB.       */
    /*-----------------------------------------------------------------------*/

    retdata = (sgmtaskrd_t *) task.rethandle;

    if ( !retdata ) {
        err = ERR(SERVERERR, SSSERROR, MALLOCERR);
        goto end;
    } else if ( !retdata->datalen || !retdata->dataptr ) {
        err = ERR(SERVERERR, SSSERROR, MALLOCERR);
        goto end;
    } else {
	subscribe_t   *tmpSubscribe = NULL;

	if ( !unpack_data(authStr, authlen,(void **)&tmpSubscribe, sizeof(subscribe_t),
			  NULL, retdata->dataptr, retdata->datalen, 0)) {
	    err = ERR(CLIENTERR, RPCERR, CLOBBEREDDATA);
	    goto end;
        }

	if ( sgmsub.phase == PHASE2_NOK && tmpSubscribe->phase != CLEANUP_OK ) {
	    err = SSSERR(SERVERERR, SSDBERR, 0);
	} else if ( sgmsub.phase == PHASE2_NOK && tmpSubscribe->phase == CLEANUP_OK) {
            goto end;
	} else if ( tmpSubscribe->phase != SUBSCRIBE_OK ) {
	    err = ERR(SERVERERR, SSSERROR, INVALIDARGS);
	} 

        SGMSSDBInsertSysInfoChange(sgmsub.sysinfo.sys_id);
    }

    end:
	if ( sqlData ) SGMIfree("Subscribe:11", sqlData);
	if ( retdata ) SGMIfree("Subscribe:12", retdata);
	if ( sgmevdata ) SGMIfree("Subscribe:13", sgmevdata);
	if ( err1 ) return(err1);
        return(err);
}
