#include "sspingdefs.h"
#include <ssdbapi.h>

extern service_t       *espservices;
extern hostservice_t   *esphservices;
extern int             numhservices;
extern int             restartdaemon;

int addHostService(char *host, char *service)
{
    int   serindex = -1;
    int   hostindex = 0;
    int   i = 0;

    if ( !host || !service || !espservices || !esphservices ) return(0);

    hostindex = checkHost(host);
    if ( hostindex < 0 ) return(HOSTNOTFOUND);

    if ( (serindex = checkService(espservices, service)) < 0 ) {
	return(SERVICENOTFOUND);
    } else {
	if ( esphservices[hostindex].nextservice > MAXSERVICESPERHOST-1 )
	    return(TOOMANYSERVICES);
	for ( i = 0; i <= esphservices[hostindex].nextservice; i++ ) {
	    if ( esphservices[hostindex].services[i] == serindex ) return(0);
	}
	esphservices[hostindex].nextservice++;
	esphservices[hostindex].services[esphservices[hostindex].nextservice] = serindex;
	return(0);
    }
}

int deleteHostService(char *host, char *service)
{
    int   serindex = -1;
    int   hostindex = 0;
    int   i = 0;
    int   found = 0;

    if ( !host || !service ) return(0);

    hostindex = checkHost(host);
    if ( hostindex < 0 ) return(HOSTNOTFOUND);

    if ( (serindex = checkService(espservices, service)) < 0 ) {
	/* return(SERVICENOTFOUND); */
        return(0);
    } else {
	for ( i = 0; i <= esphservices[hostindex].nextservice; i++ ) {
	    if ( esphservices[hostindex].services[i] == serindex ) {
		esphservices[hostindex].services[i] = -1;
                found = 1;
	    }
	}
    }

    if ( !found ) return(SERVICENOTFOUND);
    return(0);
}

int processservice(char *host, char *ptr, int flag)
{
    char  *tmpptr = ptr;
    char  *sername = NULL;
    int   i = 0;
    int   err = 0;

    if ( !host || !ptr ) return(INVALIDARGS);

    if ( flag == UPDATE ) resetHostService(host);

    while ( (sername = strtok(tmpptr, " ,\n")) != NULL ) {
	switch(flag)
	{
	    case ADD:
	    case UPDATE:
		err = addHostService(host, sername);
		break;
	    case DELETE:
		err = deleteHostService(host, sername);
		break;
            default:
		return(0);
	}
	tmpptr = NULL;
    }
    /*if ( !err ) writeHostServices(MAPFILE);*/
    writeHostServices(MAPFILE);
    return(err);
}

int resetHostService(char *host)
{
    int hostindex = 0;

    if ( !host ) return(0);

    hostindex = checkHost(host);
    if ( hostindex < 0 ) return(0);

    esphservices[hostindex].nextservice = -1;
    return(1);
}

int readHostServices(char *file)
{
    FILE   *fp = NULL;
    char   readbuf[512];
    int    cmds = 0;
    char   *host = NULL;
    char   *tmpbuf = NULL;
    int    err = 0;
    char   *sername = NULL;

    if ( !file ) return(0);

    if ( (fp = fopen(file, "r")) == NULL ) {
        if ( errno != ENOENT ) return(FILENOTREADABLE);
        else return(0);
    }

    while ( fgets(readbuf, sizeof(readbuf), fp) != NULL ) {
	tmpbuf = readbuf;
	while ( *tmpbuf && isspace(*tmpbuf)) tmpbuf++;
	if ( *tmpbuf == '\0' || *tmpbuf == '\n' || *tmpbuf == '#' )
	    continue;

	host = tmpbuf++;
	while (*tmpbuf && !isspace(*tmpbuf)) tmpbuf++;
	if ( *tmpbuf ) *tmpbuf++ = '\0';

	while ( *tmpbuf && isspace(*tmpbuf)) tmpbuf++;
	if ( *tmpbuf == '\0' || *tmpbuf == '\n' ) {
	    continue;
	}

        sername = strtok(tmpbuf, " ,\n");
        while (sername != NULL) {
	    addHostService(host, sername);
	    sername = strtok(NULL, " ,\n");
	}

        /*err = processservice(host, tmpbuf, ADD);*/
    }

    fclose(fp);
    return(err);
    
}

