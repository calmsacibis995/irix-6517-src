#ifndef CF_H
#define CF_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.8 $
 */

#include <cf_sm.h>
#include <cf_q.h>
#include <cf_sanity.h>

extern void skipblank(char**);
extern char **getline(char*, int, FILE*);
extern float getval(char*, char*, char*, int, int, int);
extern int getint(char*, char*, char*, int, int, int);

#endif
