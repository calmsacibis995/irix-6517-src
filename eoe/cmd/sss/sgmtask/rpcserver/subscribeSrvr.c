#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include "sgmtaskerr.h"
#include "subscribe.h"
#include "sgmtask.h"
#include "sgmauth.h"

__uint32_t  unsubscribe_srvr(char *SGMHost, char **dPtr, __uint32_t *dLen,
                             char *subData, __uint32_t subDataLen)
{
    subscribe_t   *sgmsub = NULL;
    subscribe_t    semsub;
    __uint32_t    err = 0;
    char          *tmpEvData = NULL;
    char          *retData = NULL;
    char          authStr[AUTHSIZE];
    int           authlen = 0, authType;
    int           alignment = 0;
    int           oActID = 0, nActID =0;

    memset(&semsub, 0, sizeof(subscribe_t));

    if ( !subData || !dPtr || !dLen ) 
	return(ERR(SERVERERR, PARAMERR, INVALIDARGS));

    if ( (err=SGMSSDBInit(1)) != 0 ) {
        return(SSSERR(SERVERERR, SSDBERR, err));
    }

    if ( (authlen = SGMAUTHIGetAuth(SGMHost, authStr, &authType, 0)) == 0)
        return(ERR(SERVERERR, AUTHERR, CANTGETAUTHINFO));

    if ( !unpack_data(authStr, authlen, &sgmsub, sizeof(subscribe_t),
                      &tmpEvData, subData, subDataLen, 0) ) {
        return(ERR(SERVERERR, AUTHERR, CANTGETAUTHINFO));
    }

    alignment = 8-(authlen%8); 

    if ( subDataLen != (authlen+sizeof(subscribe_t) + sgmsub->datalen + alignment) ) {
	return (ERR(SERVERERR, RPCERR, CLOBBEREDDATA));
    }

    if ( SGMSSDBGetSystemDetails(&semsub.sysinfo, 1, 1) ) {
	return(SSSERR(SERVERERR, SSDBERR, UNKNOWNSSDBERR));
    }

    if ( sgmsub->phase == PHASE1 ) {
	oActID=SGMSSDBGetActionID("forward_hostname",sgmsub->sysinfo.hostname);
        if ( (err=SGMSRVRSSDBif(SGMHost, &semsub, sgmsub, UNSUBSCRIBE, PROCDATA, tmpEvData)) ) {
	       return(SSSERR(SERVERERR, SSDBERR, err));
        }
	nActID=SGMSSDBGetActionID("forward_hostname",sgmsub->sysinfo.hostname);

	if ( nActID == 0 && oActID != 0 ) {
	    /* Action ID got deleted.  We need to inform SEM about that */
	    SGMEXTISendSEMRuleConfig(oActID, 2);
	} else if ( oActID == nActID ) {
	    /* Someone Unsubscribed only a part of events */
	    SGMEXTISendSEMRuleConfig(oActID, 3);
	}

        semsub.phase = UNSUBSCRIBE_OK;
        semsub.datalen = 0;
    }

    semsub.timekey = 0;
    *dLen = pack_data(dPtr, authStr, authlen, &semsub, sizeof(subscribe_t),
		      retData, semsub.datalen, 0); 
    if ( !(*dLen) ) {
        if ( retData) free(retData);
        return(ERR(SERVERERR, OSERR, MALLOCERR));
    }

    if ( retData ) free(retData);
    SGMSSDBDone(1);
    return 0;

}

__uint32_t  subscribe_srvr(char *SGMHost, char **dPtr, __uint32_t *dLen,
                           char *subData, __uint32_t subDataLen)
{
    subscribe_t    *sgmsub = NULL;
    subscribe_t    semsub;
    __uint32_t     err = 0;
    char           *tmpEvData = NULL;
    char           *retData = NULL;
    char           authStr[AUTHSIZE];
    int            authlen = 0, authType;
    int            alignment = 0;
    int            nActID = 0, oActID = 0;
    __uint32_t     hostid = 0;


    memset(&semsub, 0, sizeof(subscribe_t));

    if ( !subData || !dPtr ||  !dLen ) 
	return(ERR(SERVERERR, PARAMERR, INVALIDARGS));


    if ( (err=SGMSSDBInit(1)) != 0 ) {
        return(SSSERR(SERVERERR, SSDBERR, err));
    }

    if ( (authlen = SGMAUTHIGetAuth(SGMHost, authStr, &authType, 0)) == 0)
        return(ERR(SERVERERR, AUTHERR, CANTGETAUTHINFO));

    if ( !unpack_data(authStr, authlen, &sgmsub, sizeof(subscribe_t),
                      &tmpEvData, subData, subDataLen, 0) ) {
        return(ERR(SERVERERR, AUTHERR, CANTUNPACKDATA));
    }

    
    alignment = 8-(authlen%8); 

    if ( subDataLen != (authlen+sizeof(subscribe_t) + sgmsub->datalen + alignment) ) {
	return (ERR(SERVERERR, RPCERR, CLOBBEREDDATA));
    }


    if ( SGMSSDBGetSystemDetails(&semsub.sysinfo, 1, 1) ) {
	return(SSSERR(SERVERERR, SSDBERR, UNKNOWNSSDBERR));
    } 

    switch(sgmsub->phase)
    {
	case PHASE1:
	    if ( (err=SGMSRVRSSDBif(SGMHost, &semsub, sgmsub, SUBSCRIBE, PROCDATA, tmpEvData)) ) {
                SGMSRVRSSDBif(SGMHost, &semsub, sgmsub, SUBSCRIBE, CLEANUP, NULL);
		return(SSSERR(SERVERERR, SSDBERR, err));
	    }
	    semsub.phase = PHASE1_OK;
	    semsub.errCode = 0;
	    break;
	case PHASE2:
	    oActID=SGMSSDBGetActionID("forward_hostname",sgmsub->sysinfo.hostname);
	    if ( (err=SGMSRVRSSDBif(SGMHost, &semsub, sgmsub, SUBSCRIBE, ENBLHOST, NULL)) ) {
                SGMSRVRSSDBif(SGMHost, &semsub, sgmsub, SUBSCRIBE, CLEANUP, NULL);
		return(SSSERR(SERVERERR, SSDBERR, err));
	    }

            SGMSSDBInsertSysInfoChange(semsub.sysinfo.sys_id);

	    nActID=SGMSSDBGetActionID("forward_hostname",sgmsub->sysinfo.hostname);
	    if ( oActID == nActID ) SGMEXTISendSEMRuleConfig(oActID, 3);
	    else if ( oActID == 0 && nActID != 0 ) 
		SGMEXTISendSEMRuleConfig(nActID, 1);

	    semsub.phase = SUBSCRIBE_OK;
	    semsub.datalen = 0;
	    break;
	case PHASE2_NOK:
	   if ( (err=SGMSRVRSSDBif(SGMHost, &semsub, sgmsub, SUBSCRIBE, CLEANUP, NULL))){
	       return(ERR(SERVERERR, SSDBERR, err));
	   }
           semsub.phase = CLEANUP_OK;
           semsub.datalen = 0;
           break;
    }

    semsub.timekey = 0;
    *dLen = pack_data(dPtr, authStr, authlen, &semsub, sizeof(subscribe_t),
		      retData, semsub.datalen, 0); 
    if ( !(*dLen) ) {
        if ( retData) free(retData);
        return(ERR(SERVERERR, OSERR, MALLOCERR));
    }

    if ( retData ) free(retData);

    SGMSSDBDone(1);
    return 0;

}
