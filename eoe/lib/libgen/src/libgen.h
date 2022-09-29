/*
 * libgen.h
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
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

/*	declarations of functions found in libgen
*/
#ifndef _LIBGEN_H
#define _LIBGEN_H

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.9 $"
#include <standards.h>

/*
 * XPG4-UX functions
 */
extern char *basename(char *);
extern char *dirname(char *);
extern char *regcmp(const char *, ...);
extern char *regex(const char *, const char *, ...);
extern char * __loc1;

#if _SGIAPI || _ABIAPI
#include <sys/types.h>
#include <stdio.h>

extern char * bgets(char *, size_t, FILE *, char *);
extern size_t bufsplit(char *, size_t, char **);
extern char * copylist(const char *, off_t *);
extern int eaccess(const char *, int);

#include	<time.h>

extern int gmatch(const char *, const char *);
extern int isencrypt(const char *, size_t);
extern int mkdirp(const char *, mode_t);
extern int p2open(const char *, FILE *[2]);
extern int p2close(FILE *[2]);
extern char * pathfind(const char *, const char *, const char *);
extern int rmdirp(char *, char *);
extern char * strcadd(char *, const char *);
extern char * strccpy(char *, const char *);
extern char * streadd(char *, const char *, const char *);
extern char * strecpy(char *, const char *, const char *);
extern int strfind(const char *, const char *);
extern char * strrspn(const char *, const char *);
extern char * strtrns(const char *, const char *, const char *, char *);
#endif /* _SGIAPI || _ABIAPI */

#ifdef __cplusplus
}
#endif

#endif /* _LIBGEN_H */
