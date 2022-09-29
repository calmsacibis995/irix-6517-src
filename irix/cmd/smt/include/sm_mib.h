#ifndef SM_MIB_H
#define SM_MIB_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.6 $
 */

#include <smt_asn1.h>
#include <smt_parse.h>

extern int underfdditree(oid *);
extern int undersgifdditree(oid *);
extern struct tree *init_fddimib(char *);
extern struct tree *init_sgifddimib(char *);
extern int get_fddimib(char *);
extern int get_sgifddimib(char *);

#define OIDLEN(x) (sizeof(x)/sizeof(oid))

#endif
