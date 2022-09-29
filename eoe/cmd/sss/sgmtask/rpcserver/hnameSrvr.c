#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <evmonapi.h>
#include <ssdbapi.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include "sgmauth.h"
#include "subscribe.h"

__uint32_t  sgmhname_change(char *SGMhost,void *mydata, int dlen)
{

    char          sqlstr[1024];
    __uint32_t    err = 0;
    int           i = 0;
    int           nrows = 0, ncols = 0;
    int           action_id = 0;
    sysinfo_t     *mysys = NULL;
    char          *p = NULL;
    char          *data = NULL;

    if ( !SGMhost || !mydata || !dlen )
        return(ERR(SERVERERR, PARAMERR, INVALIDARGS));

    if ( dlen != sizeof(sysinfo_t) ) return(ERR(SERVERERR, RPCERR, CLOBBEREDDATA));

    
    doencr(mydata, STANDARD_KEY, STANDARD_MASK, dlen);

    mysys = (sysinfo_t *) mydata;

    if ( !mysys ) return (ERR(SERVERERR, RPCERR, CLOBBEREDDATA));

    if ( (err=SGMSSDBInit(1)) != 0 ) {
	return(SSSERR(SERVERERR, SSDBERR, err));
    }

    if ( strlen(mysys->sys_id) == 0 ) return(0);

    snprintf(sqlstr, sizeof(sqlstr), "select auth_host from sss_auth where auth_user = '%s'", mysys->sys_id);
    err = SGMSSDBGetTableData(sqlstr, &data, &nrows, &ncols, "@", "|");

    if ( err ) return(SSSERR(SERVERERR, SSDBERR, err));
    else if ( !nrows || !data )  return(0);
    else if ( nrows > 1 ) return(ERR(SERVERERR, PARAMERR, INVALIDMULTIPLEHOST));
    else {

	/* Update authentication information */

        snprintf(sqlstr, sizeof(sqlstr), "update sss_auth set auth_host = '%s' where auth_user = '%s'", 
	     SGMhost, mysys->sys_id);

        SGMSSDBSetTableData(sqlstr, "sss_auth", SSDB_REQTYPE_UPDATE);

	/* Update actions */

	p = strtok(data, "@|");
	action_id = SGMSSDBGetActionID("forward_hostname", p);
	if ( action_id == 0 ) return(0);
	snprintf(sqlstr, sizeof(sqlstr), "update event_action set forward_hostname = '%s' where action_id = %d", SGMhost, action_id);
	err = SGMSSDBSetTableData(sqlstr, "event_action", SSDB_REQTYPE_UPDATE);
	if ( err ) return(SSSERR(SERVERERR, SSDBERR, err));
    }

    SGMSSDBDone(1);
    if ( action_id ) SGMEXTISendSEMRuleConfig(action_id, 3);
    return(0);
}

__uint32_t semhname_change(char *SEMhost,void *mydata, int dlen)
{
    char            sqlstr[1024];
    char            *sysid = NULL;
    __uint32_t      err = 0;
    int             i = 0, nextptr = 0;
    char            *p = NULL;
    int             nrows = 0, ncols = 0;
    char            oldhostname[257];
    char            *data = NULL;
    sysinfo_t       *mysys = NULL;

    if ( !SEMhost || !mydata || !dlen )
        return(ERR(SERVERERR, PARAMERR, INVALIDARGS));

    if ( dlen != sizeof(sysinfo_t) ) return(ERR(SERVERERR, RPCERR, CLOBBEREDDATA));

   
    doencr(mydata, STANDARD_KEY, STANDARD_MASK, dlen);

    mysys = (sysinfo_t *) mydata;

    if ( !mysys ) return (ERR(SERVERERR, RPCERR, CLOBBEREDDATA));

    if ( (err=SGMSSDBInit(1)) != 0 ) {
        return(SSSERR(SERVERERR, SSDBERR, err));
    }

    if ( strlen(mysys->sys_id) == 0 ) return(0);

    /* Get Old hostname from System Table */

    snprintf(sqlstr, sizeof(sqlstr), 
	     "select hostname from system where sys_id = '%s' and active = 1",
	     mysys->sys_id);

    err = SGMSSDBGetTableData(sqlstr, &p, &nrows, &ncols, "@", "|");

    if ( err ) return(SSSERR(SERVERERR, SSDBERR, err));
    else if ( !nrows || !p ) return(0);
    else if ( nrows > 1 ) return(ERR(SERVERERR, PARAMERR, INVALIDMULTIPLEHOST));
    else {
	strcpy(oldhostname, strtok(p, "@|"));
	SGMIfree("p", p);
    }

    err = SGMSSDBSetSystemDetails(SEMhost, mysys, 0);
    if ( err ) return(SSSERR(SERVERERR, SSDBERR, err));
    snprintf(sqlstr, sizeof(sqlstr), 
	  "update sss_auth set auth_host = '%s' where auth_host like '%s%s'", 
	  SEMhost, oldhostname, "%%");
    err = SGMSSDBSetTableData(sqlstr, "sss_auth", SSDB_REQTYPE_UPDATE);

    if ( err ) return (SSSERR(SERVERERR, SSDBERR, err));
    SGMSSDBDone(1);
    SGMEXTISendSEMSgmOps(SUBSCRIBEPROC, mysys->sys_id);
    return(0);
}
