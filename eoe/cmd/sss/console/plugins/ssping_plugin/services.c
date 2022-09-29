#include "sspingdefs.h"

extern service_t    *espservices;
extern int          numservices;
extern int          restartdaemon;

#define  MAXDEFSERVICES                 8

static service_t    defservices[MAXDEFSERVICES] = {
           {"icmp", "/usr/etc/ping -c 3 -f -i 4 HOST", 1},
	   {"dns",  "nslookup - HOST </dev/null", 1},
	   {"x-server", "DISPLAY=HOST:0 /usr/bin/X11/xhost", 1},
	   {"rpcbind",  "/usr/etc/rpcinfo -p HOST", 1},
	   {"smtp","( echo \"expn root\" ; echo quit ) | telnet HOST 25 | cat",1},  
	   {"nntp", "( echo \"listgroup comp.sys.sgi\"; echo quit ) | telnet HOST 119 | cat", 1},
           {"autofsd", "/usr/pcp/bin/autofsd-probe -h HOST", 1},
           {"pmcd", "/usr/pcp/bin/pmcd_wait -h HOST", 1}
 };

int setupDefaultServices(void)
{
    int    i = 0;

    if ( espservices != NULL ) return(0);

    while ( i <  MAXDEFSERVICES ) {
	if ( !addService(&espservices, defservices[i].service_name,
			 defservices[i].service_cmd, i+1) ) {
	    free(espservices);
	    espservices = NULL;
	    return(0);
        } else {
	    i += 1;
	    numservices += 1;
	}
    }
    numservices ++;
    return(1);
}

int addService(service_t **s, char *name, char *cmd, int numser)
{
    char  *p = NULL;
    service_t *tmp = *s;
    char  *actname = NULL;

    if ( !s || !name || !cmd ) return(0);

    actname = strtok(name, "@HOST");
    if ( actname == NULL ) return(0);
    
    if ( tmp == NULL ) {
	if ( (tmp = (service_t *) calloc(1, sizeof(service_t))) == NULL ) {
	    return(0);
	}
	numser = 1;
    } else {
	if ( numser < 2 ) return(0);
	if ( (tmp = (service_t *) realloc(tmp,numser*sizeof(service_t)))==NULL )
	    return(0);
    }


    strncpy(tmp[numser-1].service_name, actname, MAXSERNAMELEN);
    strncpy(tmp[numser-1].service_cmd, cmd, MAXSERCMDLEN);

    /* Trim trailing \n */

    p = strrspn(tmp[numser-1].service_cmd, "\n");
    *p = '\0';

    tmp[numser-1].flag = 1;
    *s = tmp;
    return(1);
}

int readServices(char *servicefile)
{
    FILE    *fp = NULL;
    char    readbuf[512];
    int     cmds = 0;
    char    *tmpbuf;
    char    *sername = NULL;

    /* Return -1 if there is an error ! */

    if ( !servicefile ) return(-1);

    if ( (fp = fopen(servicefile, "r")) == NULL ) {
	if ( errno != ENOENT ) return(-1);
	else {
            setupDefaultServices();
	    writeServices(servicefile, espservices);
	    return(1);
        }
    }

    while ( fgets(readbuf, sizeof(readbuf), fp) != NULL )
    {
	tmpbuf = readbuf;
	while ( *tmpbuf && isspace(*tmpbuf)) tmpbuf++;
	if ( *tmpbuf == '\0' || *tmpbuf == '\n' || *tmpbuf == '#' )
	    continue;
	
	sername = tmpbuf++;
	while ( *tmpbuf && !isspace(*tmpbuf)) tmpbuf++;
	if ( *tmpbuf ) *tmpbuf++ = '\0';
	while ( *tmpbuf && isspace(*tmpbuf)) tmpbuf++;
	if ( *tmpbuf == '\0' || *tmpbuf == '\n' ) {
	    continue;
	}

	cmds++;

	if ( !addService(&espservices, sername, tmpbuf, cmds) ) {
	    free(espservices);
	    espservices = NULL;
	    numservices = -1;
	    fclose(fp);
	    return(-1);
	}

    }

    fclose(fp);
    numservices = cmds;
    return(cmds);
}

int writeServices(char *servicefile, service_t *s)
{

    FILE  *fp = NULL;
    int   i = 0;
    service_t  *p = s;
    int   nobytes = 0;
    mode_t orgmode = umask(S_IXUSR|S_IRWXG|S_IRWXO);

    if ( !servicefile || !s || numservices < 0 ) return(0);

    if ( (fp = fopen(servicefile, "w")) == NULL ) return(0);

    for (i = 0; i < numservices; i++ ) {
        if ( p[i].flag ) 
	    nobytes+=fprintf(fp,"%s@HOST \t%s\n",p[i].service_name,
						  p[i].service_cmd);
    }

    fclose(fp);
    umask(orgmode);
    restartdaemon = 1;
    return(nobytes);
}

int checkService(service_t *s, char *service)
{
    service_t  *p = s;
    int        i  = 0;
    int        numser = numservices;
    int        cmpvalue = 0;

    if ( !s || !service || numser < 0 ) return(-1);

    for ( i = 0; i < numser; i++ ) {
        cmpvalue = strcmp(p[i].service_name, service);
        if ( p[i].flag && cmpvalue == 0 ) {
            return(i);
        }
/*
	if ( p[i].flag && !strcmp(p[i].service_name, service) )
	    return(i);
*/
    }

    return(-1);
}

int deleteService(char *service)
{
    service_t  *s = espservices;
    int        i  = 0;

    if ( (i = checkService(s, service)) < 0 ) return(1);
    s[i].flag = 0;
    writeServices(SERVICESFILE, s);
    return(0);
}

int modService(char *service, char *cmd, int flag)
{
    service_t *s = espservices;
    int       i  = 0;

    if ( !service || !cmd ) return(INVALIDARGS);
    if ( numservices > 0 && !s ) return(INVALIDARGS);

    i = checkService(s, service);

    switch(flag)
    {
	case ADD:
	    if ( i >= 0 ) {
		return(SERVICEALREADYEXISTS);
	    } else if ( strstr(cmd, "HOST") == NULL ) {
		return(INVALIDCOMMAND);
	    } else if ( !addService(&s, service, cmd, numservices+1) ) {
		return(UNABLETOADDSERVICE);
	    } else {
		numservices += 1;
            }
	    break;
        case UPDATE:
	    if ( i < 0 ) {
		return(SERVICENOTFOUND);
	    } else if ( strstr(cmd, "HOST") == NULL ) {
		return(INVALIDCOMMAND);
	    } else {
		strncpy(s[i].service_name,service,MAXSERNAMELEN);
		strncpy(s[i].service_cmd,cmd,MAXSERCMDLEN);
		s[i].flag = 1;
	    }
	    break;
	default:
	    return(INVALIDOPERATION);
    }
    
    writeServices(SERVICESFILE, s);
    return(0);
}
