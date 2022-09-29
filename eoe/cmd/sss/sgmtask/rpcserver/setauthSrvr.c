#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <evmonapi.h>
#include <ssdbapi.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include "sgmauth.h"

__uint32_t  setauth_srvr(char *SGMhost,void *data, int dlen)
{

    char          sqlstr[1024];
    __uint32_t    auth_key;
    __uint32_t    auth_mask;
    char          pwd[30];
    int           auth_type;
    __uint32_t    err = 0;
    char          *mydata = NULL;
    int           i = 0, nextptr = 0;
    char          *sysid = NULL;

    if ( !SGMhost || !data || !dlen )
        return(ERR(SERVERERR, PARAMERR, INVALIDARGS));

    /* Unpack the data */
    
    doencr(data, STANDARD_KEY, STANDARD_MASK, dlen);

    if ( (err=SGMSSDBInit(1)) != 0 ) {
	return(SSSERR(SERVERERR, SSDBERR, err));
    }

    if (!getauthdetails((char *) data, &auth_key, &auth_mask, pwd, &auth_type)){
	return(ERR(SERVERERR, AUTHERR, CANTCREATEAUTHINFO));
    }

    mydata = (char *) data;
    for ( i = 0; i < dlen-5; i++ ) {
	if ( *(mydata+i) == 'S' && *(mydata+i+1) == 'Y' &&
	     *(mydata+i+2) == 'S' && *(mydata+i+3) == 'I' &&
	     *(mydata+i+4) == 'D' ) {
	    nextptr = i+5;
	    break;
        }
    }

    if ( nextptr < dlen ) sysid = mydata+nextptr+1;

    if ( sysid == NULL ) {
        snprintf(sqlstr, 1024, "delete from sss_auth where auth_host like '%s%s'",
	     SGMhost, "%%");
    } else {
        snprintf(sqlstr, 1024, "delete from sss_auth where auth_host like '%s%s' and auth_user = '%s'",
	     SGMhost, "%%", sysid);
    }

    if ( (err = SGMSSDBSetTableData(sqlstr,"sss_auth", SSDB_REQTYPE_DELETE) )) {
	return(SSSERR(SERVERERR, SSDBERR, err));
    }

    if ( sysid == NULL ) {
        snprintf(sqlstr, 1024,"insert into sss_auth values('%s','', '%x', '%x', '%s', %d)",
		 SGMhost, auth_key, auth_mask, pwd, auth_type);
    } else {
        snprintf(sqlstr, 1024,"insert into sss_auth values('%s','%s', '%x', '%x', '%s', %d)",
		 SGMhost, sysid, auth_key, auth_mask, pwd, auth_type);
    }

    if ( (err = SGMSSDBSetTableData(sqlstr,"sss_auth", SSDB_REQTYPE_INSERT) ) ){
	return(SSSERR(SERVERERR, SSDBERR, err));
    }

    SGMSSDBDone(1);
    return(0);
}
