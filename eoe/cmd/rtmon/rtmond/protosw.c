/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1997, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Multi-protocol support.
 */
#include "rtmond.h"

static	protosw_t** table = NULL;
static	int nproto = 0;

void
registerProtocol(protosw_t* sw)
{
    int i;

    for (i = 0; i < nproto; i++)
	if (table[i]->proto == sw->proto) {
	    if (table[i] == sw)
		return;
	    Fatal(NULL, "Attempt to re-register protocol %d", sw->proto);
	}
    if (nproto == 0)
	table = (protosw_t**) malloc(sizeof (protosw_t*));
    else
	table = (protosw_t**) realloc(table, (nproto+1) * sizeof (protosw_t*));
    if (table == NULL)
	Fatal(NULL, "No memory to register protocol %s", sw->name);
    table[nproto++] = sw;
}

protosw_t*
findProtocol(int proto)
{
    int i;

    for (i = 0; i < nproto; i++)
	if (table[i]->proto == proto)
	    return (table[i]);
    return (NULL);
}
