#include <stdio.h>
#include <sys/types.h>
#include <ssdbapi.h>
#include <string.h>
#include "sgmtaskerr.h"
#include "sgmauth.h"

__uint32_t  getData_srvr(char *SGMHost, char **dPtr, __uint32_t *dLen,
                               char *data, __uint32_t datalen)
{
    char                    *sqlstr = NULL;
    char                    authStr[AUTHSIZE];
    int                     err = 0;
    int                     nrows = 0, ncols = 0;
    int                     authlen = 0, authType;
    char                    *tmpPtr = NULL;


    if ( !SGMHost || !data || datalen <= 0 ) 
	return ERR(SERVERERR, PARAMERR, INCOMPLETEDATA);
    
    if ( dPtr == NULL || dLen == NULL ) {
        return(ERR(SERVERERR, PARAMERR, INVALIDDATAPTR));
    }

    if ( (err=SGMSSDBInit(1)) != 0 ) {
        return(SSSERR(SERVERERR, SSDBERR, err));
    }

    if ( (authlen = SGMAUTHIGetAuth(SGMHost, authStr, &authType, 0)) == 0)
        return(ERR(SERVERERR, AUTHERR, CANTGETAUTHINFO));

    if ( !unpack_data(authStr, authlen, &sqlstr, 0, NULL, data, datalen, 0) ) {
	return(ERR(SERVERERR, AUTHERR, CANTGETAUTHINFO));
    }

    if ( sqlstr && !strncasecmp(sqlstr, "select", 6)) {
	err = SGMSSDBGetTableData(sqlstr, &tmpPtr, &nrows, &ncols, "%&^", "|");
	if ( err ) return(SSSERR(SERVERERR, SSDBERR, err));
	else if (!tmpPtr) {
	    return(SSSERR(SERVERERR, SSDBERR, NORECORDSFOUND));
	} else {
	    *dLen = pack_data(dPtr, authStr, authlen, tmpPtr, strlen(tmpPtr)+1,
			      NULL, 0, authType);
	    free(tmpPtr);
	}
    } else {
	return ERR(SERVERERR, PARAMERR, INCOMPLETEDATA);
    }

    SGMSSDBDone(1);
    return(0);
}
