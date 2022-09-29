#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include "subscribe.h"
#include <sys/types.h>
#include <ssdbapi.h>

#define DISPLAY_ONLY                1

int get_subdata( char *orgdata, char *evdata, char **retdata, int flag)
{
    char  *tmpData = evdata;
    char  Pclassdef[2048];
    char  *tmpSubData;
    char  *tmporgdata;
    char  *tmpPtr = NULL;
    int   i = 0;
    int   nobytes=0;
    int   tmpnobytes = 0;

    if ( flag != DISPLAY_ONLY  && !orgdata ) return (0);
    if ( evdata == NULL || retdata == NULL ) return (0);

    /* We got a bunch of event types in orgdata and evdata has a lot more
       event data.  We start searching */

    if ( *tmpData != '%' || *(tmpData+1) != '&' || *(tmpData+2) != '^')
	return(0);
    else
	tmpData += 3;

    while ( *tmpData ) {
	char  class[20];
	char  type[20];
	if ( *tmpData == '%' && *(tmpData+1) == '&' && *(tmpData+2) == '^' ) {
	    /*  We found a class  */
	    Pclassdef[i] = '\0';
	    sscanf(Pclassdef, "%[0-9]|%[0-9]|", class, type);
	    if ( flag == DISPLAY_ONLY ) {
		if ( orgdata ) { 
                    if ( strstr(orgdata, type) == NULL ) 
		        fprintf(stdout, "--> %s\n", Pclassdef);
		} else {
		    fprintf(stdout, "--> %s\n", Pclassdef);
                }
	    } else {
		if ( strstr(orgdata, type) != NULL ) {
		    nobytes += strlen(Pclassdef) + 4;
		}
	    }

	    tmpData += 3;
	    i = 0;
	} else {
	    Pclassdef[i] = *tmpData;
	    tmpData++;
	    i++;
	}
    }

    if ( i > 0 ) {
	char  class[20];
	char  type[20];
	Pclassdef[i] = '\0';
	sscanf(Pclassdef, "%[0-9]|%[0-9]|", class, type);
	if ( flag == DISPLAY_ONLY ) {
	    if (orgdata) {
		if ( strstr(orgdata,type) == NULL ) 
		    fprintf(stdout, "--> %s\n", Pclassdef);
	    } else {
		fprintf(stdout, "--> %s\n", Pclassdef);
	    }
	} else {
	    if ( strstr(orgdata, type) != NULL ) {
		nobytes += strlen(Pclassdef) + 4;
            }
	}
    }

    if ( nobytes == 0 ) return (0);
    else nobytes += 10;

    tmpData = NULL;

    if ( (tmpPtr = (char *) calloc(1,nobytes)) == NULL ) 
	return(0);

    bzero(tmpPtr, nobytes);
    /* Now, start putting data in (*retdata)*/

    tmpData = evdata;
    tmpData += 3;
    i = 0;
    while ( *tmpData ) {
	char class[20];
	char type[20];
	if ( *tmpData == '%' && *(tmpData+1) == '&' && *(tmpData+2) == '^' ) {
	    /*  We found a class  */
	    Pclassdef[i] = '\0';
	    sscanf(Pclassdef, "%[0-9]|%[0-9]|", class, type);
	    if ( strstr(orgdata, type) != NULL ) {
		tmpnobytes += snprintf( tmpPtr+tmpnobytes, nobytes-tmpnobytes,
					"%%&^%s", Pclassdef);
	    }

	    tmpData += 3;
	    i = 0;
	} else {
	    Pclassdef[i] = *tmpData;
	    tmpData++;
	    i++;
	}

    }

    if ( i > 0 ) {
	char  class[20];
	char  type[20];
	Pclassdef[i] = '\0';
	sscanf(Pclassdef, "%[0-9]|%[0-9]|", class, type);
	if ( strstr(orgdata,type) != NULL ) {
	    tmpnobytes += snprintf( tmpPtr+tmpnobytes, nobytes-tmpnobytes,
				    "%%&^%s", Pclassdef);
	}
    }

    snprintf(tmpPtr+tmpnobytes, nobytes-tmpnobytes, "%%&^");
    (*retdata) = tmpPtr;
    return(1);
}

int main (int argc, char **argv)
{
    __uint32_t   err = 0;
    int          nobytes = 0;
    char         data[1024];
    char         *rdata = NULL;
    char         *subdata = NULL;
    char         *rdata1 = NULL;
    int          proc;

    if ( argc <= 2 ) {
	fprintf(stderr, "Usage: %s <-s|-u> <hostname>\n", argv[0]);
	exit(1);
    }

    signal_setup();

    if ( (err = SGMSSDBInit(1)) != 0 ) {
        pSSSErr(SSSERR(CLIENTERR, SSDBERR, err));
        exit(1);
    }

    if ( strcmp(argv[1], "-s") == 0 ) proc = SUBSCRIBEPROC;
    else proc = UNSUBSCRIBEPROC;
    
    printf("Getting Event Descriptions for %s from %s\n", 
           (proc == SUBSCRIBEPROC ? "SUBSCRIBE" : "UNSUBSCRIBE"),
           (proc == SUBSCRIBEPROC ? argv[2] : "localhost"));
    err = SGMIGetData(argv[2], &rdata, &rdata1, proc);
    if ( err ) {
	pSSSErr(err);
	goto end;
    }
    if ( !rdata ) goto end;
    else {
        printf("Select Events from list below : \n");
        get_subdata(rdata1, rdata, &subdata, DISPLAY_ONLY);
    }

    if ( rdata1 ) printf("(Types already subscribed : %s)\n", rdata1);

    printf("Enter Types seperated by commas : ");
    scanf("%s", data);

    if ( !get_subdata(data, rdata, &subdata, !DISPLAY_ONLY) ) {
	printf("Cannot get subscribe data\n");
	goto end;
    }

    printf ("Running subscribe with %s\n", data);
    err = SGMSUBSISubscribe(subdata, argv[2], proc);

    if ( err ) pSSSErr(err);

    if ( !err ) fprintf(stderr, "Operation completed successfully\n");
    end:
    SGMSSDBDone(1);
    if ( subdata ) free(subdata);
    exit(err);

}
