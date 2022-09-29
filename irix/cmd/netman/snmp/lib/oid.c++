/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Object Identifier
 *
 *	$Revision: 1.3 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include "oid.h"


oid::oid(void)
{
    length = 0;
    subid = 0;
}

oid::oid(const char *s)
{
    length = 0;
    subid = 0;
    setValue(s);
}

oid::oid(oid &o)
{
    unsigned int len;
    subID *s;
    o.getValue(&s, &len);

    length = 0;
    subid = 0;
    setValue(s, len);
}

oid::~oid(void)
{
    if (length != 0 && subid != 0)
	delete subid;
}

void
oid::setValue(subID *s, unsigned int len)
{
    if (length != 0 && subid != 0)
	delete subid;
    length = len;
    subid = new subID[len];
    bcopy(s, subid, length * sizeof(subID));
}

void
oid::setValue(const char *s)
{
    char buf[256];
    subID tmp[OID_MAX_LENGTH];

    // If a subid exists that we created, delete it
    if (length != 0 && subid != 0)
	delete subid;

    length = 0;
    strcpy(buf, s);	// Copy string into temporary space

    // Decode the sub-IDs into temporary space and find length
    for (char *sub = strtok(buf, "."); sub != 0; sub = strtok(0, "."))
	tmp[length++] = atoi(sub);

    if (length == 0)
	return;

    // Allocate space and copy sub-IDs in
    subid = new subID[length];
    bcopy(tmp, subid, length * sizeof(subID));
}

char *
oid::getString(char *s)
{
    char buf[256];

    // Use the space passed as an argument if its there
    char *p = (s == 0) ? buf : s;
    for (unsigned int i = 0; i < length; i++, p += strlen(p)) {
	if (i == 0)
	    sprintf(p, "%u", subid[i]);
	else
	    sprintf(p, ".%u", subid[i]);
    }

    if (s == 0) {
	unsigned long tmp = (unsigned long) p - (unsigned long) buf;
	s = new char[tmp + 1];
	strcpy(s, buf);
    }
    return s;
}
