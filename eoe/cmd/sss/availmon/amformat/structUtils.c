/*--------------------------------------------------------------------*/
/*                                                                    */
/* Copyright 1992-1998 Silicon Graphics, Inc.                         */
/* All Rights Reserved.                                               */
/*                                                                    */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics    */
/* Inc.; the contents of this file may not be disclosed to third      */
/* parties, copied or duplicated in any form, in whole or in part,    */
/* without the prior written permission of Silicon Graphics, Inc.     */
/*                                                                    */
/* RESTRICTED RIGHTS LEGEND:                                          */
/* Use, duplication or disclosure by the Government is subject to     */
/* restrictions as set forth in subdivision (c)(1)(ii) of the Rights  */
/* in Technical Data and Computer Software clause at                  */
/* DFARS 252.227-7013, and/or in similar or successor clauses in      */
/* the FAR, DOD or NASA FAR Supplement.  Unpublished - rights         */
/* reserved under the Copyright Laws of the United States.            */
/*                                                                    */
/*--------------------------------------------------------------------*/

#include  <amstructs.h>

void initEvent(Event_t *);
void freeEvent(Event_t **);
void initConfig(amConfig_t *);
void freerepparams(repparams_t *);
void freeConfig(amConfig_t *);
repparams_t *newrepparams(int);
emailaddrlist_t *newaddrnode(char *);
void freeaddrlist(emailaddrlist_t *);
int insertaddr(repparams_t *, emailaddrlist_t *);
/*--------------------------------------------------------------------*/
/* initEvent function                                                 */
/*--------------------------------------------------------------------*/

void initEvent(Event_t *event) {
    if ( event == NULL ) return;

    event->sys_id        = 0;
    event->serialnum     = NULL;
    event->hostname      = NULL;
    event->eventcode     = 0;
    event->eventtime     = 0;
    event->lasttick      = 0;
    event->prevstart     = 0;
    event->starttime     = 0;
    event->statusinterval= 0;
    event->bounds        = 0;
    event->diagtype      = NULL;
    event->diagfile      = NULL;
    event->syslog        = NULL;
    event->hinvchange    = 0;
    event->verchange     = 0;
    event->metrics       = 0;
    event->flag          = 0;
    event->summary       = NULL;
    event->shutdowncomment=NULL;
    event->notifyflags   = 0;
}

/*--------------------------------------------------------------------*/
/* freeEvent function                                                 */
/*--------------------------------------------------------------------*/

void freeEvent(Event_t **event)
{
    if ( *event == NULL ) return;

    if ( (*event)->serialnum != NULL) free( (*event)->serialnum);
    if ( (*event)->hostname != NULL) free( (*event)->hostname);

    if ( (*event)->diagtype != NULL ) {
	free( (*event)->diagtype );
    }

    if ( (*event)->diagfile != NULL ) {
	free( (*event)->diagtype );
    }

    if ( (*event)->syslog != NULL ) {
	free( (*event)->syslog );
    }

    if ( (*event)->summary != NULL ) {
	free( (*event)->summary );
    }

    if ( (*event)->shutdowncomment != NULL ) {
	free( (*event)->shutdowncomment );
    }

    free( (*event) );
    (*event) = NULL;
}

/*--------------------------------------------------------------------*/
/* initConfig                                                         */
/*--------------------------------------------------------------------*/

void initConfig(amConfig_t *Config)
{
    int   i = 0;

    if ( Config == NULL ) return;

    Config->configFlag     = 0;
    Config->statusinterval = 0;
    for (i = 0; i < MAXTYPES; i++ ) 
	Config->notifyParams[i] = NULL;
    return;
}

/*--------------------------------------------------------------------*/
/* freerepTypes                                                       */
/*--------------------------------------------------------------------*/

void freerepparams ( repparams_t *notifyparams )
{
    if ( notifyparams == NULL ) return;

    if ( notifyparams->filename ) free(notifyparams->filename);
    if ( notifyparams->addrlist ) freeaddrlist(notifyparams->addrlist);
}

/*--------------------------------------------------------------------*/
/* freeConfig                                                         */
/*--------------------------------------------------------------------*/

void freeConfig ( amConfig_t *Config )
{
    int  i = 0;
    if ( Config == NULL ) return;

    for ( i = 0; i < MAXTYPES; i++ ) {
	freerepparams ( Config->notifyParams[i] );
    }

    return;
}


/*--------------------------------------------------------------------*/
/* newrepparams                                                       */
/*--------------------------------------------------------------------*/

repparams_t *newrepparams(int repType)
{
    repparams_t  *tmprepparams = NULL;
    int          i = 0;

    if ( repType == 0 ) return (NULL);

    if ( (tmprepparams = (repparams_t *) malloc(sizeof(repparams_t)))
	 != NULL ) {

	tmprepparams->reportType = repType;

	for ( i = 0; i < 4; i++ ) 
	    tmprepparams->subject[i] = '\0';

	tmprepparams->filename = NULL;

	tmprepparams->addrlist   = NULL;
    }

    return(tmprepparams);
}

/*--------------------------------------------------------------------*/
/* newaddrnode                                                        */
/*--------------------------------------------------------------------*/

emailaddrlist_t  *newaddrnode(char *emailaddr)
{
    emailaddrlist_t  *tmpemailaddr = NULL;

    if ( emailaddr != NULL ) 
    {
	if ( (tmpemailaddr = 
		(emailaddrlist_t *) malloc(sizeof(emailaddrlist_t)))
		!= NULL ) {
	    tmpemailaddr->address = strdup(emailaddr);
	    tmpemailaddr->next    = NULL;
        }
    }

    return(tmpemailaddr);
}

void freeaddrlist(emailaddrlist_t *addrlist ) 
{
    emailaddrlist_t  *tmpemailaddrlist1 = NULL;
    emailaddrlist_t  *tmpemailaddrlist2 = NULL;

    if ( addrlist == NULL ) return;

    tmpemailaddrlist1 = addrlist;

    while ( tmpemailaddrlist1 != NULL ) {
	tmpemailaddrlist2 = tmpemailaddrlist1->next;
	free(tmpemailaddrlist1->address);
	free(tmpemailaddrlist1);
	tmpemailaddrlist1 = tmpemailaddrlist2;
    }

    return;
}
/*--------------------------------------------------------------------*/
/* insertaddr                                                         */
/*--------------------------------------------------------------------*/

int insertaddr(repparams_t *Rep, emailaddrlist_t *node)
{
    emailaddrlist_t  *tmpaddrlist = NULL;

    if ( Rep == NULL ) return(-1);
    if ( node == NULL ) return(-1);


    tmpaddrlist = Rep->addrlist;

    if ( tmpaddrlist == NULL ) {
	Rep->addrlist = node;
    } else {
	while ( tmpaddrlist->next != NULL ) 
	    tmpaddrlist = tmpaddrlist->next;
        tmpaddrlist->next = node;
    }

    return(0);
}