int writeHostServices(char *file)
{
    FILE   *fp = NULL;
    int    i   = 0;
    int    j   = 0;
    int    serindex = -1;
    int    k = 0;
    mode_t orgmode = umask(S_IXUSR|S_IRWXG|S_IRWXO);

    if ( ( fp = fopen(file, "w")) == NULL ) return(0);

    for ( i = 0; i < numhservices; i++ ) {
        if ( esphservices[i].nextservice < 0 ) continue;
	fprintf(fp, "%s \t", esphservices[i].host_name);
	k = 0;
	for ( j = 0; j <= esphservices[i].nextservice; j++ ) {
	    serindex = esphservices[i].services[j];
            if ( serindex >= 0 ) {
		fprintf(fp, "%s%s", (k == 0 ? " ": ", "), 
                                espservices[serindex].service_name);
                k++;
            }
	}
        fprintf(fp, "\n");
    }

    fclose(fp);
    umask(orgmode);
    restartdaemon = 1;
    return(1);
}


int addHost(char *name, int localflag, int nhosts)
{
    hostservice_t *s = esphservices;

    if (!name ) return(0);

    if ( s == NULL ) {
	if ( (esphservices = (hostservice_t *) calloc(1, sizeof(hostservice_t))) == NULL ) {
	    return(0);
	}
	nhosts = 1;
    } else {
	if ( nhosts < 2 ) return(0);
	if ( (esphservices = (hostservice_t *) realloc(esphservices, nhosts*sizeof(hostservice_t))) == NULL ) {
	    return(0);
	}
    }
    
    /*memset((void *) &esphservices[nhosts-1], 0, sizeof(hostservice_t));*/
    strncpy(esphservices[nhosts-1].host_name, name, MAXHOSTLEN);
    esphservices[nhosts-1].nextservice = -1;
    return(1);
}

int checkHost(char *name)
{
    int i = -1;

    if ( !name || numhservices < 0 ) return(-1);

    for ( i = 0; i < numhservices; i++ ) {
	if ( !strcmp(esphservices[i].host_name, name) ) 
	    return(i);
    }

    return(-1);
}

/*&#FZ
  Get Subscribed host information from SSDB
*/

int GetHostData(void)
{
    ssdb_Error_Handle        Error = 0;
    ssdb_Connection_Handle   Connection = 0;
    ssdb_Request_Handle      Request = 0;
    ssdb_Client_Handle       Client = 0;
    int                      err = 0;
    int                      NumRows = 0, NumCols = 0;
    int                      i = 0;
    const char               **vector;
    char                     *hostname = NULL;
    int                      local = 0;

    if ( (Error = ssdbCreateErrorHandle()) == NULL ) {
        return(ssdbGetLastErrorCode(NULL));
    }

    if ( (Client = ssdbNewClient(Error, SSDBUSER, SSDBPASSWORD, 0)) == NULL)
    {
        err = ssdbGetLastErrorCode(Error);
        if (Error) ssdbDeleteErrorHandle(Error);
        return(err);
    }

    if ( !(Connection = ssdbOpenConnection(Error, Client, NULL,
                        SSDATABASE, (SSDB_CONFLG_RECONNECT |
                        SSDB_CONFLG_REPTCONNECT))) ) {
        err = ssdbGetLastErrorCode(Error);
        if (Client) ssdbDeleteClient(Error, Client);
        if (Error) ssdbDeleteErrorHandle(Error);
        return(err);
    }

    if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
                     "select hostname, local from system where active = 1"))) {
        goto end;
    }

    if ( (NumRows = ssdbGetNumRecords(Error, Request)) == 0 ) {
        goto end;
    }

    ssdbRequestSeek(Error, Request, 0, 0);
    for( i = 0; i < NumRows; i++ ) {
        vector = ssdbGetRow(Error, Request);
        if ( vector ) {
            hostname = vector[0];
            local    = atoi(vector[1]);

            if ( !addHost(hostname, local, i+1) )
	    {
		free(esphservices);
		esphservices = NULL;
                numhservices = -1;
		err = UNABLETOADDHOST;
                goto end;
	    } else {
		numhservices = i+1;
	    }
        }
    }

    end:
        err = ssdbGetLastErrorCode(Error);
        if (Request) ssdbFreeRequest(Error, Request);
        if (Connection) ssdbCloseConnection(Error, Connection);
        if (Client) ssdbDeleteClient(Error, Client);
        if (Error) ssdbDeleteErrorHandle(Error);
        return(err);
}

