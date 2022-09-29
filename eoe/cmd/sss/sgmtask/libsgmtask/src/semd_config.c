/*
 * C Interface to reconfigure the semd/sgmd daemon.
 */
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <common.h>
#include <seh_archive.h>
#include <sgmdefs.h>
#include <eventmonapi.h>
#include <strings.h>

#define SEMD_CONFIG_PTHREADS 1

static int semd_config(int flag,char *pStr)
{
    char *sendmsg=NULL;
    int length,c;
    char buffer[1024],respbuffer[1024];
    int i=0;

    bzero(buffer,1024);

    switch(flag)
    {
    case(RULE_CONFIG) :
	{
	    if(!pStr)
		return -1;
	    strcpy(buffer,RULE_CONFIG_STRING);
	    strcat(buffer," ");
	    strcat(buffer,pStr);
	    break;
	}
    case(EVENT_CONFIG) :
	{
	    if(!pStr)
		return -1;
	    strcpy(buffer,EVENT_CONFIG_STRING);
	    strcat(buffer," ");
	    strcat(buffer,pStr);
	    break;
	}
    case(GLOBAL_CONFIG) :
	{
	    if(!pStr)
		return -1;
	    strcpy(buffer,GLOBAL_CONFIG_STRING);
	    strcat(buffer," ");
	    strcat(buffer,pStr);
	    break;
	}
#if SGM
    case(SGM_OPS) :
	{
	    if(!pStr)
		return -1;
	    strcpy(buffer,SGM_CONFIG_STRING);
	    strcat(buffer," ");
	    strcat(buffer,pStr);
	    break;
	}
#endif
    default:
	{
	    free(sendmsg);
	    return -1;
	}
    }

    length=strlen(buffer);
    c=1024;

    if(emapiSendMessage(buffer,length,respbuffer,&c) == 0)
    {
	goto LOCAL_ERROR;
    }

    return 0;

LOCAL_ERROR:
    
    return -1;
}

int configure_event(char *pStr)
{
    return semd_config(EVENT_CONFIG,pStr);
}

int configure_rule(char *pStr)
{
    return semd_config(RULE_CONFIG,pStr);
}

int configure_global(char *pStr)
{
    return semd_config(GLOBAL_CONFIG,pStr);
}

#if SGM
int configure_sgm(char *pStr)
{
    return semd_config(SGM_OPS,pStr);
}
#endif

#define TEST 0

#if TEST
int main()
{
    /* 
     * Class = 1 Type =2 sehthrottle=3 sehfrequency = 4 throttled flag = 1
     */
    configure_event("UPDATE localhost 1 2 3 4 1");
    configure_event("DELETE localhost 1 2");
    configure_event("NEW localhost 1 2 3 4 1");

    /* 
     * Rule configuration, aciton_id = 1
     */
    configure_rule("UPDATE 1");
    configure_rule("DELETE 1");
    configure_rule("NEW 1");

    /* 
     * Glbal configuration, throttle = 1, logging = 1, action = 1
     */
    configure_global("1 1 1");

#if SGM
    /* 
     * Sgm configuration, Mode change...
     */
    configure_sgm("MODE 1");
    /* 
     * Sgm configuration subscribe & unsubscribe 12345.
     */
    configure_sgm("SUBSCRIBE 12345");
    configure_sgm("UNSUBSCRIBE 12345");
#endif
    return 0;
}
#endif
